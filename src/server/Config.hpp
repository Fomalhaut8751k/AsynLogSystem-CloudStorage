#ifndef CONFIG_H
#define CONFIG_H

#include "json.hpp"


class Config
{
private:
    Config();
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

public:
    static Config& GetInstance()
    {
        static Config config;
        return config;
    }

    void ReadConfig()
    {

    }

};




#endif