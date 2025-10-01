#include <iostream>
#include <config.hpp>
#include <socket.hpp>
#include <string>

int main(int argc, char **argv)
{
    try
    {
        Config conf = load_config_from_args(argc, argv);

        auto socket = create_socket();

        sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET; // UDP
        server_addr.sin_port = htons(conf["port"]); 
        inet_pton(AF_INET, conf["ip"].get<std::string>().c_str(), &server_addr.sin_addr);

        std::vector<char> buffer(1024);
        sockaddr_in response_addr;

        while(true)
        {
            std::string message = "";
            std::cin >> message;
            send_message(socket, message, server_addr);
            std::cout << "Message sent!\n";

            // Optional: Wait for response from server
            auto bytes = get_message(socket, buffer.data(), buffer.size() - 1, response_addr);
            buffer[bytes] = '\0';
            std::cout << "Server response: " << buffer.data() << std::endl;        }
            
            close(socket);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}