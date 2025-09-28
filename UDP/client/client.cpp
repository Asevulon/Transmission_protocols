#include <iostream>
#include <config.hpp>

int main(int argc, char **argv)
{
    if (argc != 2)
        throw std::runtime_error("invalid arguments");

    auto config = load_config(argv[1]);
    std::cout << config << std::endl;
    return 0;
}