// here’s the concept for `logger_server.cpp`:
// ./logger_server <port> <log_file> <image_file>
// g++ logger_server.cpp -std=c++17 -pthread -o logger_server
// ./logger_server 9000 log.bin image.bin


// bit of a mixture here. we need a logger client as well
// this server will accept clients and give them what they want
// for now they want everything.
// this is a server and accepts connections from clients 

#include "Logger.h"

using json = nlohmann::json;

constexpr size_t MEM_SIZE = 32764;
constexpr size_t MAX_SM_MEMORY = 0x8000;
std::atomic<bool> sim_running{false};
std::thread sim_thread;


std::vector<ClientInfo> clients;
std::mutex clients_mutex;


uint16_t *sim_memory;
uint16_t *ref_memory;
//static uint16_t sim_memory[MEM_SIZE] = {0};
//static uint16_t ref_memory[MEM_SIZE] = {0};

int myfd = -1;

// double ref_time_dbl_wall() {
//     using Clock = std::chrono::system_clock;
//     auto now = Clock::now();
//     std::chrono::duration<double> timestamp = now.time_since_epoch();
//     return timestamp.count();  // seconds since epoch
// }

// double ref_time_dbl() {
//     using Clock = std::chrono::steady_clock;
//     static const auto start_time = Clock::now();
//     auto now = Clock::now();
//     std::chrono::duration<double> elapsed = now - start_time;
//     return elapsed.count();  // seconds with sub-second resolution
// }

// uint64_t ref_time_us() {
//     return static_cast<uint64_t>(ref_time_dbl() * 1'000'000);
// }

std::atomic<bool> shutdown_requested{false};

uint16_t memory_image[65536];

std::mutex mem_lock;
std::mutex file_lock;

std::string log_file = "log.bin";
std::string image_file = "image.bin";

void handle_signal(int signum) {
    std::cerr << "\nSignal " << signum << " received. Shutting down...\n";
    shutdown_requested = true;
    sim_running = false;

}

std::string get_log_directory(const std::string& root = "log") {
    time_t raw_time = static_cast<time_t>(ref_time_dbl_wall());
    struct tm timeinfo;
    localtime_r(&raw_time, &timeinfo);

    std::ostringstream oss;
    oss << root << "/"
        << std::setfill('0') << std::setw(4) << (1900 + timeinfo.tm_year) << "/"
        << std::setw(2) << (1 + timeinfo.tm_mon) << "/"
        << std::setw(2) << timeinfo.tm_mday << "/"
        << std::setw(2) << timeinfo.tm_hour << "/"
        << std::setw(2) << timeinfo.tm_min;

    return oss.str();
}


bool ensure_directory_exists(const std::string& dir_path) {
    try {
        return std::filesystem::create_directories(dir_path);
    } catch (const std::exception& e) {
        std::cerr << "Error creating directories: " << e.what() << "\n";
        return false;
    }
}


// Returns connected socket or -1 on failure
int connect_to_input_socket(const char* hostname, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, hostname, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << hostname << "\n";
        close(sock);
        return -1;
    }

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    std::cout << "[startup] connected to " << hostname << ":" << port << "\n";
    return sock;
}

void encode_diffs(const uint16_t* current, uint16_t* previous, size_t mem_size,
                  std::vector<uint16_t>& control,
                  std::vector<uint16_t>& data) {
    control.clear();
    data.clear();

    uint16_t last_written = 0xFFFF;

    for (uint16_t i = 0; i < mem_size / 2;) {
        if (current[i] != previous[i]) {
            uint16_t run_start = i;
            uint16_t run_count = 0;

            // Count how many consecutive changes
            while (i < mem_size / 2 && current[i] != previous[i]) {
                ++run_count;
                ++i;
            }

            // Compute relative offset from last write
            uint16_t rel = (last_written == 0xFFFF) ? run_start : (run_start - last_written);

            // Insert spacing markers for large gaps
            while (rel >= MAX_SM_MEMORY) {
                control.push_back(MAX_SM_MEMORY);  // zero block marker
                control.push_back(0);
                rel -= MAX_SM_MEMORY;
            }

            // Store relative offset and run count
            control.push_back(rel);
            control.push_back(run_count);

            for (uint16_t j = 0; j < run_count; ++j) {
                uint16_t idx = run_start + j;
                data.push_back(current[idx]);
                previous[idx] = current[idx];  // update ref
            }

            last_written = run_start + run_count - 1;
        } else {
            ++i;
        }
    }
}

