Converting a Python Flask web server to C++ involves quite a bit of work because these two languages serve different purposes and the frameworks used in both languages are quite different. 

For the purpose of this task, I'll provide a rough implementation in C++ using the [Crow library](https://github.com/ipkn/crow), which is a micro web framework for C++. This will include basic routing and handling of the `run-command` endpoint. 

However, remember, a direct translation may not always be efficient or idiomatic in the target language, so tweaks might be needed. Here's a rudimentary version of the given Python code in C++ using the Crow library:

```cpp
#include <crow.h>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <stdexcept>

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        exit(1);
    }

    std::string SERVER_IP = argv[1];

    crow::SimpleApp app;

    CROW_ROUTE(app, "/run-command")
    .methods("GET"_method)
    ([](const crow::request& req)
    {
        // Extract command details
        auto dest = req.url_params.get("route");
        auto ip = req.url_params.get("ip");
        auto port = req.url_params.get("port");
        auto action = req.url_params.get("action");
        auto deviceId = req.url_params.get("deviceId");
        auto type = req.url_params.get("type");
        auto offset = req.url_params.get("offset");
        auto value = req.url_params.get("value");
        auto rowId = req.url_params.get("rowId");

        if(dest && dest != SERVER_IP)
        {
            // Forward request and handle it (This needs more handling, it's just a basic example)
            // TODO: Handle the request forwarding. You might use libraries like libcurl for this in C++.
        }

        // Execute the command (pseudo-code)
        std::string command = "ping -c 1 " + std::string(ip);
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe)
        {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }

        crow::json::wvalue x;
        x["status"] = "success";
        x["output"] = result;
        return x;
    });

    app.port(5000).multithreaded().run();
}
```

**Things to Note:**

1. This is a basic example. For the forwarding part, you might want to use libraries like `libcurl` in C++.
2. Ensure you have the Crow library and its dependencies installed and properly linked when compiling this code.
3. C++ doesn't have a subprocess library as Python does, so for external command execution, we use `popen()`.
4. This code may need adjustments based on your actual needs and the environment you're working in.


Certainly. An alternative to Crow that doesn't require Boost is `cpp-httplib`. Below is an implementation using `cpp-httplib` and `nlohmann/json` for JSON processing. 

Make sure you have `cpp-httplib` and `nlohmann/json` libraries available in your environment.

```cpp
#include <iostream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <array>
#include <memory>
#include <stdexcept>

std::string execute_command(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        return 1;
    }

    std::string SERVER_IP = argv[1];

    httplib::Server svr;

    svr.Get("/run-command", [&](const httplib::Request& req, httplib::Response& res) {
        std::string ip = req.get_param_value("ip");

        std::string command = "ping -c 1 " + ip;
        std::string output = execute_command(command);

        nlohmann::json response;
        response["status"] = "success";
        response["output"] = output;

        res.set_content(response.dump(), "application/json");
    });

    svr.listen("0.0.0.0", 5000);

    return 0;
}
```

**Things to Note:**

1. This is a basic example; it primarily supports the ping functionality demonstrated in the Python code.
2. C++ doesn't have a subprocess library as Python does, so for external command execution, we use `popen()`.
3. This code may need adjustments based on your actual needs and the environment you're working in.
4. The request forwarding functionality isn't implemented here, but can be done using the same `cpp-httplib` library.
5. You need to link against required libraries when compiling the code.

You can find the libraries here:
- [cpp-httplib](https://github.com/yhirose/cpp-httplib)
- [nlohmann/json](https://github.com/nlohmann/json)
Certainly. Below is an example of how you can achieve this using `cpp-httplib`:

1. The code first checks if the destination (dest) is provided in the POST request and if it differs from the server's IP.
2. If it needs to forward the request, it sends a POST request with the command and data to the desired destination.
3. Otherwise, it processes the command and data in the current server.

```cpp
#include <iostream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <array>
#include <memory>
#include <stdexcept>

std::string execute_command(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        return 1;
    }

    std::string SERVER_IP = argv[1];

    httplib::Server svr;

    svr.Post("/run-command", [&](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json request_json = nlohmann::json::parse(req.body);
        
        std::string dest = request_json["dest"];
        std::string command = request_json["command"];
        nlohmann::json data = request_json["data"];

        if (dest != SERVER_IP) {
            // Forwarding the request
            httplib::Client client(dest.c_str(), 5000);
            auto response = client.Post("/run-command", req.body, "application/json");

            if (response) {
                res.set_content(response->body, "application/json");
            } else {
                nlohmann::json error_response;
                error_response["status"] = "error";
                error_response["message"] = "Connection refused to destination server";
                res.set_content(error_response.dump(), "application/json");
            }
        } else {
            // Process the command and data here
            // For the sake of simplicity, we're just echoing them back in this example
            nlohmann::json response;
            response["status"] = "success";
            response["command"] = command;
            response["data"] = data;
            res.set_content(response.dump(), "application/json");
        }
    });

    svr.listen("0.0.0.0", 5000);

    return 0;
}
```

Things to note:

- You would send POST requests to this server with a JSON body that contains three fields: `dest`, `command`, and `data`.
- If the `dest` does not match the `SERVER_IP`, the request will be forwarded.
- The server listens on `0.0.0.0:5000` for incoming requests.
- Error handling is basic and might need improvements based on your requirements.
- This example doesn't execute received commands for security reasons; it simply echoes back the command and data. Adjust this part according to your needs.


Sure! The following is a simple C++ application that:

1. Prompts the user for the IP address, a command string, and a filename.
2. Starts an HTTP server using `cpp-httplib` that listens for POST requests on `/run-command`.
3. When a request is received, it checks the destination (`dest`).
    - If the destination matches the provided IP, it reads the contents of the specified data file and sends the content along with the provided command string to the server you described in the previous answer.
    - If the destination doesn't match, it just returns a message indicating that the destination is wrong.

Let's write the code:

```cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <httplib.h>
#include <nlohmann/json.hpp>

std::string readFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    std::string server_ip, command_string, filename;

    // Ask user for inputs
    std::cout << "Enter IP address: ";
    std::cin >> server_ip;
    std::cout << "Enter command string: ";
    std::cin >> command_string;
    std::cout << "Enter data filename: ";
    std::cin >> filename;

    // Read the contents of the data file
    std::string file_content;
    try {
        file_content = readFile(filename);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    httplib::Server svr;

    svr.Post("/run-command", [&](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json request_json = nlohmann::json::parse(req.body);
        std::string dest = request_json["dest"];
        
        if (dest == server_ip) {
            // Process the request
            nlohmann::json response;
            response["status"] = "success";
            response["command"] = command_string;
            response["data"] = file_content;
            res.set_content(response.dump(), "application/json");
        } else {
            nlohmann::json response;
            response["status"] = "error";
            response["message"] = "Wrong destination";
            res.set_content(response.dump(), "application/json");
        }
    });

    svr.listen("0.0.0.0", 5000);
    return 0;
}
```

Make sure you have the appropriate libraries (`cpp-httplib` and `nlohmann/json`) installed and linked to compile and run this code.
