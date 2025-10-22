#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <csignal>
#include <atomic>
#include <poll.h>
#include <fcntl.h>

using namespace std;

atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
    cout << "\nЗавершение работы сервера...\n";
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    // Простая конфигурация
    uint16_t port = 12345;
    size_t buffer_size = 1024;

    int server_fd;
    sockaddr_in address;
    int addrlen = sizeof(address);

    // Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return -1;
    }

    // Разрешить повторное использование адреса
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server_fd);
        return -1;
    }

    // Привязка сокета к порту
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }

    // Прослушивание порта
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        return -1;
    }

    cout << "Сервер запущен на порту " << port << endl;
    cout << "Ожидание подключений... (Ctrl+C для выхода)\n";

    while (running) {
        cout << "Ожидание клиента...\n";
        
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0) {
            if (!running) break;
            perror("accept failed");
            continue;
        }

        string client_ip = inet_ntoa(address.sin_addr);
        cout << "Клиент подключен: " << client_ip << endl;
        cout << "Для отключения клиента введите 'quit'" << endl;

        // Устанавливаем таймаут для операций
        struct timeval timeout;
        timeout.tv_sec = 30; // 30 секунд таймаут
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        bool client_connected = true;
        
        while (running && client_connected) {
            // Проверяем ввод от пользователя сервера
            struct pollfd stdin_poll;
            stdin_poll.fd = STDIN_FILENO;
            stdin_poll.events = POLLIN;
            
            // Проверяем данные от клиента
            struct pollfd client_poll;
            client_poll.fd = client_socket;
            client_poll.events = POLLIN;
            
            struct pollfd fds[2] = {stdin_poll, client_poll};
            int result = poll(fds, 2, 1000); // Таймаут 1 секунда
            
            if (result < 0) {
                perror("poll failed");
                break;
            }
            
            // Обрабатываем ввод от сервера
            if (fds[0].revents & POLLIN) {
                string server_message;
                getline(cin, server_message);
                
                if (server_message == "quit") {
                    cout << "Отключение клиента...\n";
                    client_connected = false;
                    break;
                }
                
                if (!server_message.empty()) {
                    // Добавляем символ новой строки для корректного чтения клиентом
                    server_message += "\n";
                    ssize_t sent = send(client_socket, server_message.c_str(), server_message.length(), 0);
                    if (sent <= 0) {
                        perror("send failed");
                        client_connected = false;
                    } else {
                        cout << "Сообщение отправлено клиенту\n";
                    }
                }
            }
            
            // Обрабатываем данные от клиента
            if (fds[1].revents & POLLIN) {
                vector<char> buffer(buffer_size, 0);
                ssize_t received = recv(client_socket, buffer.data(), buffer_size - 1, 0);
                
                if (received > 0) {
                    buffer[received] = '\0';
                    cout << "Клиент: " << buffer.data();
                    if (buffer[received-1] != '\n') cout << endl;
                } else if (received == 0) {
                    cout << "Клиент отключился\n";
                    client_connected = false;
                } else {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv failed");
                        client_connected = false;
                    }
                }
            }
        }
        
        close(client_socket);
        cout << "Соединение с клиентом закрыто\n\n";
    }

    close(server_fd);
    cout << "Сервер остановлен\n";
    return 0;
}