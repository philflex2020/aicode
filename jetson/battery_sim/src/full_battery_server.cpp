#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include "battery_sim.h"

#include <fcntl.h>      // O_CREAT, O_RDWR
#include <sys/mman.h>   // mmap
#include <unistd.h>     // ftruncate
#include <cstring>
#include <cerrno>

#define SHM_NAME "/battery_sim_shm"
#define SHM_SIZE sizeof(BatterySim)

using json = nlohmann::json;

BatterySim* g_battery_sim = nullptr;


bool setup_shared_memory() {
    int fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        std::cerr << "❌ shm_open failed: " << strerror(errno) << "\n";
        return false;
    }

    if (ftruncate(fd, SHM_SIZE) == -1) {
        std::cerr << "❌ ftruncate failed: " << strerror(errno) << "\n";
        close(fd);
        return false;
    }

    void* ptr = mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "❌ mmap failed: " << strerror(errno) << "\n";
        close(fd);
        return false;
    }

    g_battery_sim = static_cast<BatterySim*>(ptr);
    close(fd);  // Can close, still mapped
    return true;
}

int main() {
    // Create a static dummy battery simulation instance

    if (!setup_shared_memory()) {
        return 1;  // Bail if shm setup failed
    }

    if (g_battery_sim->racks[0].modules[0].cells[0].voltage == 0.0f) {
        BatterySim *dummy_sim = g_battery_sim; 
    // g_battery_sim = &dummy_sim;

        std::cout << "filling in dummy data ...\n";
        // Fill in dummy data for testing
        for (int r = 0; r < NUM_RACKS; ++r) {
            for (int m = 0; m < NUM_MODULES; ++m) {
                for (int c = 0; c < NUM_CELLS; ++c) {
                    auto &cell = dummy_sim->racks[r].modules[m].cells[c];
                    cell.voltage = 3.6f + 0.01f * c;
                    cell.current = 1.2f;
                    cell.temperature = 25.0f + r;
                    cell.soc = 90.0f - m;
                    cell.resistance = 0.01f * (1 + c);
                }
            }
        }
    }
    std::cout << "Starting Battery WebSocket Server...\n";

    struct PerSocketData {}; // required for uWS::WebSocketBehavior<UserData>

    uWS::App().ws<PerSocketData>("/*", {
        .message = [](auto* ws, std::string_view message, uWS::OpCode opCode) {
            try {
                auto j = json::parse(message);

                if (j["action"] == "get") {
                    int rack = j.value("rack", 0);
                    int module = j.value("module", 0);
                    std::cout << "Received message: " << message << std::endl;
                    if (!g_battery_sim) {
                        ws->send("{\"error\":\"Battery simulation not initialized\"}", opCode);
                        return;
                    }

                    if (rack >= NUM_RACKS || module >= NUM_MODULES) {
                        ws->send("{\"error\":\"Invalid rack or module\"}", opCode);
                        return;
                    }

                    json reply;
                    for (int i = 0; i < NUM_CELLS; ++i) {
                        const auto& cell = g_battery_sim->racks[rack].modules[module].cells[i];
                        reply["cells"].push_back({
                            {"index", i},
                            {"voltage", cell.voltage},
                            {"current", cell.current},
                            {"temperature", cell.temperature},
                            {"soc", cell.soc},
                            {"resistance", cell.resistance}
                        });
                    }

                    ws->send(reply.dump(), opCode);
                }
                else if (j["action"] == "set") {
                    std::cout << "Received message: " << message << std::endl;
                    int rack    = j.value("rack", 0);
                    int module  = j.value("module", 0);
                    int cell    = j.value("cell", 0);
                    std::string field = j.value("field", "");
                    float value = j.value("value", 0.0f);
                
                    if (rack >= NUM_RACKS || module >= NUM_MODULES || cell >= NUM_CELLS) {
                        ws->send("{\"error\":\"Invalid rack/module/cell index\"}", opCode);
                        return;
                    }
                
                    auto& c = g_battery_sim->racks[rack].modules[module].cells[cell];
                
                    if (field == "voltage") {
                        c.voltage = value;
                    } else if (field == "temperature") {
                        c.temperature = value;
                    } else if (field == "soc") {
                        c.soc = value;
                    } else if (field == "resistance") {
                        c.resistance = value;
                    } else {
                        ws->send("{\"error\":\"Unknown field\"}", opCode);
                        return;
                    }
                
                    // Optionally echo confirmation
                    ws->send("{\"status\":\"ok\"}", opCode);
                }
                
            } catch (const std::exception& e) {
                ws->send(std::string("{\"error\":\"") + e.what() + "\"}", opCode);
            }
        }
    }).listen(9001, [](auto *token) {
        if (token) {
            std::cout << "✅ Battery server listening on port 9001\n";
        } else {
            std::cerr << "❌ Failed to listen on port 9001\n";
        }
    }).run();

    return 0;
}





