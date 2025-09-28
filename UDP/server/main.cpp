#include <iostream>
#include <config.hpp>
#include <socket.hpp>

int main(int argc, char **argv)
{
    try
    {
        Config conf = load_config_from_args(argc, argv);

        auto socket = create_socket();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}