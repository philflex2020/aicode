#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int PORT = 8888;

int main() {
    int clientSocket;
    sockaddr_in serverAddr;
    char buffer[1024] = {0};
    const char* message = "Hello, Server!";

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Socket creation error" << std::endl;
        return 1;
    }

    // Connect to the server
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Connection error" << std::endl;
        close(clientSocket);
        return 1;
    }

    // Send data to server
    send(clientSocket, message, strlen(message), 0);

    // Receive response from server
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead > 0) {
        std::cout << "Received from server: " << buffer << std::endl;
    }

    // Close the socket
    shutdown(clientSocket, SHUT_RDWR);
    close(clientSocket);

    return 0;
}
