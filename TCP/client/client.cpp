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
#include "config.hpp"

using namespace std;

atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
    cout << "\nЗавершение работы клиента...\n";
}

int connect_to_server(uint16_t port, const string &ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return -1;
    }

    // Устанавливаем таймаут на подключение
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid IP address\n";
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect failed");
        close(sock);
        return -1;
    }

    // Увеличиваем таймаут после подключения
    timeout.tv_sec = 30;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    return sock;
}

void communicate_with_server(int sock) {
    bool connected = true;
    
    while (running && connected) {
        // Проверяем ввод пользователя и данные от сервера
        struct pollfd stdin_poll;
        stdin_poll.fd = STDIN_FILENO;
        stdin_poll.events = POLLIN;
        
        struct pollfd server_poll;
        server_poll.fd = sock;
        server_poll.events = POLLIN;
        
        struct pollfd fds[2] = {stdin_poll, server_poll};
        int result = poll(fds, 2, 1000); // Таймаут 1 секунда
        
        if (result < 0) {
            perror("poll failed");
            break;
        }
        
        // Обрабатываем ввод пользователя
        if (fds[0].revents & POLLIN) {
            string user_message;
            getline(cin, user_message);
            
            if (user_message == "quit") {
                cout << "Завершение работы...\n";
                running = false;
                break;
            }
            
            if (user_message == "reconnect") {
                cout << "Принудительное переподключение...\n";
                connected = false;
                break;
            }
            
            if (!user_message.empty()) {
                user_message += "\n";
                ssize_t sent = send(sock, user_message.c_str(), user_message.length(), 0);
                if (sent <= 0) {
                    perror("send failed");
                    connected = false;
                } else {
                    cout << "Сообщение отправлено серверу\n";
                }
            }
        }
        
        // Обрабатываем данные от сервера
        if (fds[1].revents & POLLIN) {
            vector<char> buffer(1024, 0);
            ssize_t received = recv(sock, buffer.data(), buffer.size() - 1, 0);
            
            if (received > 0) {
                buffer[received] = '\0';
                cout << "Сервер: " << buffer.data();
                if (buffer[received-1] != '\n') cout << endl;
            } else if (received == 0) {
                cout << "Сервер отключился\n";
                connected = false;
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("recv failed");
                    connected = false;
                }
            }
        }
        
        // Проверяем соединение (heartbeat)
        static int heartbeat_counter = 0;
        heartbeat_counter++;
        if (heartbeat_counter >= 10) { // Каждые 10 секунд
            heartbeat_counter = 0;
            // Простая проверка - пытаемся отправить 0 байт
            ssize_t result = send(sock, "", 0, MSG_NOSIGNAL);
            if (result < 0) {
                cout << "Соединение с сервером разорвано\n";
                connected = false;
            }
        }
    }
    
    if (sock >= 0) {
        close(sock);
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    Config conf = load_config_from_args(argc, argv);

    uint16_t port = conf["port"].get<uint16_t>();
    string ip = conf["ip"].get<std::string>();
    int reconnect_delay = conf["delay"].get<uint16_t>(); // секунды между попытками
    int attempt = 0;

    cout << "Клиент запущен\n";
    cout << "Подключение к " << ip << ":" << port << "...\n";
    cout << "Команды:\n";
    cout << "  quit - выход из программы\n";
    cout << "  reconnect - принудительное переподключение\n\n";

    while (running) {
        attempt++;
        cout << "Попытка подключения #" << attempt << "...\n";
        
        int sock = connect_to_server(port, ip);
        if (sock >= 0) {
            cout << "Успешно подключено к серверу!\n";
            cout << "Введите сообщения для отправки серверу:\n";
            
            attempt = 0; // Сбрасываем счетчик попыток при успешном подключении
            communicate_with_server(sock);
            
            if (running) {
                cout << "Потеряно соединение с сервером. Переподключение через " << reconnect_delay << " секунд...\n";
                cout << "Нажмите Ctrl+C для выхода или подождите переподключения\n";
                
                // Ожидание перед переподключением
                for (int i = reconnect_delay; i > 0 && running; i--) {
                    cout << "Переподключение через " << i << " сек...\r" << flush;
                    sleep(1);
                }
                cout << endl;
            }
        } else {
            cout << "Не удалось подключиться к серверу\n";
            
            if (attempt >= 5) {
                cout << "Не удалось подключиться после " << attempt << " попыток.\n";
                cout << "Продолжить попытки? (y/n): ";
                
                string answer;
                getline(cin, answer);
                if (answer != "y" && answer != "Y") {
                    running = false;
                    break;
                }
                attempt = 0; // Сброс счетчика после подтверждения пользователя
            } else {
                cout << "Повторная попытка через " << reconnect_delay << " секунд...\n";
                sleep(reconnect_delay);
            }
        }
    }

    cout << "Клиент остановлен\n";
    return 0;
}