#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

const int PORT = 8888;
const int MAX_CONNECTIONS = 5;

int main() {
    int serverSocket, newSocket;
    sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(serverAddr);
    char buffer[1024] = {0};
    const char* response = "Hello, Client!";

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Socket creation error" << std::endl;
        return 1;
    }

    // Set non-blocking mode
    int flags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);

    // Bind socket to IP and port
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Bind error" << std::endl;
        close(serverSocket);
        return 1;
    }

    // Listen for incoming connections
    listen(serverSocket, MAX_CONNECTIONS);
    std::cout << "Server listening on port " << PORT << std::endl;

    struct pollfd fds[MAX_CONNECTIONS + 1];
    memset(fds, 0, sizeof(fds));

    fds[0].fd = serverSocket;
    fds[0].events = POLLIN; // Check for incoming connection requests
    int nfds = 1;

    while (true) {
        int pollResult = poll(fds, nfds, -1); // Wait indefinitely for events

        if (pollResult < 0) {
            std::cerr << "Poll error" << std::endl;
            break;
        }

        if (fds[0].revents & POLLIN) {
            // Incoming connection request
            newSocket = accept(serverSocket, (struct sockaddr*)&serverAddr, &addrLen);
            if (newSocket >= 0) {
                std::cout << "New connection established." << std::endl;

                // Set non-blocking mode for the new socket
                int flags = fcntl(newSocket, F_GETFL, 0);
                fcntl(newSocket, F_SETFL, flags | O_NONBLOCK);

                // Add the new socket to the pollfd array
                fds[nfds].fd = newSocket;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        // Handle data from connected clients
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                // Data received from a client
                int bytesRead = recv(fds[i].fd, buffer, sizeof(buffer), 0);
                if (bytesRead > 0) {
                    std::cout << "Received from client: " << buffer << std::endl;

                    // Send response to client
                    send(fds[i].fd, response, strlen(response), 0);
                } else if (bytesRead == 0) {
                    // Connection closed by client
                    std::cout << "Client disconnected." << std::endl;

                    // Read remaining data on the socket
                    do {
                        bytesRead = recv(fds[i].fd, buffer, sizeof(buffer), 0);
                    } while (bytesRead > 0);

                    // No more data to read, so shutdown before closing
                    shutdown(fds[i].fd, SHUT_RDWR);
                    close(fds[i].fd);

                    // Remove the disconnected socket from the pollfd array
                    nfds--;
                    fds[i] = fds[nfds];
                }
            }
        }
    }

    // Close all remaining sockets
    for (int i = 1; i < nfds; i++) {
        // No more data to read, so shutdown before closing
        shutdown(fds[i].fd, SHUT_RDWR);
        close(fds[i].fd);
    }

    // Close the server socket
    close(serverSocket);

    return 0;
}
