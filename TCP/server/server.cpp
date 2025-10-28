#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <config.hpp>
#include <vector>
#include <csignal>

using namespace std;

volatile bool running = true;

void signal_handler(int sig)
{
    running = false;
    std::cout << "\nПолучен сигнал завершения. Завершение работы...\n";
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, signal_handler);

    auto conf = load_config_from_args(argc, argv);

    size_t buffer_size = conf["buffer size"].get<size_t>();
    uint16_t port = conf["port"].get<uint16_t>();

    int server_fd, new_socket;
    sockaddr_in address;
    int addrlen = sizeof(address);

    // Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        return -1;
    }

    // Разрешить повторное использование адреса (чтобы не ждать TIME_WAIT)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Привязка сокета к порту
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        return -1;
    }

    // Прослушивание порта
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        close(server_fd);
        return -1;
    }

    std::cout << "Сервер запущен на порту " << port << ". Ожидание подключения...\n";

    while (running)
    {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            if (!running)
                break; // прервано сигналом
            perror("accept failed");
        }

        std::cout << "Новое подключение: " << inet_ntoa(address.sin_addr) << std::endl;

        vector<char> buffer(buffer_size, {});
        while (running)
        {
            memset(buffer.data(), 0, buffer_size);
            int valread = read(new_socket, buffer.data(), buffer_size - 1);
            if (valread <= 0)
            {
                if (valread < 0)
                    perror("read error");
                else
                    std::cout << "Клиент отключился.\n";
                break;
            }
            buffer[valread] = '\0';
            std::cout << "От клиента: " << buffer.data() << std::endl;
            std::cout << "Ответ сервера: " << std::endl;
            std::string res;
            std::cin >> res;
            send(new_socket, res.data(), strlen(res.data()), 0);
        }

        close(new_socket);
        std::cout << "Соединение закрыто. Ожидание нового клиента...\n";
    }

    close(server_fd);
    std::cout << "Сервер остановлен.\n";
    return 0;
}