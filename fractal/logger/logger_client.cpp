#include "Logger.h"

constexpr size_t MAX_BUFFER = 65536;

void dump_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (i % 16 == 0) std::printf("\n%04zx: ", i);
        std::printf("%02x ", data[i]);
    }
    std::printf("\n");
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port>\n";
        return 1;
    }

    const char* server_ip = argv[1];
    int port = std::stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    std::cout << "Connected to logger server at " << server_ip << ":" << port << "\n";

    uint8_t buffer[MAX_BUFFER];
    while (true) {
        ssize_t header_bytes = recv(sock, buffer, sizeof(LogHeader), MSG_WAITALL);
        if (header_bytes <= 0) break;

        LogHeader* hdr = reinterpret_cast<LogHeader*>(buffer);
        if (hdr->packet_hdr != 0) {
            std::cerr << "Invalid header (packet_hdr = " << hdr->packet_hdr << ")\n";
            break;
        }

        if (hdr->packet_size > MAX_BUFFER - sizeof(LogHeader)) {
            std::cerr << "Payload too large\n";
            break;
        }

        ssize_t payload_bytes = recv(sock, buffer + sizeof(LogHeader), hdr->packet_size, MSG_WAITALL);
        if (payload_bytes != hdr->packet_size) {
            std::cerr << "Failed to read full payload\n";
            break;
        }

        std::cout << "Received log packet: " << hdr->packet_size << " bytes | timestamp_us: " << hdr->timestamp_us << "\n";
        dump_hex(hdr->data, hdr->packet_size);
    }

    std::cerr << "Connection closed or error occurred\n";
    close(sock);
    return 0;
}
