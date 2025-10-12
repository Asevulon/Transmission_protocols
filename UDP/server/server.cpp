#include <iostream>
#include <config.hpp>
#include <socket.hpp>
#include <arpa/inet.h>
#include <csignal>

int sockfd = -1;
void clean_socket()
{
    if (sockfd != -1)
        close(sockfd);
    std::cout << "\nSocked was cleaned\n";
}
void sigint_handler(int s) { exit(0); }

int main(int argc, char **argv)
{
    try
    {
        atexit(clean_socket);
        std::signal(SIGINT, sigint_handler);

        Config conf = load_config_from_args(argc, argv);

        sockfd = create_socket();
        auto server_addr = create_server_addr(conf["port"].get<uint16_t>());
        bind_socket(sockfd, server_addr);
        std::cout << "server is ready on port: " << conf["port"].get<uint16_t>() << '\n';

        sockaddr_in client_addr;
        std::vector<char> buffer;
        buffer.resize(1024);
        while (true)
        {
            auto bytes = get_message(sockfd, buffer.data(), buffer.size() - 1, client_addr);
            buffer[bytes] = '\0';

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "from " << client_ip << ":" << ntohs(client_addr.sin_port) << "\n";
            std::cout << "message: " << buffer.data() << std::endl;
            std::string reply;
            std::cin >> reply;
            send_message(sockfd, reply, client_addr);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}