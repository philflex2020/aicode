
#include <iostream>
#include <string>
#include <thread>
#include <map>
#include <functional>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <dlfcn.h>
#include "smObj.h" // Assuming this header defines the smObj structure

#define PORT 8080

//g++ -std=c++17 -pthread -I../inc -o server server.cpp

// Function to send a response to the client
void send_response(int client_socket, const std::string& response) {
    send(client_socket, response.c_str(), response.size(), 0);
}

// Command: HELP
void command_help(int client_socket, std::shared_ptr<smObj> obj, const std::string& args,
    const std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map) {
    std::string response = "Available Commands:\n";
    for (const auto& [command, _] : command_map) {
        response += " - [" + command + "]\n";
    }
    send_response(client_socket, response);
}

// Command: GET
void command_get(int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
    if (obj->attributes.count(args)) {
        int value = obj->attributes[args];
        send_response(client_socket, "Value: " + std::to_string(value) + "\n");
    } else {
        send_response(client_socket, "Error: Attribute not found\n");
    }
}

// Command: SET
void command_set(int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
    size_t pos = args.find(' ');
    if (pos != std::string::npos) {
        std::string attr = args.substr(0, pos);
        int value = std::stoi(args.substr(pos + 1));
        obj->attributes[attr] = value;
        send_response(client_socket, "Attribute set successfully\n");
    } else {
        send_response(client_socket, "Error: Invalid SET command format\n");
    }
}

