#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <vector>

#define PORT 11111
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

int setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main()
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "Bind failed\n";
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) < 0)
    {
        std::cerr << "Listen failed\n";
        return 1;
    }

    setNonBlocking(listen_fd);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        std::cerr << "Failed to create epoll\n";
        return 1;
    }

    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event);

    std::vector<epoll_event> events(MAX_EVENTS);

    std::cout << "Server started on port " << PORT << "\n";

    while (true)
    {
        int n = epoll_wait(epoll_fd, events.data(), MAX_EVENTS, -1);
        if (n < 0)
        {
            std::cerr << "epoll_wait failed\n";
            break;
        }

        for (int i = 0; i < n; ++i)
        {
            if (events[i].data.fd == listen_fd)
            {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) continue;

                setNonBlocking(client_fd);

                epoll_event client_event;
                client_event.events = EPOLLIN | EPOLLET;
                client_event.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);

                std::cout << "New connection: " << inet_ntoa(client_addr.sin_addr) << "\n";
            }
            else
            {
                char buffer[BUFFER_SIZE];
                int bytes_read = recv(events[i].data.fd, buffer, BUFFER_SIZE, 0);
                if (bytes_read <= 0)
                {
                    close(events[i].data.fd);
                    std::cout << "Client disconnected\n";
                }
                else
                {
                    std::string msg(buffer, bytes_read);
                    std::cout << "Received: " << msg << "\n";
                    send(events[i].data.fd, msg.c_str(), msg.size(), 0);
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}
