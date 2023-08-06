#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include <netdb.h>
#include <queue>
#include <mutex>
#include "perf.h"

// std::queue<PerfMeasurement> perfQueue;
// std::mutex perfMutex;
// std::condition_variable perfCV;
bool isAggregatorRunning = true;

const char* SERVER_IP = "172.17.0.2";
const int PORT = 8888;


void aggregatorThread() {
    double maxTime = std::numeric_limits<double>::min();
    double minTime = std::numeric_limits<double>::max();
    double totalTime = 0.0;
    int count = 0;

    while (isAggregatorRunning) {
        std::unique_lock<std::mutex> lock(Perf::perfMutex);

        // Wait for data to be available in the queue
        Perf::perfCV.wait(lock, [] { return !Perf::perfQueue.empty(); });
        std::cout << "thread wake" << std::endl;
        
        if (!isAggregatorRunning) {
            std::cout << "thread stopping" << std::endl;
            // Exit the loop if the aggregator is no longer running
            break;

        }

        while (!Perf::perfQueue.empty()) {
            // Process the measurement results from the queue
            PerfMeasurement measurement = Perf::perfQueue.front();
            Perf::perfQueue.pop();

// Update statistics
            maxTime = std::max(maxTime, measurement.elapsedTime);
            minTime = std::min(minTime, measurement.elapsedTime);
            totalTime += measurement.elapsedTime;

            ++count;
            std::cout << "Measurement: " << measurement.name << ", Elapsed Time: " << measurement.elapsedTime << " ms\n";
        }
    }
    // Calculate average after the loop
    double averageTime = (count > 0) ? (totalTime / count) : 0.0;

    // Print the results
    std::cout << "Aggregator: Max Time: " << maxTime << " ms\n";
    std::cout << "Aggregator: Min Time: " << minTime << " ms\n";
    std::cout << "Aggregator: Average Time: " << averageTime << " ms\n";
    std::cout << "Aggregator: Count: " << count << "\n";
}





void sendData(int clientSocket, int threadid) {
    const char* message = "Hello, Server!";
    int messageCount = 1000;
    char buffer[1024] = {0}; // Added buffer declaration

    //perf::Timing roundTripTiming;
    //std::string ThreadName = "ClientThread :" + std::string(threadid);
    Perf perf("ClientThread");
    perf.start();

    for (int i = 0; i < messageCount; i++) {
        // Send message to server
        send(clientSocket, message, strlen(message), 0);

        // Wait for response
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead > 0) {
            std::cout << " Thread :"<< threadid << "  Received from server: " << buffer << std::endl;
        } else {
            std::cerr << "Error receiving response from server" << std::endl;
            break;
        }
    }
    perf.stop();
    // End timing for round-trip messages
    //roundTripTiming.stop();
    //collector.addTiming(roundTripTiming.getDuration());
}

void clientThread( int threadid) {
    int clientSocket;
    struct sockaddr_in serverAddr;
    struct in_addr ipv4addr;
    //struct hostent *hp;
    memset(&serverAddr, 0, sizeof(serverAddr));
    printf(" thread %d running \n", threadid);


    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Client socket creation error" << std::endl;
        return ;
    }

    // Set non-blocking mode
    //int flags = fcntl(clientSocket, F_GETFL, 0);
    //fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
    inet_pton(AF_INET, SERVER_IP, &ipv4addr);
    // hp = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
    // printf(" hp = %p \n", (void *)hp);
    // if (!hp)
    //   hp = gethostbyname(SERVER_IP);
    // printf(" hp 2 = %p \n", (void *)hp);
    // printf(" hp 2 = %x . %x . %x .%x  \n", hp->h_addr[0], hp->h_addr[1], hp->h_addr[2], hp->h_addr[3]);

    // bcopy(hp->h_addr, &(serverAddr.sin_addr.s_addr), hp->h_length);

    // Set server information
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // Start timing for connection
    //perf::Timing connectTiming;
    //connectTiming.start();

    // Connect to the server
    int connectResult = connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (connectResult == -1) {
        std::cerr << "Client Connection error" << std::endl;
        close(clientSocket);
        return ;
    }

    // End timing for connection
    //connectTiming.stop();
    //std::cout << "Client Time spent on connecting: " << connectTiming.getDuration() << " ms" << std::endl;

    // Create a thread to send data
    std::thread sendThread(sendData, clientSocket,threadid);

    // Join the send thread
    sendThread.join();

    // Close the client socket
    close(clientSocket);

    // Get timing from the collector
    //double roundTripTime = collector.getTiming();
    //std::cout << "Client Time spent on round-trip messages: " << roundTripTime << " ms" << std::endl;

    return ;
}

void stopAggregator() {
    std::cout << " stop aggregator" << std::endl;
    isAggregatorRunning = false; 
    Perf perf("EndThread");
    perf.start();
    perf.stop();


    //Perf::perfCV.notify_one();
}


int main() {

     const int numClients = 5;
    std::vector<std::thread> threads;
    // Start the aggregator thread
    std::thread aggregator(aggregatorThread);

    for (int i = 0; i < numClients; ++i) {
        threads.emplace_back(clientThread, i);
    }

    // Wait for all threads to finish
    for (std::thread& t : threads) {
        t.join();
    }

// Terminate the aggregator thread
//    aggregator.detach();
    stopAggregator();
    aggregator.join();
    return 0;



}