// #include <uwebsockets/App.h>
// #include <iostream>
// #include <nlohmann/json.hpp>
// #include "battery_sim.h"

// using json = nlohmann::json;

// BatterySim* g_battery_sim = nullptr; // Will be shared memory pointer

// int main() {
//     std::cout << "Starting Battery WebSocket Server...\n";

//     uWS::App().ws("/*", {
//         .message = [](uWS::WebSocket<false, true>* ws, std::string_view message, uWS::OpCode opCode) {
//             try {
//                 auto j = json::parse(message);

//                 if (j["action"] == "get") {
//                     int rack = j.value("rack", 0);
//                     int module = j.value("module", 0);

//                     if (!g_battery_sim) {
//                         ws->send("{\"error\":\"Battery simulation not initialized\"}", opCode);
//                         return;
//                     }

//                     if (rack >= NUM_RACKS || module >= NUM_MODULES) {
//                         ws->send("{\"error\":\"Invalid rack or module\"}", opCode);
//                         return;
//                     }

//                     json reply;
//                     for (int i = 0; i < NUM_CELLS; i++) {
//                         const auto& cell = g_battery_sim->racks[rack].modules[module].cells[i];
//                         reply["cells"].push_back({
//                             {"index", i},
//                             {"voltage", cell.voltage},
//                             {"current", cell.current},
//                             {"temperature", cell.temperature},
//                             {"soc", cell.soc},
//                             {"resistance", cell.resistance}
//                         });
//                     }

//                     ws->send(reply.dump(), opCode);
//                 }
//             } catch (const std::exception& e) {
//                 ws->send(std::string("{\"error\":\"") + e.what() + "\"}", opCode);
//             }
//         }
//     }).listen(9001, [](auto *token) {
//         if (token) {
//             std::cout << "✅ Battery server listening on port 9001\n";
//         } else {
//             std::cerr << "❌ Failed to listen on port 9001\n";
//         }
//     }).run();

//     return 0;
// }

// int main() {
//     std::cout << "Starting Battery WebSocket Server...\n";

//     uWS::App().ws("/*", {
//         .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
//             try {
//                 auto j = json::parse(message);

//                 if (j["action"] == "get") {
//                     int rack = j.value("rack", 0);
//                     int module = j.value("module", 0);

//                     if (!g_battery_sim) {
//                         ws->send("{\"error\":\"Battery simulation not initialized\"}", opCode);
//                         return;
//                     }

//                     if (rack >= NUM_RACKS || module >= NUM_MODULES) {
//                         ws->send("{\"error\":\"Invalid rack or module\"}", opCode);
//                         return;
//                     }

//                     json reply;
//                     for (int i = 0; i < NUM_CELLS; i++) {
//                         const auto& cell = g_battery_sim->racks[rack].modules[module].cells[i];
//                         reply["cells"].push_back({
//                             {"index", i},
//                             {"voltage", cell.voltage},
//                             {"current", cell.current},
//                             {"temperature", cell.temperature},
//                             {"soc", cell.soc},
//                             {"resistance", cell.resistance}
//                         });
//                     }

//                     ws->send(reply.dump(), opCode);
//                 }