// Command: DEFINE
void command_define(int client_socket, std::shared_ptr<smObj> obj, const std::string& func_name,
                    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map) {
    std::string so_path = "./obj/" + func_name + ".so";
    std::string cpp_path = "./src/" + func_name + ".cpp";

    if (std::filesystem::exists(so_path)) {
        send_response(client_socket, "Function already defined as shared object\n");
        // Add the function to the command map if it's not already there
        if (command_map.find(func_name) == command_map.end()) {
            command_map[func_name] = [so_path, func_name](int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
                void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
                if (!handle) {
                    send_response(client_socket, "Error loading shared object\n");
                    return;
                }
                using func_type = void(*)(int, std::shared_ptr<smObj>, const std::string&);
                auto func = (func_type)dlsym(handle, func_name.c_str());
                if (!func) {
                    send_response(client_socket, "Error locating function in shared object\n");
                    dlclose(handle);
                    return;
                }
                func(client_socket, obj, args);
                send_response(client_socket, "Function executed successfully\n");
                dlclose(handle);
            };
            send_response(client_socket, "Function added to the command map\n");
        }
        return;
    }

    if (std::filesystem::exists(cpp_path)) {
        send_response(client_socket, "Compiling source file...\n");
        std::string command = "g++ -shared -fPIC -I ../inc -o " + so_path + " " + cpp_path;
        if (system(command.c_str()) == 0) {
            send_response(client_socket, "Compiled and defined successfully\n");

            // Add the function to the command map
            command_map[func_name] = [so_path, func_name](int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
                void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
                if (!handle) {
                    send_response(client_socket, "Error loading shared object\n");
                    return;
                }
                using func_type = void(*)(int, std::shared_ptr<smObj>, const std::string&);
                auto func = (func_type)dlsym(handle, func_name.c_str());
                if (!func) {
                    send_response(client_socket, "Error locating function in shared object\n");
                    dlclose(handle);
                    return;
                }
                func(client_socket, obj, args);
                send_response(client_socket, "Function executed successfully\n");
                dlclose(handle);
            };
            send_response(client_socket, "Function added to the command map\n");
        } else {
            send_response(client_socket, "Error compiling source file\n");
        }
        return;
    }

    send_response(client_socket, "Function not found. Creating a template...\n");
    std::ofstream file(cpp_path);
    file << "#include <iostream>\n"
         << "#include \"smObj.h\"\n"
         << "\n"
         << "extern \"C\" void " << func_name << "(int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {\n"
         << "    // Implementation of " << func_name << "\n"
         << "    std::cout << \"Function " << func_name << " executed.\\n\";\n"
         << "}\n";
    file.close();
    send_response(client_socket, "Template created at: " + cpp_path + "\n");
}
#if 0
void xxcommand_define(int client_socket, std::shared_ptr<smObj> obj, const std::string& func_name,
                    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map) {
    std::string so_path = "./obj/" + func_name + ".so";
    std::string cpp_path = "./src/" + func_name + ".cpp";

    if (std::filesystem::exists(so_path)) {
        send_response(client_socket, "Function already defined as shared object\n");
        if (command_map.find(func_name) == command_map.end()) {
            command_map[func_name] = [so_path, func_name](int client_socket, std::shared_ptr<smObj> obj, const std::string&) {
                void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
                if (!handle) {
                    send_response(client_socket, "Error loading shared object\n");
                    return;
                }
                using func_type = void(*)(std::shared_ptr<smObj>);
                auto func = (func_type)dlsym(handle, func_name.c_str());
                if (!func) {
                    send_response(client_socket, "Error locating function in shared object\n");
                    dlclose(handle);
                    return;
                }
                func(obj);
                send_response(client_socket, "Function executed successfully\n");
                dlclose(handle);
            };
            send_response(client_socket, "Function added to the command map\n");
        }
        return;
    }

    if (std::filesystem::exists(cpp_path)) {
        send_response(client_socket, "Compiling source file...\n");
        std::string command = "g++ -shared -fPIC -I ../inc -o " + so_path + " " + cpp_path;
        if (system(command.c_str()) == 0) {
            send_response(client_socket, "Compiled and defined successfully\n");
            command_map[func_name] = [so_path, func_name](int client_socket, std::shared_ptr<smObj> obj, const std::string&) {
                void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
                if (!handle) {
                    send_response(client_socket, "Error loading shared object\n");
                    return;
                }
                using func_type = void(*)(std::shared_ptr<smObj>);
                auto func = (func_type)dlsym(handle, func_name.c_str());
                if (!func) {
                    send_response(client_socket, "Error locating function in shared object\n");
                    dlclose(handle);
                    return;
                }
                func(client_socket, obj, args);
                send_response(client_socket, "Function executed successfully\n");
                dlclose(handle);
            };
            send_response(client_socket, "Function added to the command map\n");
        } else {
            send_response(client_socket, "Error compiling source file\n");
        }
        return;
    }

    send_response(client_socket, "Function not found. Creating a template...\n");
    std::ofstream file(cpp_path);
    file << "#include <iostream>\n"
         << "#include \"smObj.h\"\n"
         << "\n"
         << "extern \"C\" void " << func_name << "(int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {\n"
         << "    // Implementation of " << func_name << "\n"
         << "    std::cout << \"Function " << func_name << " executed.\\n\";\n"
         << "}\n";
    file.close();
    send_response(client_socket, "Template created at: " + cpp_path + "\n");
    // file << "#include <iostream>\n"
    //      << "#include \"smObj.h\"\n"
    //      << "\n"
    //      << "extern \"C\" void " << func_name << "(std::shared_ptr<smObj> obj) {\n"
    //      << "    std::cout << \"Function " << func_name << " executed.\\n\";\n"
    //      << "}\n";
    // file.close();
    // send_response(client_socket, "Template created at: " + cpp_path + "\n");
}

#endif

// Register built-in commands
void register_builtin_commands(
    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map,
    std::shared_ptr<smObj> obj) {
    command_map["help"] = [&command_map](int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
        command_help(client_socket, obj, args, command_map);
    };
    command_map["get"] = command_get;
    command_map["set"] = command_set;
    command_map["define"] = [&command_map](int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
        command_define(client_socket, obj, args, command_map);
    };
}

// Handle client requests
void handle_client(int client_socket, std::shared_ptr<smObj> obj,
    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map) {
    char buffer[1024] = {0};
    while (true) {
        int valread = read(client_socket, buffer, 1024);
        if (valread <= 0) break;

        std::string command_line(buffer, valread);
        command_line.erase(command_line.find_last_not_of("\r\n") + 1);
        size_t pos = command_line.find(' ');

        std::string command = command_line.substr(0, pos);
        std::string args = (pos != std::string::npos) ? command_line.substr(pos + 1) : "";

        printf("Received command: [%s]\n", command.c_str());
        if(command_map.find(command) == command_map.end())
        {
            printf(" did not find command [%s] \n", command.c_str());
            std::string response = "Available Commands:\n";
            for (const auto& [command, _] : command_map) {
                response += " - [" + command + "]\n";
             }
             std::cout << response << std::endl;
        }
        if (command_map.count(command)) {
            command_map[command](client_socket, obj, args);
        } else {
            send_response(client_socket, "Error: Unknown command\n");
        }
    }

    close(client_socket);
}

