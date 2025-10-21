#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <config.hpp>
#include <thread>
#include <atomic>
#include <poll.h>
#include <csignal>

using namespace std;

atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
    cout << "\nЗавершение работы...\n";
}

// Функция для установки таймаута
bool set_socket_timeout(int sockfd, int seconds) {
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        return false;
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_SNDTIMEO failed");
        return false;
    }
    
    return true;
}

// Безопасная отправка
bool safe_send(int socket, const string& message, int timeout_sec = 5) {
    if (!set_socket_timeout(socket, timeout_sec)) {
        return false;
    }
    
    size_t total_sent = 0;
    while (total_sent < message.length() && running) {
        ssize_t sent = send(socket, message.c_str() + total_sent, 
                           message.length() - total_sent, MSG_NOSIGNAL);
        
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cerr << "Таймаут отправки сообщения\n";
                return false;
            } else {
                perror("Ошибка отправки");
                return false;
            }
        }
        total_sent += sent;
    }
    
    return total_sent == message.length();
}

// Безопасное получение
string safe_receive(int socket, size_t buffer_size, int timeout_sec = 5) {
    if (!set_socket_timeout(socket, timeout_sec)) {
        return "";
    }
    
    vector<char> buffer(buffer_size, 0);
    ssize_t received = read(socket, buffer.data(), buffer_size - 1);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            cerr << "Таймаут ожидания ответа от сервера\n";
            return "";
        } else {
            perror("Ошибка чтения");
            return "";
        }
    } else if (received == 0) {
        cout << "Сервер отключился\n";
        return "";
    }
    
    buffer[received] = '\0';
    return string(buffer.data());
}

// Поток для приема сообщений от сервера
void receiver_thread(int sock, size_t buffer_size, int timeout_sec) {
    cout << "Поток приема запущен\n";
    
    while (running) {
        string response = safe_receive(sock, buffer_size, timeout_sec);
        
        if (response.empty()) {
            if (running) {
                cerr << "Потеряно соединение с сервером\n";
            }
            running = false;
            break;
        }
        
        cout << "\nСервер: " << response << endl;
        cout << "Вы: " << flush;
    }
    
    cout << "Поток приема завершен\n";
}

int connect_to_server(uint16_t port, const string &ip, int timeout_sec = 5) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Не удалось создать сокет\n";
        return -1;
    }

    // Устанавливаем таймаут на подключение
    if (!set_socket_timeout(sock, timeout_sec)) {
        close(sock);
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        cerr << "Неверный IP-адрес\n";
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            cerr << "Таймаут подключения к серверу\n";
        } else {
            perror("Ошибка подключения");
        }
        close(sock);
        return -1;
    }

    return sock;
}

int main(int argc, char **argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    auto conf = load_config_from_args(argc, argv);

    size_t buffer_size = conf["buffer size"].get<size_t>();
    uint16_t port = conf["port"].get<uint16_t>();
    string ip = conf["ip"].get<string>();
    int delay = conf.value("delay", 3);
    int timeout_sec = conf.value("timeout", 5);
    int connect_timeout = conf.value("connect_timeout", 5);

    cout << "Клиент запущен. Подключение к " << ip << ":" << port << "...\n";
    cout << "Таймаут операций: " << timeout_sec << " сек.\n";
    cout << "Таймаут подключения: " << connect_timeout << " сек.\n";
    cout << "Введите сообщение или '/quit' для выхода:\n";

    int sock = -1;

    while (running) {
        if (sock == -1) {
            sock = connect_to_server(port, ip, connect_timeout);
            if (sock == -1) {
                cout << "Не удаётся подключиться к серверу. Повтор через " 
                     << delay << " сек...\n";
                sleep(delay);
                continue;
            }
            cout << "Подключено к серверу!\n";
            
            // Запускаем поток для приема сообщений
            thread receiver(receiver_thread, sock, buffer_size, timeout_sec);
            receiver.detach();
        }

        // Ввод сообщения от пользователя
        string message;
        cout << "Вы: ";
        if (!getline(cin, message)) {
            cout << "\nЗавершение работы\n";
            break;
        }

        if (message.empty()) {
            continue;
        }

        if (message == "/quit") {
            cout << "Завершение работы\n";
            break;
        }

        // Отправка сообщения
        if (!safe_send(sock, message, timeout_sec)) {
            cerr << "Ошибка отправки. Переподключение...\n";
            close(sock);
            sock = -1;
            continue;
        }

        cout << "Сообщение отправлено. Ожидание ответа...\n";
    }

    running = false;
    if (sock != -1) {
        close(sock);
    }

    cout << "Клиент остановлен.\n";
    return 0;
}