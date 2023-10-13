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