// Start the socket server
void start_socket_server(std::shared_ptr<smObj> obj) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Socket server started on port " << PORT << std::endl;

    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>> command_map;
    register_builtin_commands(command_map, obj);

    while (true) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        std::thread client_thread(handle_client, client_socket, obj, std::ref(command_map));
        client_thread.detach();
    }
}

#if 0
#include <iostream>
#include <string>
#include <thread>
#include <map>
#include <functional>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <dlfcn.h>
#include "smObj.h" // Assuming this header defines the smObj structure

#define PORT 8080

// Function to send a response to the client
void send_response(int client_socket, const std::string& response) {
    send(client_socket, response.c_str(), response.size(), 0);
}

void register_builtin_commands(
    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map,
    std::shared_ptr<smObj> obj) {
    command_map["help"] = [&command_map](int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
        command_help(client_socket, obj, args, command_map);
    };
    command_map["get"] = command_get;
    command_map["set"] = command_set;
    command_map["define"] = [&command_map](int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
        command_define(client_socket, obj, args, command_map);
    };
}

// // Define built-in commands
// void register_builtin_commands(
//     std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map,
//     std::shared_ptr<smObj> obj);

// Command: HELP
void command_help(int client_socket, std::shared_ptr<smObj> obj, const std::string& args,
    const std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map) {
    std::string response = "Available Commands:\n";
    for (const auto& [command, _] : command_map) {
        response += " - " + command + "\n";
    }
    send_response(client_socket, response);
}

// Command: GET
void command_get(int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
    if (obj->attributes.count(args)) {
        int value = obj->attributes[args];
        send_response(client_socket, "Value: " + std::to_string(value) + "\n");
    } else {
        send_response(client_socket, "Error: Attribute not found\n");
    }
}

// Command: SET
void command_set(int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
    size_t pos = args.find(' ');
    if (pos != std::string::npos) {
        std::string attr = args.substr(0, pos);
        int value = std::stoi(args.substr(pos + 1));
        obj->attributes[attr] = value;
        send_response(client_socket, "Attribute set successfully\n");
    } else {
        send_response(client_socket, "Error: Invalid SET command format\n");
    }
}

void command_define(int client_socket, std::shared_ptr<smObj> obj, const std::string& func_name,
                    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map) {
    std::string so_path = "./obj/" + func_name + ".so";
    std::string cpp_path = "./src/" + func_name + ".cpp";

    if (std::filesystem::exists(so_path)) {
        send_response(client_socket, "Function already defined as shared object\n");
        // Add the function to the command map if it's not already there
        if (command_map.find(func_name) == command_map.end()) {
            command_map[func_name] = [so_path, func_name](int client_socket, std::shared_ptr<smObj> obj, const std::string&) {
                void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
                if (!handle) {
                    send_response(client_socket, "Error loading shared object\n");
                    return;
                }
                using func_type = void(*)(std::shared_ptr<smObj>);
                auto func = (func_type)dlsym(handle, func_name.c_str());
                if (!func) {
                    send_response(client_socket, "Error locating function in shared object\n");
                    dlclose(handle);
                    return;
                }
                func(obj);
                send_response(client_socket, "Function executed successfully\n");
                dlclose(handle);
            };
            send_response(client_socket, "Function added to the command map\n");
        }
        return;
    }

    if (std::filesystem::exists(cpp_path)) {
        send_response(client_socket, "Compiling source file...\n");
        std::string command = "g++ -shared -fPIC -I ../inc -o " + so_path + " " + cpp_path;
        if (system(command.c_str()) == 0) {
            send_response(client_socket, "Compiled and defined successfully\n");

            // Add the function to the command map
            command_map[func_name] = [so_path, func_name](int client_socket, std::shared_ptr<smObj> obj, const std::string&) {
                void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
                if (!handle) {
                    send_response(client_socket, "Error loading shared object\n");
                    return;
                }
                using func_type = void(*)(std::shared_ptr<smObj>);
                auto func = (func_type)dlsym(handle, func_name.c_str());
                if (!func) {
                    send_response(client_socket, "Error locating function in shared object\n");
                    dlclose(handle);
                    return;
                }
                func(obj);
                send_response(client_socket, "Function executed successfully\n");
                dlclose(handle);
            };
            send_response(client_socket, "Function added to the command map\n");
        } else {
            send_response(client_socket, "Error compiling source file\n");
        }
        return;
    }

    send_response(client_socket, "Function not found. Creating a template...\n");
    std::ofstream file(cpp_path);
    file << "#include <iostream>\n"
         << "#include \"smObj.h\"\n"
         << "\n"
         << "extern \"C\" void " << func_name << "(std::shared_ptr<smObj> obj) {\n"
         << "    std::cout << \"Function " << func_name << " executed.\\n\";\n"
         << "}\n";
    file.close();
    send_response(client_socket, "Template created at: " + cpp_path + "\n");
}