//             } catch (const std::exception& e) {
//                 ws->send(std::string("{\"error\":\"") + e.what() + "\"}", opCode);
//             }
//         }
//     }).listen(9001, [](auto *token) {
//         if (token) {
//             std::cout << "✅ Battery server listening on port 9001\n";
//         } else {
//             std::cerr << "❌ Failed to listen on port 9001\n";
//         }
//     }).run();

//     return 0;
// }



// #include <uwebsockets/App.h>
// #include <iostream>
// #include <nlohmann/json.hpp>
// #include "battery_sim.h"

// using json = nlohmann::json;

// BatterySim* g_battery_sim = nullptr; // Point to shared memory

// int main() {
//     std::cout << "Starting Battery WebSocket Server...\n";

//     uWS::WebSocketBehavior<false> behavior;
//     behavior.message = [](uWS::WebSocket<false, true>* ws, std::string_view message, uWS::OpCode opCode) {
//         try {
//             auto j = json::parse(message);

//             if (j["action"] == "get") {
//                 int rack = j.value("rack", 0);
//                 int module = j.value("module", 0);

//                 if (!g_battery_sim) {
//                     ws->send("{\"error\":\"Battery simulation not initialized\"}", opCode);
//                     return;
//                 }

//                 if (rack >= NUM_RACKS || module >= NUM_MODULES) {
//                     ws->send("{\"error\":\"Invalid rack or module\"}", opCode);
//                     return;
//                 }

//                 json reply;
//                 for (int i = 0; i < NUM_CELLS; i++) {
//                     const auto& cell = g_battery_sim->racks[rack].modules[module].cells[i];
//                     reply["cells"].push_back({
//                         {"index", i},
//                         {"voltage", cell.voltage},
//                         {"current", cell.current},
//                         {"temperature", cell.temperature},
//                         {"soc", cell.soc},
//                         {"resistance", cell.resistance}
//                     });
//                 }

//                 ws->send(reply.dump(), opCode);
//             }

//         } catch (const std::exception& e) {
//             ws->send(std::string("{\"error\":\"") + e.what() + "\"}", opCode);
//         }
//     };

//     uWS::App()
//         .ws<false>("/*", behavior)
//         .listen(9001, [](auto *token) {
//             if (token) {
//                 std::cout << "✅ Battery server listening on port 9001\n";
//             } else {
//                 std::cerr << "❌ Failed to listen on port 9001\n";
//             }
//         })
//         .run();

//     return 0;
// }


// #include <uwebsockets/App.h>
// #include <iostream>
// #include <nlohmann/json.hpp>
// #include "battery_sim.h"

// using json = nlohmann::json;

// BatterySim* g_battery_sim = nullptr; // point to shared memory later

// int main() {
//     std::cout << "Starting Battery WebSocket Server...\n";

//     uWS::App().ws<false>("/*", uWS::WebSocketBehavior<false>{
//         .message = [](uWS::WebSocket<false, true>* ws, std::string_view message, uWS::OpCode opCode) {
//             try {
//                 auto j = json::parse(message);
//                 if (j["action"] == "get") {
//                     int rack = j.value("rack", 0);
//                     int module = j.value("module", 0);

//                     if (rack >= NUM_RACKS || module >= NUM_MODULES) {
//                         ws->send("{\"error\":\"Invalid rack or module\"}", opCode);
//                         return;
//                     }

//                     json reply;
//                     for (int i = 0; i < NUM_CELLS; i++) {
//                         const auto& cell = g_battery_sim->racks[rack].modules[module].cells[i];
//                         reply["cells"].push_back({
//                             {"index", i},
//                             {"voltage", cell.voltage},
//                             {"current", cell.current},
//                             {"temperature", cell.temperature},
//                             {"soc", cell.soc},
//                             {"resistance", cell.resistance}
//                         });
//                     }

//                     ws->send(reply.dump(), opCode);
//                 }
//             } catch (const std::exception& e) {
//                 ws->send(std::string("{\"error\":\"") + e.what() + "\"}", opCode);
//             }
//         }
//     }).listen(9001, [](auto *token) {
//         if (token) {
//             std::cout << "✅ Battery server listening on port 9001\n";
//         } else {
//             std::cerr << "❌ Failed to listen on port 9001\n";
//         }
//     }).run();
// }
