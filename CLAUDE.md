# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AsynLogSystem-CloudStorage is a cloud storage system with an asynchronous logging framework built on libevent/muduo and C++17. The system consists of three main components:

1. **Asynchronous Log System** (`log_system/`) - High-performance async logging with producer-consumer pattern, achieving 185+ MB/s throughput
2. **HTTP Server** (`HttpServer/`) - HTTP server with middleware, routing, session management, SSL/TLS support, built on muduo network library
3. **Cloud Storage Service** (`src/server/`) - File upload/download/delete service with web interface

## Build System

```bash
# Build entire project
mkdir -p build && cd build
cmake ..
make

# Executables output to: bin/
# Libraries output to: lib/
```

### Build Structure
- Root `CMakeLists.txt` configures global settings (C++17, output paths, dependencies)
- `log_system/` - Header-only INTERFACE library
- `HttpServer/` - STATIC library (depends on OpenSSL, muduo, log_system)
- `src/server/` - Executable `Service` (cloud storage server)
- `src/client/` - Client test executable
- `src/superclient/` - SuperClient for reconnection management

### Key Dependencies
- **muduo** (mymuduo) - Network library for HTTP server
- **OpenSSL** - SSL/TLS support
- **libevent** - Event-driven I/O (used in backup log clients)
- **jsoncpp** - JSON parsing
- **Threads** (pthread) - Multi-threading

## Architecture

### 1. Asynchronous Log System (`log_system/include/`)

**Core Design**: Producer-consumer pattern with log queue (replaced double-buffer for better performance)

**Key Components**:
- `AsyncWorkerWithLogQueue.hpp` - Main async worker using lock-free-ish log queue
- `Message.hpp` - Log formatting with thread_local timestamp caching (major perf optimization)
- `Manager.hpp` - Singleton logger manager
- `ThreadPool.hpp` - Thread pool for remote log backup (connects to backup server for ERROR/FATAL logs)
- `backlog/ClientBackupLog.hpp` - libevent-based client for sending logs to remote server
- `backlog/ServerBackupLog.hpp` - libevent-based server for receiving backup logs

**Performance Optimizations**:
- Thread-local timestamp caching (avoids repeated `localtime()` calls)
- `snprintf` with stack buffer instead of `ostringstream` (eliminates heap allocations)
- Log queue with node pool to reduce allocation overhead
- Dynamic buffer expansion (soft/hard modes) with automatic shrinking
- Batch disk writes (notify threshold at 5/6 of max buffer size)
- Disk space monitoring (pauses logging if < 4GB available)

**Configuration**: `LogSystemConfig.hpp` loads from config file:
```
THREADPOOL:
    INIT_THREADSIZE 4
    THREAD_SIZE_THRESHHOLD 4
    LOGQUE_MAX_THRESHHOLD 4

ASYNCBUFFER:
    INIT_BUFFER_SIZE 8388608  # 8MB initial, max 128MB

ASYNCWORKER:
    EFFECTIVE_EXPANSION_TIMES 4
```

**Usage Pattern**:
```cpp
mylog::GetLogger("asynclogger")->Info("message");
mylog::GetLogger("asynclogger")->Error("error");  // Triggers remote backup
```

### 2. HTTP Server (`HttpServer/`)

Built on **muduo** network library (replaced libevent for better large file handling).

**Key Features**:
- Static and dynamic routing (regex-based path parameters like `/download/:filename`)
- Middleware chain (CORS, authentication, etc.)
- Session management with pluggable storage backends
- SSL/TLS support
- Chunked transfer for large files (handles files > available RAM)
- Database connection pooling (`utils/db/`)

**Important Files**:
- `http/HttpServer.h` - Main server class
- `http/HttpContext.h` - HTTP parsing state machine (handles chunked uploads/downloads)
- `router/Router.h` - Route registration and matching
- `middleware/MiddlewareChain.h` - Middleware execution pipeline
- `session/SessionManager.h` - Session lifecycle management

**Large File Handling**:
- **Upload**: Parses `Content-Length` from headers before reading body. If > 256MB, reads body in chunks from socket buffer and writes incrementally to disk (avoids loading entire file into memory)
- **Download**: Uses `writeCompleteCallback_` to send file in chunks as socket buffer becomes writable (prevents memory exhaustion)

**Routing**:
- Static: Exact path match (`/api/users`)
- Dynamic: Regex patterns (`/download/:filename` → `/download/([^/]+)`)

### 3. Cloud Storage Service (`src/server/`)

**Main Files**:
- `StorageServer.hpp` - HTTP server setup with route handlers
- `StorageDataManager.hpp` - File metadata management (persists to `storage.data` JSON file)
- `StorageUtils.hpp` - File operations utilities
- `Service.hpp` - Terminal-based client for headless servers
- `index.html` - Web UI for upload/download/delete

