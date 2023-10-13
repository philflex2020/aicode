#include "crow_all.h"
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