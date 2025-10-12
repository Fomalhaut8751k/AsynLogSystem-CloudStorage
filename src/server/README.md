# 1. DataManager.hpp 
## class DataManager
- 封装了一个结构体`struct StorageInfo`，用于记录文件的信息，包括：
    ```cpp
    time_t atime_;        // 文件最后一次访问时间
    time_t mtime_;        // 文件最后一次修改时间
    int64_t fsize_;              // 文件大小
    std::string storage_path_;          // 文件的存储路径
    std::string url_;                // url下载地址

    /*  创建新的StorageInfo 
        输入存储的路径，判断该路径是否存在文件，如果存在
        获取该文件的大小，路径，最近访问时间，最近修改时间等等
        写入StorageInfo对象的成员变量中:

        atime_ = fu.LastAccessTime();
        mtime_ = fu.LastModifyTime();
        fsize_ = fu.FileSize();
        storage_path_ = storage_path;
        url_ = config.GetDownLoadPrefix() + fu.FileName();
    */
    int NewStorageInfo(const std::string &storage_path); 
    ```

- 在`DataManager`中，有一个成员变量用于存储这些`StorageInfo`:
    ```cpp
    std::unordered_map<std::string, StorageInfo> table_;
    ```
    另外，成员变量`std::string storage_file_;`，一般就是`storage.dat`（包含存储信息的json串数据）的地址，初始化是会从中把存储信息加载到`table_`当中。

- 将`table_`中的`StorageInfo`持久化到`storage.dat`
    