**Storage Structure**:
- `low_storage/` - Frequently accessed files
- `deep_storage/` - Archived files
- `storage.data` - JSON metadata (filename, size, timestamps, paths)

**API Endpoints**:
- `POST /upload` - File upload (supports chunked for large files)
- `GET /download/:filename` - File download (supports chunked)
- `DELETE /delete/:filename` - File deletion
- `GET /list` - List all files

**Configuration**: `Storage.conf`
```
SERVER_IP 0.0.0.0
SERVER_PORT 8000
DOWNLOAD_PREFIX /download/
LOW_STORAGE_PATH ./low_storage
DEEP_STORAGE_PATH ./deep_storage
STORAGE_FILE ./storage.data
```

## Critical Implementation Details

### Log System Synchronization

**Producer-Consumer Coordination** (see README.md problem #2, #3, #10):
- `label_data_ready_` - Producer has data to swap
- `label_consumer_ready_` - Consumer finished processing (set to `true` AFTER processing, not before)
- Incorrect ordering causes deadlocks or lost logs

**Graceful Shutdown** (`~AsyncWorker()`):
1. Set `ProhibitSubmitLabel_ = true` to block new submissions
2. Wait for `user_current_count_` to reach 0 (all in-flight submissions complete)
3. Notify producer/consumer to exit

### Thread Pool Reconnection Logic

**Problem**: If remote backup server goes down, thread pool threads must detect disconnection and allow reconnection (see README.md problem #14, #15).

**Solution**:
- Each thread's `Client` monitors event loop exit status
- If event loop exits abnormally (`ret == 1`), mark `clientactive_ = false`
- `SuperClient` can trigger thread pool restart after detecting server is back online
- Threads exit gracefully when `Client` disconnects

### HTTP Chunked Transfer

**Why**: muduo's `onMessage()` callback receives partial request body chunks. For large files, waiting for complete body causes memory exhaustion.

**Implementation**:
1. Parse request headers first to get `Content-Length`
2. If `Content-Length > 256MB`, switch to chunked mode
3. Read available bytes from buffer, write to file immediately, repeat
4. For downloads, use `conn->setWriteCompleteCallback()` to send next chunk when socket buffer is writable

### UTF-8 Handling in Chunked Transfers

**Issue**: Chinese characters (3 bytes in UTF-8) may be split across chunks, causing corruption (see README.md problem #18).

**Solution**: Use actual bytes read (`buf->readableBytes()`) instead of `Content-Length` when writing chunks. Incomplete UTF-8 sequences are completed in subsequent chunks.

## Common Development Tasks

### Adding a New Route

```cpp
// In StorageServer.hpp
router->addHandler(HttpRequest::Method::POST, "/your-path", 
    [](const HttpRequest& req, HttpResponse& resp) {
        // Handle request
    });
```

### Adding Middleware

```cpp
// Create middleware class inheriting from Middleware
// Register in HttpServer before routes
server.use(std::make_shared<YourMiddleware>());
```

### Configuring Log Levels

```cpp
// In AsyncLogger builder
builder.SetLevel(mylog::LogLevel::INFO);  // Only INFO and above
```

### Testing Log Performance

```cpp
auto start = std::chrono::steady_clock::now();
while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
    mylog::GetLogger("asynclogger")->Info("test message");
}
// Check log file size to calculate throughput
```

## Known Issues & Workarounds

1. **Event loop exits prematurely** (README.md #4): Add 100ms sleep before `event_base_dispatch()` to ensure events are registered
2. **Repeated `evthread_use_pthreads()` assertion** (README.md #14): Call once in ThreadPool constructor, not per Client
3. **Large file OOM**: Use chunked transfer, never load entire file into string
4. **Log loss on shutdown**: Ensure `~AsyncWorker()` waits for in-flight submissions before exiting

## Performance Benchmarks

**Async Log System** (single thread, 150B logs):
- Throughput: 185.94 MB/s (after optimizations)
- QPS: ~194K logs/s
- Latency: P50=3.5μs, P95=15.8μs, P99=442μs

**Cloud Storage** (single request):
- Download speed: 11 MB/s
- 5 concurrent downloads: ~2.2 MB/s each

## Network Library Migration Notes

**libevent → muduo** (completed 2026-04-12):
- Reason: libevent's `evhttp` loads entire request body into memory before callback, causing OOM for large files
- muduo's `onMessage()` provides incremental buffer access, enabling true streaming
- Migration affected: `HttpServer`, `HttpContext`, `HttpRequest`, `HttpResponse`
- Backup log system still uses libevent (no large data transfers)
