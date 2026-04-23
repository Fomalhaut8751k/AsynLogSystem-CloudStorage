---
name: project_reader
description: Use this skill whenever the user wants to initialize or analyze a C++ project repository. This includes scanning project structure, generating CLAUDE.md summaries, understanding modules, build systems, and architecture. When running /init, follow directory filtering rules and ignore non-essential assets like images.
license: Proprietary. LICENSE.txt has complete terms
---

# C++ Project Reading Guide

## Overview

This guide defines how to analyze a C++ project repository during /init and generate a high-quality CLAUDE.md.

Focus on:
- Build system (VERY IMPORTANT)
- Module structure (.h / .cpp relationships)
- Entry points
- Dependency graph

---

## Initialization Rules (/init)

### 1. Directory Filtering (IMPORTANT)

Ignore completely:

- img
- imgs

Do NOT:
- Traverse
- Read files
- Include in directory tree

---

### 2. Files to Prioritize

Core Source Files:
- .cpp
- .cc
- .c
- .h
- .hpp

Build System (CRITICAL):
- CMakeLists.txt
- Makefile
- .cmake
- build.sh

Project Metadata:
- README.md
- docs/

---

### 3. Files to Deprioritize

- build/, cmake-build-*
- .git/
- __pycache__/
- logs / binaries (.out, .exe)

---

## C++-Specific Analysis Strategy

### Step 1: Build System (FIRST PRIORITY)

Identify:
- Whether using CMake / Makefile / Bazel
- Main targets:
  - executable
  - static/shared library

From build files, extract:
- add_executable
- add_library
- target_link_libraries

This step determines overall project structure.

---

### Step 2: Entry Point

Locate:

int main(int argc, char** argv)

Then trace:
- main → controller / manager → modules

---

### Step 3: Module Structure

Analyze .h and .cpp pairs:

For each module:
- Responsibility
- Public interface (.h)
- Implementation (.cpp)

Focus on:
- Class design
- namespace usage
- Interface vs implementation separation

---

### Step 4: Include Dependency

Analyze:

#include "xxx.h"

Build:
- Module dependency relationships
- Detect potential circular dependencies

---

### Step 5: Execution Flow

Describe:
- Program startup flow
- Core call chain
- Key data flow

---

### Step 6: Key Components

Identify:
- Core classes
- Utility classes
- Manager / Engine classes
- Important data structures

---

## CLAUDE.md Generation

The generated CLAUDE.md should include:

1. Project Overview
- What the project does
- Use cases

2. Build System
- Toolchain used
- How to build and run

3. Directory Structure
- Clean structure (excluding img/imgs)

4. Core Modules
For each module:
- Name
- Responsibility
- Key classes

5. Execution Flow
- main → module interactions

6. Dependency Summary
- High-level module relationships

---

## Example Filtering Logic

def should_ignore_dir(dirname):
    return dirname.lower() in ["img", "imgs"]

for root, dirs, files in os.walk(project_path):
    dirs[:] = [d for d in dirs if not should_ignore_dir(d)]

---

## C++ Reading Tips

- Read .h files before .cpp files
- Start from main() and trace downward
- Focus on architecture before details
- Pay attention to ownership and memory management (raw pointers vs smart pointers)
- Watch for design patterns (singleton, factory, etc.)

---

## Notes

- Prioritize understanding over completeness
- Avoid wasting context on non-code assets
- Keep summaries structured and concise
- Emphasize architecture clarity

---

## Next Steps

- Run /init using this guide
- Generate CLAUDE.md
- Refine based on project complexity