int  build_log_packet(std::vector<uint8_t>&buf, Header& hdr, const std::vector<uint16_t>& control, const std::vector<uint16_t>& data, uint64_t timestamp_us) {
    // Calculate sizes
    const size_t hdr_size      = sizeof(Header);
    const size_t control_size  = control.size() * sizeof(uint16_t);
    const size_t data_size     = data.size() * sizeof(uint16_t);
    const size_t payload_size  = hdr_size + control_size + data_size;
    const size_t total_size    = sizeof(LogHeader) - 1 + payload_size;

    buf.resize(total_size);

    // Build LogHeader
    LogHeader* log = reinterpret_cast<LogHeader*>(buf.data());
    log->packet_hdr  = 0;  // log packet marker
    log->packet_size = total_size;
    log->timestamp_us = timestamp_us;
    hdr.control_size = control.size();
    hdr.data_size = data.size();

    // Copy Header + control + data
    size_t offset = 0;
    uint8_t* payload = log->data;

    std::memcpy(payload + offset, &hdr, hdr_size);
    offset += hdr_size;

    std::memcpy(payload + offset, control.data(), control_size);
    offset += control_size;

    std::memcpy(payload + offset, data.data(), data_size);

    return (int)buf.size();
}


void run_simulator() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> offset_dist(0, 65535);
    std::uniform_int_distribution<int> value_dist(2000, 4000);
    std::uniform_int_distribution<int> count_dist(100, 400);
    std::cout << "[simulator] started"<<std::endl;
    std::vector<uint16_t> control_vec, data_vec;
    for (int i = 0; i < (int)MEM_SIZE/2; ++i)
    {
        sim_memory[i] = 0;
        ref_memory[i] = 0;
    }
    std::cout << "[simulator] memory cleared"<<std::endl;
    while (sim_running.load()) {

        int count = count_dist(rng);
        std::cout << "[simulator] count " << count <<std::endl;

        for (int i = 0; i < count; ++i) {
            int offset = offset_dist(rng);
            int value = value_dist(rng);
            sim_memory[offset] = value;
        }

        // generate diffs

        if(1){
            encode_diffs(sim_memory, ref_memory, MEM_SIZE, control_vec, data_vec);
// now we have to create the message 
            Header hdr;
            std::vector<uint8_t> log_buf;
            int rc;
            int log_len = build_log_packet(log_buf, hdr, control_vec, data_vec, ref_time_us());
            std::cout << " sending len " << log_len << std::endl; 
            if(myfd> 0)
            {
                rc = write(myfd, log_buf.data(), log_len);
                if(rc <0)
                {
                    std::cerr << " write faiuled " << std::endl;
                }
            }

            //if (!control_vec.empty()) 
            {
                std::cout << "[simulator] " << control_vec.size() << " changes\n";
                // optionally send to a socket, log buffer, or apply_diffs()
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "[simulator] stopped\n";
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


int process_message(int fd, uint8_t* buf, int len)
{
    if (len <= 0) return 0;

    std::string input(reinterpret_cast<char*>(buf), len);
    input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());
    input.erase(std::remove(input.begin(), input.end(), '\n'), input.end());

    if ((buf[0] == 'Q') || (buf[0] == 'q')) {
        std::string goodbye = "Goodbye.\n";
        send(fd, goodbye.c_str(), goodbye.size(), 0);
        return -1;  // signal to break connection loop
     }
    if ((buf[0] == 'H') || (buf[0] == 'h')) {
         const char* help =
        "Available commands:\n"
        "  Q                 - Quit the connection\n"
        "  h or H            - Show this help message\n"
        "  {\"cmd\":\"start_sim\"} - JSON: Start Sim\n"
        "  {\"cmd\":\"status\"} - JSON: Show logger status\n"
        "  {\"cmd\":\"quit\"}   - JSON: Close the connection\n"
        "  {\"cmd\":\"get\", \"offset\": <n>} - JSON: Read value from memory\n"
        "  {\"cmd\":\"dump\", \"range\": [start, end]} - JSON: Dump memory range\n";
        send(fd, help, strlen(help), 0);
        return 0;  // signal to break connection loop
     }

    try {
        auto j = json::parse(input);

        if (j.contains("cmd")) {
            std::string cmd = j["cmd"];

            if (cmd == "status") {
                json reply;
                reply["msg"] = "ok";
                reply["uptime"] = ref_time_dbl(); // or your own uptime function
                std::string out = reply.dump();
                send(fd, out.c_str(), out.size(), 0);
                send(fd, "\n", 1, 0);
                return 0;
            }

            else if (cmd == "quit") {
                std::string out = R"({"msg":"bye"})";
                send(fd, out.c_str(), out.size(), 0);
                send(fd, "\n", 1, 0);
                return -1;
            }
            else if (cmd == "start_sim") {
                if (!sim_running.exchange(true)) {
                    sim_thread = std::thread(run_simulator);
                    std::cout << "[simulator] started\n";
                    json out = {{"msg", "simulator started"}};
                    std::string reply = out.dump();
                    send(fd, reply.c_str(), reply.size(), 0);
                    send(fd, "\n", 1, 0);
                } else {
                    json out = {{"msg", "simulator already running"}};
                    std::string reply = out.dump();
                    send(fd, reply.c_str(), reply.size(), 0);
                    send(fd, "\n", 1, 0);
                }
                return 0;
            }
            else {
                std::string out = R"({"error":"unknown command"})";
                send(fd, out.c_str(), out.size(), 0);
                send(fd, "\n", 1, 0);
                return 0;
            }
        } else {
            std::string out = R"({"error":"missing cmd"})";
            send(fd, out.c_str(), out.size(), 0);
            send(fd, "\n", 1, 0);
            return 0;
        }
    }
    catch (json::parse_error& e) {
        std::string out = R"({"error":"invalid json"})";
        send(fd, out.c_str(), out.size(), 0);
        send(fd, "\n", 1, 0);
        return 0;
    }
}

void handle_client(int client_fd) {
    std::ofstream out(log_file, std::ios::binary | std::ios::app);

    while (!shutdown_requested.load()) {
        uint32_t len;
        uint8_t bytes[256];
        if (recv(client_fd, bytes, 1, MSG_WAITALL) != 1) break;
        if ( bytes[0]!= 0)
        {
            int idx = 1;
            while (idx < (int)sizeof(bytes) - 1) {
            ssize_t r = recv(client_fd, &bytes[idx], 1, MSG_DONTWAIT);
            if (r == 1) {
                if (bytes[idx] == '\n') break;
                idx++;
            }
            }        
            bytes[idx + 1] = 0;  // null-terminate just in case
            auto rc  = process_message (client_fd, bytes, idx+1);
            if (rc < 0)
            {
                break;
            }
        }
        else
        {
            if (recv(client_fd, bytes, 3, MSG_WAITALL) != 3) break;

            if (recv(client_fd, &len, sizeof(len), MSG_WAITALL) != sizeof(len)) break;
            std::cout << " received len " << len << std::endl;

            std::vector<uint8_t> buf(len);
            uint8_t* rbuf = buf.data();
            LogHeader* log = reinterpret_cast<LogHeader*>(buf.data());
            log->packet_hdr  = 0;  // log packet marker
            log->packet_size = len;
            len -= sizeof(uint32_t) * 2;

            if(len > 32000) break;
            if (recv(client_fd, &rbuf[sizeof(uint32_t) * 2], len, MSG_WAITALL) != (ssize_t)len) break;
            std::cout << " received buf " << buf.size() << std::endl;
            
            //DataPacket pkt = deserialize_packet(buf);

            // Log to file
            {
                std::lock_guard<std::mutex> lg(file_lock);
//                out.write(reinterpret_cast<char*>(&len), sizeof(len));
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

    }

    close(client_fd);
    std::cout << "Client disconnected\n";
}

void broadcast_to_clients(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    auto it = clients.begin();
    while (it != clients.end()) {
        int sent = send(it->fd, data, len, 0);
        if (sent <= 0) {
            std::cerr << "Removing disconnected client fd " << it->fd << "\n";
            close(it->fd);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

int server_fd = -1;
void server_thread_fn(int server_fd) {
    while (!shutdown_requested.load()) {
        struct pollfd pfd;
        pfd.fd = server_fd;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 500);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        if (ret == 0) continue;

        if (pfd.revents & POLLIN) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

            if (client_fd >= 0) {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.push_back({client_fd, client_addr});
                std::cout << "Client connected: fd " << client_fd << "\n";
            }
        }
    }
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

    sim_memory = new uint16_t[MEM_SIZE]();
    ref_memory = new uint16_t[MEM_SIZE]();
    // sim_memory = (uint16_t*)malloc(MEM_SIZE);
    // ref_memory = (uint16_t*)malloc(MEM_SIZE);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    std::thread server_thread;
    if(0)
        server_thread = std::thread(server_thread_fn, server_fd);
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


    myfd = connect_to_input_socket("127.0.0.1", port);

    if (myfd < 0) {
        std::cerr << "Failed to connect to input socket\n";
    } else {
        // You can now send log packets, queries, etc.
    }

    std::thread flusher(periodic_flusher);

    sim_running = true;
    std::cout << "start [simulator]\n";
    sim_thread = std::thread(run_simulator);


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

if (0)server_thread.join();

// // Clean up remaining clients
    if(0) {
     std::lock_guard<std::mutex> lock(clients_mutex);
     for (auto& client : clients) {
         close(client.fd);
     }
     clients.clear();
    }
    std::cout << "Waiting for sim to complete...\n";

    if (sim_running.load()) {
        sim_running.store(false);
    }
    if (sim_thread.joinable()) sim_thread.join();

    std::cout << "Waiting for flusher to complete...\n";
    flusher.join();
    //flush_image_to_disk();
    close(server_fd);
    poll(nullptr, 0, 1000);
    std::cout << "Logger shut down cleanly.\n";

    return 0;
}
