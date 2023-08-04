#ifndef SOCKET_HANDLER_H
#define SOCKET_HANDLER_H

#include <map>
#include <string>

class SocketHandler {
public:
    SocketHandler(int port);
    ~SocketHandler();

    void run();

private:
    int serverSocket;
    int portNumber;
    std::map<std::string, std::string> pubSubMap;

    void handleClient(int clientSocket);
    std::string handleGet(const std::string& uri, const std::string& assetId);
    void publishData(const std::string& uri, int interval, int clientSocket);
};

#endif // SOCKET_HANDLER_H
