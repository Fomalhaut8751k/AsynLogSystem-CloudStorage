# 路由

简单来说，路由就是`URL`到函数的映射。
```css
/user        ->   getAllUsers()
/user/count  ->   getUsersCount()
```

当访问`/user`时就会执行`getAllUsers()`

当访问`/user/count`时就会执行`getAllUsers()`


## 注册路由
把`key`(请求方法+请求路径)和`value`(请求回调)注册到哈希表上。

不仅仅是`URL`在router匹配route的过程中，不仅会根据URL来匹配，还会根据请求的方法来看是否匹配。

- 静态路由注册和调用
    
    url中的路径完全匹配注册的路径时才会执行相应的回调函数。
    ```cpp
    RouteKey key{method, path};  // 方法和URL
    handlers_[key] = handler; 
    ```

    ```cpp
    RouteKey key{req.method(), req.path()};

    // 查找处理器
    auto handleIt = handlers_.find(key);
    if(handleIt != handlers_.end())
    {   // HandlerPtr::handle()方法
        handleIt->second->handle(req, resp);
        return true;
    }
    ```

- 动态路由注册和调用

    根据正则匹配实现

    - 正则表达式`regex`

        转化函数：
        ```cpp
        std::regex Router::convertToRegex(const std::string &pathPattern)
        {   
            std::string regexPattern = "^" + std::regex_replace(pathPattern, std::regex(R"(/:([^/]+))"), R"(/([^/]+))") + "$";
            return std::regex(regexPattern);
        }

        // 路由模式: /users/:userId -> 正则表达式: ^/users/([^/]+)$

        /*
        上述正则表达式各个组成的含义：
        ^   /users/   ([^/]+)   $
        │     │          │      │
        │     │          │      └─ 字符串结束
        │     │          └─ 捕获组：匹配1个或多个非斜杠字符
        │     └─ 固定文本 "/users/"
        └─ 字符串开始
        */
        ```

    - 注册路由

        ```cpp
        void Router::addRegexHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
        {
            std::regex pathRegex = convertToRegex(path);
            regexHandlers_.emplace_back(method, pathRegex, handler);
        }

        ```
        注册的时候，会将路由模式比如`/users/:userId`，作为参数`path`传入，通过`convertToRegex(path)`，转化为对应的正则表达式`^/users/([^/]+)$`，并记录在`regexHandlers_`中。

    - 匹配路由

        ```cpp
        for(const auto &[method, pathRegex, callback]: regexCallbacks_){
            std::smatch match;
            std::string pathStr(req.path());
            // 如果方法匹配并且动态路由匹配，则执行处理器
            if(method == req.method() && std::regex_match(pathStr, match, pathRegex))
            {
                HttpRequest newReq(req);
                extractPathParameters(match, newReq);

                callback(newReq, resp);
                return true;
            }
        }
        ```
        匹配的时候，调用函数`std::regex_match(pathStr, match, pathRegex)`, 如果匹配`^/users/([^/]+)$`，就会返回`true`：

        /users/123 -> ✓ 匹配

        /users/john_doe -> ✓ 匹配

        /users/123/profile -> ✗ 不匹配  （多了一个斜杠）

        /user/123 -> ✗ 不匹配  （固定文本不匹配）

        /users/ -> ✗ 不匹配  （非斜杠字符少于1）

