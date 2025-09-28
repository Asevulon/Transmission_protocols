#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdexcept>
#include <cstring>
#include <unistd.h>

inline int create_socket()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1)
        throw std::runtime_error("Socket is not created");
    return sockfd;
}

inline auto create_server_addr(uint16_t port)
{
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return server_addr;
}

inline void bind_socket(int socket, sockaddr *server_addr)
{
    if (bind(socket, server_addr, sizeof(sockaddr)) == -1)
    {
        close(socket);
        throw std::runtime_error("Socket is not bint");
    }
}

inline auto get_message(
    int sockfd,
    void *buffer,
    size_t size,
    sockaddr_in &client_addr)
{
    socklen_t client_len = sizeof(client_addr);
    auto res = recvfrom(sockfd, buffer, size, 0, (sockaddr *)&client_addr, &client_len);
    if (res == -1)
    {
        close(sockfd);
        throw("Receive failed");
    }
    return res;
}