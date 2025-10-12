#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <config.hpp>
#include <thread>

using namespace std;

int connect_to_server(uint16_t port, const string &ip)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "Не удалось создать сокет\n";
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0)
    {
        std::cerr << "Неверный IP-адрес\n";
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        close(sock);
        return -1; // подключение не удалось
    }

    return sock;
}

int main(int argc, char **argv)
{
    auto conf = load_config_from_args(argc, argv);

    size_t buffer_size = conf["buffer size"].get<size_t>();
    uint16_t port = conf["port"].get<uint16_t>();
    string ip = conf["ip"].get<string>();
    int delay = conf["delay"].get<int>();

    std::cout << "Клиент запущен. Подключение к " << ip << ":" << port << "...\n";

    int sock = -1;

    // Основной цикл: переподключение при необходимости
    while (true)
    {
        if (sock == -1)
        {
            sock = connect_to_server(port, ip);
            if (sock == -1)
            {
                std::cout << "Не удаётся подключиться к серверу. Повтор через "
                          << delay << " сек...\n";
                std::this_thread::sleep_for(std::chrono::seconds(delay));
                continue;
            }
            std::cout << "Подключено к серверу!\n";
        }
        // Обмен сообщениями
        std::string message;
        std::cout << "Вы: ";
        if (!std::getline(std::cin, message))
        {
            std::cout << "\nЗавершение работы по запросу пользователя.\n";
            break;
        }

        if (message.empty())
            continue;

        // Отправка
        if (send(sock, message.c_str(), message.length(), 0) <= 0)
        {
            std::cerr << "Ошибка отправки. Сервер, возможно, отключён.\n";
            close(sock);
            sock = -1;
            continue; // вернёмся к попытке переподключения
        }

        // Приём ответа
        vector<char> buffer(buffer_size, {});
        int valread = read(sock, buffer.data(), sizeof(buffer) - 1);
        if (valread <= 0)
        {
            std::cout << "Сервер разорвал соединение. Попытка переподключения...\n";
            close(sock);
            sock = -1;
            continue;
        }

        buffer[valread] = '\0';
        std::cout << "Сервер: " << buffer.data() << std::endl;
    }

    if (sock != -1)
        close(sock);

    return 0;
}