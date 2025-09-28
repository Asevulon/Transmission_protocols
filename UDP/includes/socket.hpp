#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdexcept>

inline int create_socket()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1)
        throw std::runtime_error("Socket is not created");
    return sockfd;
}
