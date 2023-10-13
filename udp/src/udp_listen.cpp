#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>

int main() {
    int sockfd;
    sockaddr_in servaddr{}, cliaddr{};
    char buffer[1024];

    // Prompt for the port number
    unsigned short port;
    std::cout << "Enter the port number to listen on: ";
    std::cin >> port;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket");
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    // Bind the socket
    if (bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Failed to bind to port");
        return 1;
    }

    std::cout << "Listening for UDP packets on port " << port << "...\n";

    // Open output file
    std::ofstream outFile("output.txt", std::ios::out | std::ios::binary);

    auto start = std::chrono::steady_clock::now();

    while (true) {
        socklen_t len = sizeof(cliaddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr*)&cliaddr, &len);

        if (n > 0) {
            outFile.write(buffer, n);
        }

        auto end = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(end - start).count() >= 10) {
            break;
        }
    }

    std::cout << "10 seconds worth of data written to output.txt" << std::endl;

    close(sockfd);
    return 0;
}