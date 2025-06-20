// ere’s the concept for `logger_server.cpp`:
// ./logger_server <port> <log_file> <image_file>
// g++ logger_server.cpp -std=c++17 -pthread -o logger_server
// ./logger_server 9000 log.bin image.bin

#include "Logger.h"
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <cstring>
#include <chrono>
#include <poll.h>

#include <chrono>


double ref_time_dbl_wall() {
    using Clock = std::chrono::system_clock;
    auto now = Clock::now();
    std::chrono::duration<double> timestamp = now.time_since_epoch();
    return timestamp.count();  // seconds since epoch
}

double ref_time_dbl() {
    using Clock = std::chrono::steady_clock;
    static const auto start_time = Clock::now();
    auto now = Clock::now();
    std::chrono::duration<double> elapsed = now - start_time;
    return elapsed.count();  // seconds with sub-second resolution
}

std::atomic<bool> shutdown_requested{false};

uint16_t memory_image[65536];

std::mutex mem_lock;
std::mutex file_lock;

std::string log_file = "log.bin";
std::string image_file = "image.bin";

void handle_signal(int signum) {
    std::cerr << "\nSignal " << signum << " received. Shutting down...\n";
    shutdown_requested = true;
}

void flush_image_to_disk() {
    std::lock_guard<std::mutex> lg(mem_lock);
    std::ofstream img(image_file, std::ios::binary | std::ios::trunc);
    img.write(reinterpret_cast<char*>(memory_image), sizeof(memory_image));
    std::cout << "[flush] memory image written to " << image_file << "\n";
}

void periodic_flusher() {
    const double flush_interval = 10.0; // seconds
    const double check_interval = 0.1;  // seconds
    double last_flush_time = ref_time_dbl();

    while (!shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(int(check_interval * 1000)));

        double now = ref_time_dbl();
        if ((now - last_flush_time) >= flush_interval) {
            flush_image_to_disk();
            last_flush_time = now;
        }
    }

    flush_image_to_disk();  // Final flush on exit
}

void handle_client(int client_fd) {
    std::ofstream out(log_file, std::ios::binary | std::ios::app);

    while (!shutdown_requested.load()) {
        uint32_t len;
        if (recv(client_fd, &len, sizeof(len), MSG_WAITALL) != sizeof(len)) break;

        std::vector<uint8_t> buf(len);
        if (recv(client_fd, buf.data(), len, MSG_WAITALL) != (ssize_t)len) break;

        //DataPacket pkt = deserialize_packet(buf);

        // Log to file
        {
            std::lock_guard<std::mutex> lg(file_lock);
            out.write(reinterpret_cast<char*>(&len), sizeof(len));
            out.write(reinterpret_cast<char*>(buf.data()), buf.size());
        }

        // Apply to memory
        // {
        //     std::lock_guard<std::mutex> lg(mem_lock);
        //     for (size_t i = 0; i < pkt.control.size(); ++i) {
        //         if (pkt.control[i] < 65536)
        //             memory_image[pkt.control[i]] = pkt.data[i];
        //     }
        // }
    }

    close(client_fd);
    std::cout << "Client disconnected\n";
}

int main(int argc, char* argv[]) {
    int port = 9000;

    if (argc > 1) {
        port = std::stoi(argv[1]);
        
    }
    if (argc > 2) {
        image_file = argv[2];
    }
    if (argc > 3) {
        log_file = argv[3];
    }

    if (argc == 1) {
        std::cerr << "Usage: " << argv[0] << " <port> <image_file> <log_file\n";
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
    }
    std::cout << "Logger listening on port " << port << "\n";
    std::thread flusher(periodic_flusher);

    while (!shutdown_requested.load()) {
        struct pollfd pfd;
        pfd.fd = server_fd;
        pfd.events = POLLIN;

        // Wait up to 500ms for a new connection or shutdown flag
        int ret = poll(&pfd, 1, 500);
        if (ret < 0) {
            perror("poll");
            break;
        }

        if (ret == 0) {
            // timeout, check shutdown_requested again
            continue;
        }

        if (pfd.revents & POLLIN) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

            if (client_fd >= 0) {
                std::cout << "New client\n";
                std::thread(handle_client, client_fd).detach();
            }
        }
    }


    std::cout << "Waiting for flusher to complete...\n";
    flusher.join();
    flush_image_to_disk();
    close(server_fd);
    std::cout << "Logger shut down cleanly.\n";
    return 0;
}
