#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 11111
#define BUFFER_SIZE 1024

int main()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "Socket creation error\n";
        return 1;
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address / Address not supported\n";
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "Connection Failed\n";
        return 1;
    }

    std::cout << "Connected to server at " << SERVER_IP << ":" << SERVER_PORT << "\n";

    while (true)
    {
        std::string msg;
        std::cout << "Enter message: ";
        std::getline(std::cin, msg);

        if (msg == "exit") break;

        send(sock, msg.c_str(), msg.size(), 0);

        char buffer[BUFFER_SIZE] = {0};
        int valread = recv(sock, buffer, BUFFER_SIZE, 0);
        if (valread > 0)
        {
            std::cout << "Server replied: " << std::string(buffer, valread) << "\n";
        }
    }

    close(sock);
    return 0;
}