// Command: DEFINE
void old_command_define(int client_socket, std::shared_ptr<smObj> obj, const std::string& func_name) {
    std::string so_path = "./obj/" + func_name + ".so";
    std::string cpp_path = "./src/" + func_name + ".cpp";

    if (std::filesystem::exists(so_path)) {
        send_response(client_socket, "Function already defined as shared object\n");
        return;
    }

    if (std::filesystem::exists(cpp_path)) {
        send_response(client_socket, "Compiling source file...\n");
        std::string command = "g++ -shared -fPIC -I ../inc  -o " + so_path + " " + cpp_path;
        if (system(command.c_str()) == 0) {
            send_response(client_socket, "Compiled and defined successfully\n");
        } else {
            send_response(client_socket, "Error compiling source file\n");
        }
        return;
    }

    send_response(client_socket, "Function not found. Creating a template...\n");
    std::ofstream file(cpp_path);
    file << "#include <iostream>\n"
         << "#include \"smObj.h\"\n"
         << "\n"
         << "extern \"C\" void " << func_name << "(std::shared_ptr<smObj> obj) {\n"
         << "    std::cout << \"Function " << func_name << " executed.\\n\";\n"
         << "}\n";
    file.close();
    send_response(client_socket, "Template created at: " + cpp_path + "\n");
}

// Register built-in commands
void register_builtin_commands(
    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>>& command_map,
    std::shared_ptr<smObj> obj) {
    command_map["help"] = [&command_map](int client_socket, std::shared_ptr<smObj> obj, const std::string& args) {
        command_help(client_socket, obj, args, command_map);
    };
    command_map["get"] = command_get;
    command_map["set"] = command_set;
    command_map["define"] = command_define;
}

// Handle client requests
void handle_client(int client_socket, std::shared_ptr<smObj> obj,
    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>> command_map) {
    char buffer[1024] = {0};
    while (true) {
        int valread = read(client_socket, buffer, 1024);
        if (valread <= 0) break;

        std::string command_line(buffer, valread);
        // Trim trailing '\r' and '\n'
        command_line.erase(command_line.find_last_not_of("\r\n") + 1);
        size_t pos = command_line.find(' ');

        std::string command = command_line.substr(0, pos);
        std::string args = (pos != std::string::npos) ? command_line.substr(pos + 1) : "";

        printf("received command [%s]\n", command.c_str());

        if (command_map.count(command)) {
            command_map[command](client_socket, obj, args);
        } else {
            send_response(client_socket, "Error: Unknown command\n");
        }
    }

    close(client_socket);
}

// Start the socket server
void start_socket_server(std::shared_ptr<smObj> obj) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1; // For enabling SO_REUSEADDR and SO_REUSEPORT


    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Socket server started on port " << PORT << std::endl;

    std::map<std::string, std::function<void(int, std::shared_ptr<smObj>, const std::string&)>> command_map;
    register_builtin_commands(command_map, obj);

    while (true) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        std::thread client_thread(handle_client, client_socket, obj, command_map);
        client_thread.detach();
    }
}
#endif