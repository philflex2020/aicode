#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include "perf.h"

const char* SERVER_IP = "127.0.0.1";
const int PORT = 8888;

int main() {
    int clientSocket;
    sockaddr_in serverAddr;
    char buffer[1024] = {0};
    const char* message = "Hello, Server!";
    int messageCount = 1000;

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Socket creation error" << std::endl;
        return 1;
    }

    // Set non-blocking mode
    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

    // Set server information
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // Start timing for connection
    perf::Timing connectTiming;
    connectTiming.start();

    // Connect to the server
    int connectResult = connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (connectResult == -1) {
        std::cerr << "Connection error" << std::endl;
        close(clientSocket);
        return 1;
    }

    // End timing for connection
    connectTiming.stop();
    std::cout << "Time spent on connecting: " << connectTiming.getDuration() << " ms" << std::endl;

    // Start timing for round-trip messages
    perf::Timing roundTripTiming;
    roundTripTiming.start();

    for (int i = 0; i < messageCount; i++) {
        // Send message to server
        send(clientSocket, message, strlen(message), 0);

        // Wait for response
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead > 0) {
            std::cout << "Received from server: " << buffer << std::endl;
        } else {
            std::cerr << "Error receiving response from server" << std::endl;
            break;
        }
    }

    // End timing for round-trip messages
    roundTripTiming.stop();
    std::cout << "Time spent on round-trip messages: " << roundTripTiming.getDuration() << " ms" << std::endl;

    // Close the client socket
    close(clientSocket);

    return 0;
}
