#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h> // For inet_pton on Linux/macOS

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

inline void bind_socket(int sockfd, const sockaddr_in &addr)
{
    if (bind(sockfd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        close(sockfd);
        throw std::runtime_error("Bind failed: " + std::string(strerror(errno)));
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
    return res;
}

// Send message function
inline void send_message(int sockfd, const void *buffer, size_t size,
                         const sockaddr_in &dest_addr)
{
    auto res = sendto(sockfd, buffer, size, 0,
                      reinterpret_cast<const sockaddr *>(&dest_addr),
                      sizeof(dest_addr));
    if (res == -1)
    {
        throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
    }
}

inline void send_message(int sockfd, const std::string &message,
                         const sockaddr_in &dest_addr)
{
    send_message(sockfd, message.data(), message.size(), dest_addr);
}

// Create client address function
inline auto create_client_addr(const std::string &ip, uint16_t port)
{
    sockaddr_in client_addr;
    std::memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &client_addr.sin_addr) <= 0)
    {
        throw std::runtime_error("Invalid IP address: " + ip);
    }

    return client_addr;
}

inline auto set_timeout(int sockfd, int sec)
{
    timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = 0;
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}