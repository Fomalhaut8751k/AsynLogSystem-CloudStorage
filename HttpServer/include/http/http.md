# HTTP通信协议

## HTTP请求报文
HTTP请求报文格式如下，由请求行，请求头，空行，以及请求体(可选)组成，是客户端发送给服务器的消息，用于请求特定资源或执行特定操作。

```css
请求行   | POST /api/users HTTP/1.1
 
        | Host: example.com
        | User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)
        | Content-Type: application/json
请求头   | Authorization: Bearer abc123xyz
        | Content-Length: 56
        | Accept: application/json
        | Connection: keep-alive

空行     |

请求体   | {"name": "John Doe", "email": "john@example.com", "age": 30}
```

### 1. 请求行
```css
【方法】 URT 【版本】CRLF
``` 
- 方法：`GET`, `HEAD`, `POST`, `PUT`, `DELETE`等
    
    `GET`: 获取资源，从服务器段获取(读取/只读)资源，一般没有请求体
    
    ```
    GET /index.html HTTP/1.1
    ```
    
    `HEAD`: 只获取资源的响应头，不获取响应体，用于检测资源是否存在，获取元数据，没有请求体

    ```
    HEAD /file.txt HTTP/1.1
    ```

    `POST`: 提交数据，向服务器提交数据，有请求体(需要检查Content-Length),常用于表单提交，文件上传

    ```
    POST /submit-form HTTP/1.1
    Content-Type: application/json
    Content-Length: 56

    {"username": "john", "password": "secret"}
    ```

    `PUT`: 更新或创建资源，有请求体，幂等操作(多次执行结果相同)
    
    ```
    PUT /user/123 HTTP/1.1
    Content-Type: application/json
    Content-Length: 48

    {"name": "John Doe", "email": "john@example.com"} 
    ```

    `DELETE`: 删除资源，一般没有请求体，幂等操作

    ```
    DELETE /user/123 HTTP/1.1
    ```

- URI: 统一的资源标识符(如/index.html) 

- 版本： `HTTP/1.0`, `HTTP/1.1`

- CRLF: `\r\n`  回车换行


### 2. 请求头
```css
Host: example.com                                         # 必须
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)     # 客户端信息
Accept: text/html, application/json                       # 可接受的响应类型
Content-Type: application/json                            # 请求字体的MIME类型
Authorization: Bearer abc123xyz                           # 认证信息
Content-Length: 56                                        
Connection: keep-alive                                    # 连接管理
```


### 3. 空行
```css
CRLF
```
    
只有`\r\n`表示请求头的结束。

### 4. 请求体
可选部分，常用于`POST`, `PUT`，格式由Content-Type指定
```
application/x-www-form-urlencoded  # 表单数据： name=value&age=20
application/json                   # JSON数据
multipart/form-data                # 文本上传
text/plain                         # 纯文本
```
<br>

## HTTP响应报文

与HTTP请求报文类似，HTTP响应报文由状态行，响应头，空行和响应体组成

```css
状态行   | HTTP/1.1 200 OK\r\n
        | Content-Type: text/html\r\n
响应头   | Content-Length: 123\r\n
        | Connection: close\r\n
空的行   | \r\n
响应体   | <html>...</html>
```
