#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <config.hpp>
#include <vector>
#include <csignal>
#include <thread>
#include <atomic>
#include <poll.h>
#include <sys/timerfd.h>
#include <sys/time.h>

using namespace std;

atomic<bool> running{true};

void signal_handler(int sig) {
    running = false;
    cout << "\nПолучен сигнал завершения. Завершение работы...\n";
}

// Функция для установки таймаута на сокет
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

// Функция для безопасной отправки данных
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
            } else if (errno == EPIPE) {
                cerr << "Клиент отключился\n";
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

// Функция для безопасного чтения данных
string safe_receive(int socket, size_t buffer_size, int timeout_sec = 5) {
    if (!set_socket_timeout(socket, timeout_sec)) {
        return "";
    }
    
    vector<char> buffer(buffer_size, 0);
    ssize_t total_received = 0;
    
    // Сначала читаем длину сообщения (если используется протокол с длиной)
    ssize_t received = read(socket, buffer.data(), buffer_size - 1);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            cerr << "Таймаут ожидания сообщения\n";
            return "";
        } else {
            perror("Ошибка чтения");
            return "";
        }
    } else if (received == 0) {
        cout << "Клиент отключился\n";
        return "";
    }
    
    buffer[received] = '\0';
    return string(buffer.data());
}

// Поток для чтения сообщений от клиента
void client_receiver_thread(int client_socket, const string& client_ip, size_t buffer_size) {
    cout << "Поток приема запущен для клиента " << client_ip << endl;
    
    while (running) {
        string message = safe_receive(client_socket, buffer_size, 10);
        
        if (message.empty()) {
            if (running) {
                cerr << "Ошибка получения сообщения от клиента " << client_ip << endl;
            }
            break;
        }
        
        cout << "Клиент " << client_ip << ": " << message << endl;
        
        // Эхо-ответ (можно убрать если не нужно)
        if (!safe_send(client_socket, "Эхо: " + message)) {
            break;
        }
    }
    
    close(client_socket);
    cout << "Поток приема завершен для клиента " << client_ip << endl;
}

// Поток для отправки сообщений клиенту
void client_sender_thread(int client_socket, const string& client_ip) {
    cout << "Поток отправки запущен для клиента " << client_ip << endl;
    
    while (running) {
        string message;
        cout << "Сервер (для " << client_ip << "): ";
        
        if (!getline(cin, message)) {
            if (running) {
                cout << "Ввод прерван\n";
            }
            break;
        }
        
        if (message.empty()) {
            continue;
        }
        
        if (message == "/quit") {
            cout << "Завершение работы с клиентом " << client_ip << endl;
            running = false;
            break;
        }
        
        if (!safe_send(client_socket, message)) {
            cerr << "Не удалось отправить сообщение клиенту " << client_ip << endl;
            break;
        }
        
        cout << "Сообщение отправлено клиенту " << client_ip << endl;
    }
    
    close(client_socket);
    cout << "Поток отправки завершен для клиента " << client_ip << endl;
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN); // Игнорировать SIGPIPE

    auto conf = load_config_from_args(argc, argv);

    size_t buffer_size = conf["buffer size"].get<size_t>();
    uint16_t port = conf["port"].get<uint16_t>();
    int timeout_sec = conf.value("timeout", 5);

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
        perror("setsockopt SO_REUSEADDR failed");
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

    cout << "Сервер запущен на порту " << port << ". Ожидание подключения...\n";
    cout << "Таймаут операций: " << timeout_sec << " сек.\n";
    cout << "Введите '/quit' для завершения работы.\n";

    while (running) {
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0) {
            if (!running) break;
            perror("accept failed");
            continue;
        }

        string client_ip = inet_ntoa(address.sin_addr);
        cout << "Новое подключение: " << client_ip << endl;

        // Запускаем потоки для общения с клиентом
        thread receiver_thread(client_receiver_thread, client_socket, client_ip, buffer_size);
        thread sender_thread(client_sender_thread, client_socket, client_ip);

        // Ждем завершения потоков
        sender_thread.join();
        receiver_thread.join();

        cout << "Соединение с клиентом " << client_ip << " закрыто.\n";
        cout << "Ожидание нового подключения...\n";
    }

    close(server_fd);
    cout << "Сервер остановлен.\n";
    return 0;
}