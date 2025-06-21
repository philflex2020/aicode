#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


#pragma pack(push, 1)
struct LogHeader {
    uint32_t packet_hdr;
    uint32_t packet_size;
    uint64_t timestamp_us;
    uint8_t data[1];  // [Header][control][data]
};

struct Header {
    uint8_t cmd;
    uint8_t mytype;
    uint8_t seq;
    uint8_t reserved;
};
#pragma pack(pop)

int main(int argc, char* argv[]) {
    const char* filename = (argc > 1) ? argv[1] : "log.bin";
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open " << filename << "\n";
        return 1;
    }

    while (true) {
        // Read LogHeader (excluding flexible array)
        LogHeader log;
        if (!in.read(reinterpret_cast<char*>(&log), sizeof(LogHeader) - 1)) break;
        //std::cout << " packet_size " <<  log.packet_size << std::endl;
        //std::cout << " left to read " <<  log.packet_size - (sizeof(LogHeader) - 1) << std::endl;
        
        //std::cout << " packet_size htonl " <<  htonl(log.packet_size) << std::endl;

        // Read full payload
        std::vector<uint8_t> payload(log.packet_size - (sizeof(LogHeader) - 1));
        if (!in.read(reinterpret_cast<char*>(payload.data()), payload.size())) break;

        // Extract Header + control size
        if (payload.size() < sizeof(Header)) {
            std::cerr << "Corrupt payload\n";
            break;
        }
        // TODO header will have control size and data size
        //const Header* hdr = reinterpret_cast<const Header*>(payload.data());

        size_t control_bytes = payload.size() - sizeof(Header);
        size_t control_words = control_bytes / sizeof(uint16_t);
        size_t control_vec_size = 0;

        if (control_words >= 2) {
            // Estimate size by assuming well-formed alternating control + data
            control_vec_size = control_words / 2;
        }

        std::cout << "ts: " << log.timestamp_us
                  << " μs | control_vec size: " << control_vec_size << "\n";
    }

    return 0;
}
