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
