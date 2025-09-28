#include <iostream>
#include <config.hpp>
#include <socket.hpp>
#include <arpa/inet.h>

int main(int argc, char **argv)
{
    try
    {
        Config conf = load_config_from_args(argc, argv);

        auto sockfd = create_socket();
        auto server_addr = create_server_addr(conf["port"].get<uint16_t>());
        bind_socket(sockfd, (sockaddr *)&server_addr);
        std::cout << "server is ready on port: " << conf["port"].get<uint16_t>() << '\n';
        close(sockfd);

        sockaddr_in client_addr;
        std::vector<char> buffer;
        buffer.resize(1024);
        auto bytes = get_message(sockfd, buffer.data(), buffer.size() - 1, client_addr);
        buffer[bytes] = '\0';

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "from " << client_ip << ":" << ntohs(client_addr.sin_port) << "\n";
        std::cout << "message: " << buffer.data() << std::endl;
        close(sockfd);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}