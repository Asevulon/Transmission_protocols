#pragma once

#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

typedef nlohmann::json Config;
inline Config load_config(const std::string &path)
{
    std::ifstream file(path);
    Config conf;
    file >> conf;
    return conf;
}

inline Config load_config_from_args(int argc, char **argv)
{
    if (argc != 2)
        throw std::runtime_error("invalid arguments");
    return load_config(argv[1]);
}