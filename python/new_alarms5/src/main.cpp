#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <dirent.h>
//git clone https://github.com/eidheim/Simple-WebSocket-Server.git
// not sure about this lot #include "client_ws.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

using json = nlohmann::json;


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>


// Per-socket user data (empty for now)
struct PerSocketData {};
// Thread-safe in-memory alarm config
struct TargetConfig {
    std::string ip;
    int port;
    bool secure = false;
    std::mutex mtx;
};

TargetConfig targetConfig;

// Thread-safe in-memory alarm config
struct AlarmConfig {
    std::string alarm_definitions;
    std::string alarm_level_actions;
    std::string limits_values;
    std::string limits_def;
    std::mutex mtx;
};

// Thread-safe in-memory alarm config Json representation
struct AlarmConfigJson {
    json alarm_definitions = json::array();
    json alarm_level_actions = json::array();
    json limits_values = json::array();
    json limits_def = json::array();
    std::mutex mtx;
} alarmConfigJson;

AlarmConfig alarmConfig;
 

int ws_test() {
    try {
        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        websocket::stream<tcp::socket> ws(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(targetConfig.ip, std::to_string(targetConfig.port));

        // Make the connection on the IP address we get from a lookup
        net::connect(ws.next_layer(), results.begin(), results.end());

        // Perform the WebSocket handshake
        ws.handshake(targetConfig.ip, "/");

        // Send the message
        std::string message = "tables";
        ws.write(net::buffer(std::string(message)));

        // This buffer will hold the incoming message
        beast::flat_buffer buffer;

        // Read a message into our buffer
        ws.read(buffer);

        // Print the message
        std::cout << " response to tables request" << std::endl;
        std::cout << beast::make_printable(buffer.data()) << std::endl;

        // Close the WebSocket connection
        ws.close(websocket::close_code::normal);
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// #include <boost/beast/websocket.hpp>
// #include <boost/asio.hpp>
// #include <boost/beast/core.hpp>
// #include <string>
// #include <iostream>

// using tcp = boost::asio::ip::tcp;
// namespace websocket = boost::beast::websocket;
// namespace net = boost::asio;

json sendTargetRequest(const std::string& ip, int port, const json& requestJson) {
    try {
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        websocket::stream<tcp::socket> ws(ioc);

        auto const results = resolver.resolve(ip, std::to_string(port));
        net::connect(ws.next_layer(), results.begin(), results.end());
        ws.handshake(ip, "/");

        std::string message = requestJson.dump();
        ws.write(net::buffer(message));

        boost::beast::flat_buffer buffer;
        ws.read(buffer);

        std::string responseStr = boost::beast::buffers_to_string(buffer.data());
        ws.close(websocket::close_code::normal);

        return json::parse(responseStr);
    } catch (const std::exception& e) {
        std::cerr << "WebSocket error: " << e.what() << std::endl;
        return json{{"error", std::string("WebSocket error: ") + e.what()}};
    }
}
// Read file content into string
std::string readFileToString(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Warning: Could not open file: " << filePath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

    // Containers for split parts
    json alarmDefs = json::array();
    json alarmLevelActions = json::array();
    json limitsValues = json::array();
    json limitsDef = json::object();

bool loadAlarmConfig(AlarmConfig& config ,const std::string& filename = "json/alarm_definition.json") {

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return false;
    }

    json data;
    file >> data;


    // Track created limits to avoid duplicates
    std::set<std::string> createdLimits;

    if (data.contains("alarms") && data["alarms"].is_array()) {
        for (const auto& alarm : data["alarms"]) {
            // Build alarm_definitions entry
            json alarmDefEntry = {
                {"alarm_num", alarm.value("num", 0)},
                {"alarm_name", alarm.value("name", "")},
                {"num_levels", alarm.value("levels", 1)},
                {"measured_variable", alarm.value("measured", "") == "none" ? "" : alarm.value("measured", "")},
                {"limits_structure", alarm.value("limits_var", "").empty() ? 
                    (alarm.value("name", "").empty() ? "" : 
                        alarm.value("name", "")) : alarm.value("limits_var", "")},
                {"limits_def", alarm.value("limits_def", "")},
                {"comparison_type", alarm.value("compare", "greater_than")},
                {"alarm_variable", alarm.value("alarm_var", "")},
                {"latched_variable", alarm.value("latch_var", "")},
                {"notes", alarm.value("notes", "")}
            };
            alarmDefs.push_back(alarmDefEntry);

            // Build limits_values entry if not already created
            std::string limitsStructure = alarmDefEntry["limits_structure"];
            if (!limitsStructure.empty() && createdLimits.find(limitsStructure) == createdLimits.end()) {
                auto limitsArray = alarm.value("limits", std::vector<double>{0,0,0,0});
                json limitsEntry = {
                    {"limits_structure", limitsStructure},
                    {"level1_limit", limitsArray.size() > 0 ? limitsArray[0] : 0},
                    {"level2_limit", limitsArray.size() > 1 ? limitsArray[1] : 0},
                    {"level3_limit", limitsArray.size() > 2 ? limitsArray[2] : 0},
                    {"hysteresis", limitsArray.size() > 3 ? limitsArray[3] : 0},
                    {"last_updated", ""},  // You can add timestamp here if needed
                    {"notes", ""}
                };
                limitsValues.push_back(limitsEntry);
                createdLimits.insert(limitsStructure);
            }

            // Build alarm_level_actions entries
            if (alarm.contains("actions") && alarm["actions"].is_array()) {
                for (const auto& action : alarm["actions"]) {
                    json actionEntry = {
                        {"alarm_num", alarm.value("num", 0)},
                        {"alarm_name", alarm.value("name", "")},
                        {"level", action.value("level", 1)},
                        {"enabled", action.value("level_enabled", true)},
                        {"duration", action.value("duration", "0:0")},
                        {"actions", action["actions"].dump()},
                        {"notes", action.value("notes", "")}
                    };
                    alarmLevelActions.push_back(actionEntry);
                }
            }
        }
    }

    // limits_def can be empty or filled if you have data for it
    limitsDef = json::array();
    if (data.contains("limits_def") && data["limits_def"].is_array()) {
        limitsDef = data["limits_def"];  
        for (const auto& def : data["limits_def"]) {
            std::cout << "limits_def table: " << def.value("table", "") << std::endl;
        }
    }
    // Convert to strings for serving
    config.alarm_definitions = alarmDefs.dump();
    config.alarm_level_actions = alarmLevelActions.dump();
    config.limits_values = limitsValues.dump();
    config.limits_def = limitsDef.dump();
    std::cout << "Alarm Config limits_def " << config.limits_def << std::endl;

    return true;
}

// Load alarm definitions from file or compiled string
void loadAlarmDefinitions(bool useCompiledString = false) {
    std::string jsonStr;

    if (useCompiledString) {
        const char* compiledJson = R"json(
        {
            "alarm_definitions": [
                {
                    "alarm_num": 1,
                    "name": "Example Alarm",
                    "description": "An example alarm definition"
                }
            ],
            "alarm_level_actions": [],
            "limits_values": [],
            "limits_def": []
        }
        )json";
        jsonStr = compiledJson;
    } else {
        jsonStr = readFileToString("json/alarm_definition.json");
        if (jsonStr.empty()) {
            std::cerr << "No alarm_definition.json loaded, starting with empty config." << std::endl;
            return;
        }
    }

    try {
        auto parsed = json::parse(jsonStr);
        std::lock_guard<std::mutex> lock(alarmConfigJson.mtx);
        alarmConfigJson.alarm_definitions = parsed.value("alarm_definitions", json::array());
        alarmConfigJson.alarm_level_actions = parsed.value("alarm_level_actions", json::array());
        alarmConfigJson.limits_values = parsed.value("limits_values", json::array());
        alarmConfigJson.limits_def = parsed.value("limits_def", json::array());
        std::cout << "Alarm definitions loaded successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse alarm_definition.json: " << e.what() << std::endl;
    }
}

// Send JSON message over WebSocket
void sendJsonMessage(uWS::WebSocket< false, true,PerSocketData>* ws, const json& msg) {
    std::string s = msg.dump();
    ws->send(s, uWS::OpCode::TEXT);
}

// Handle incoming WebSocket JSON messages
void handleMessage(uWS::WebSocket<false, true, PerSocketData>* ws, std::string_view message) {
    try {
        auto msg = json::parse(message);

        if (!msg.contains("type")) {
            sendJsonMessage(ws, {{"error", "Missing 'type' field"}});
            return;
        }

        std::string type = msg["type"];

        if (type == "get_config") {
            std::lock_guard<std::mutex> lock(alarmConfigJson.mtx);
            json response = {
                {"type", "config"},
                {"alarm_definitions", alarmConfigJson.alarm_definitions},
                {"alarm_level_actions", alarmConfigJson.alarm_level_actions},
                {"limits_values", alarmConfigJson.limits_values},
                {"limits_def", alarmConfigJson.limits_def}
            };
            sendJsonMessage(ws, response);
        } else {
            sendJsonMessage(ws, {{"error", "Unknown message type: " + type}});
        }
    } catch (const std::exception& e) {
        sendJsonMessage(ws, {{"error", std::string("Exception: ") + e.what()}});
    }
}

int main(int argc, char* argv[]) {
    targetConfig.ip = "192.168.86.209";
    targetConfig.port = 9001;
    //std::string bindIp = "127.0.0.1";
    std::string bindIp = "0.0.0.0";
    int port = 9100;

    if (argc > 1) {
        bindIp = argv[1];
    }

    ws_test();

    std::cout << "Starting server on " << bindIp << ":" << port << std::endl;

    loadAlarmDefinitions(false);
    std::cout << "Alarm Defs loaded " << std::endl;
    loadAlarmConfig(alarmConfig);
    std::cout << "Alarm Config loaded " << std::endl;

    std::string indexHtml = readFileToString("static/index.html");
    if (indexHtml.empty()) {
        std::cerr << "Warning: index.html not found or empty. HTTP requests will return 404." << std::endl;
    }

    // Define WebSocket behavior
    uWS::TemplatedApp<false>::WebSocketBehavior<PerSocketData> wsBehavior;
    //uWS::WebSocketBehavior<PerSocketData> wsBehavior;

// struct MyWebSocketBehavior {
//     std::function<void(uWS::WebSocket<true, false, PerSocketData>*)> open;
//     std::function<void(uWS::WebSocket<true, false, PerSocketData>*, std::string_view, uWS::OpCode)> message;
//     std::function<void(uWS::WebSocket<true, false, PerSocketData>*, int, std::string_view)> close;
// };

// MyWebSocketBehavior wsBehavior;

    wsBehavior.open = [](uWS::WebSocket<false, true, PerSocketData>* ws) {
        std::cout << "Client connected\n";
    };

    wsBehavior.message = [](uWS::WebSocket<false, true, PerSocketData>* ws, std::string_view message, uWS::OpCode opCode) {
        if (opCode == uWS::OpCode::TEXT) {
            handleMessage(ws, message);
        }
    };

    wsBehavior.close = [](uWS::WebSocket<false, true, PerSocketData>* ws, int code, std::string_view message) {
        std::cout << "Client disconnected\n";
    };
    uWS::App()
        .ws<PerSocketData>("/*", std::move(wsBehavior))
        // HTTP GET handler for /api/alarms/config
        .get("/api/alarms/config", [](auto *res, auto *req) {
            // Set content-type header to application/json
            res->writeHeader("content-type", "application/json");
            json response = json::object();

            {
        //std::lock_guard<std::mutex> lock(alarmConfigJson.mtx);
            response["alarm_definitions"]       = alarmDefs;
            response["alarm_level_actions"]     = alarmLevelActions;
            response["limits_def"]              = limitsDef;
            response["limits_values"]           = limitsValues;
            }
            // sendJsonMessage(ws, response);
            std::string s = response.dump();
            res->end(s);
        })
//        .post("/api/alarms/target/get", [](auto *res, auto *req) {
//            {sm_name: "rtos", reg_type: "mb_hold", offset: "at_total_v_over", num: 4}
//        .post("/api/alarms/target/set", [](auto *res, auto *req) {
//            {sm_name: "rtos", reg_type: "mb_hold", offset: "at_total_v_over",…}


        .get("/api/alarms/target/config", [](auto *res, auto *req) {
            // Set content-type header to application/json
            res->writeHeader("content-type", "application/json");
            json response = json::object();
            {
                std::lock_guard<std::mutex> lock(targetConfig.mtx);
                response["ip"] = targetConfig.ip;
                response["port"] = targetConfig.port;
                response["secure"] = targetConfig.secure;
            }
            std::string s = response.dump();
            res->end(s);
        })

        .get("/api/pub_defs/list", [](auto *res, auto *req) {
            // Set content-type header to application/json
            res->writeHeader("content-type", "application/json");
            json response = json::object();
            // Get a list of files in the pub_defs directory
            json files = json::array();
            
            // Using dirent.h to list files
            DIR* dir = opendir("var_lists");
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string filename = entry->d_name;
                    // Skip directories and only include .json files
                    if (entry->d_type == DT_REG && filename.length() > 5 && 
                        filename.substr(filename.length() - 5) == ".json") {
                        files.push_back(filename);
                    }
                }
                closedir(dir);
            }
            
            response["files"] = files;
            
            std::string s = response.dump();
            res->end(s);
        })
        .get("/*", [indexHtml](auto* res, auto* req) {
            if (indexHtml.empty()) {
                res->writeStatus("404 Not Found")->end("index.html not found");
                return;
            }
            res->writeHeader("Content-Type", "text/html; charset=utf-8");
            res->end(indexHtml);
        })

        // .post("/api/alarms/target/get", [](auto *res, auto *req) {
        //      //{sm_name: "rtos", reg_type: "mb_hold", offset: "at_total_v_over", num: 4}
        //     // Buffer to accumulate body data
        //     std::shared_ptr<std::string> body = std::make_shared<std::string>();
        //     // Handle client abort
        //     res->onAborted([body]() {
        //         // Clear buffer or do cleanup if needed
        //         body->clear();
        //         std::cerr << "Request aborted by client" << std::endl;
        //     });

        //     // Handle incoming data chunks
        //     res->onData([res, body](std::string_view data, bool last) {
        //         body->append(data.data(), data.size());

        //         if (!last) {
        //             // Wait for more data
        //             return;
        //         }

        //         json response = json::object();

        //         try {
        //             auto parsed = json::parse(*body);
        //             if (!parsed.contains("sm_name") || !parsed["sm_name"].is_string()) {
        //                 response["error"] = "Missing or invalid 'sm_name' field";
        //                 res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
        //                 body->clear();
        //                 return;
        //             }
        //             parsed["action"] = "get";
                    
        //             response["status"] = "Got Data";
        //             res->writeHeader("content-type", "application/json")->end(response.dump());
        //         } catch (const std::exception& e) {
        //             response["error"] = std::string("Exception: ") + e.what();
        //             res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(response.dump());
        //         }

        //         body->clear();
        //     });

        //     // Return nothing to indicate async response
        // })

            //            {sm_name: "rtos", reg_type: "mb_hold", offset: "at_total_v_over", num: 4}
//        .post("/api/alarms/target/set", [](auto *res, auto *req) {
//            {sm_name: "rtos", reg_type: "mb_hold", offset: "at_total_v_over",…}

        //        .get("/api/alarms/target/get", [](auto *res, auto *req) {

    .post("/api/alarms/target/get", [](auto *res, auto *req) {
    std::shared_ptr<std::string> body = std::make_shared<std::string>();

    res->onAborted([body]() {
        body->clear();
        std::cerr << "Request aborted by client" << std::endl;
    });

    res->onData([res, body](std::string_view data, bool last) {
        body->append(data.data(), data.size());

        if (!last) {
            return;
        }

        json response = json::object();

        try {
            auto parsed = json::parse(*body);
            if (!parsed.contains("sm_name") || !parsed["sm_name"].is_string()) {
                response["error"] = "Missing or invalid 'sm_name' field";
                res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                body->clear();
                return;
            }

            parsed["action"] = "get";

            // Send to target and get response
            json targetResponse;
            {
                // Lock targetConfig for thread safety
                std::lock_guard<std::mutex> lock(targetConfig.mtx);
                targetResponse = sendTargetRequest(targetConfig.ip, targetConfig.port, parsed);
                targetResponse["read_data"] = targetResponse["data"];
                targetResponse.erase("data");

            }

            res->writeHeader("content-type", "application/json")->end(targetResponse.dump());

        } catch (const std::exception& e) {
            response["error"] = std::string("Exception: ") + e.what();
            res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(response.dump());
        }

        body->clear();
    });
})
.post("/api/alarms/target/set", [](auto *res, auto *req) {
    std::shared_ptr<std::string> body = std::make_shared<std::string>();

    res->onAborted([body]() {
        body->clear();
        std::cerr << "Request aborted by client" << std::endl;
    });

    res->onData([res, body](std::string_view data, bool last) {
        body->append(data.data(), data.size());

        if (!last) {
            return;
        }

        json response = json::object();

        try {
            auto parsed = json::parse(*body);
            if (!parsed.contains("sm_name") || !parsed["sm_name"].is_string()) {
                response["error"] = "Missing or invalid 'sm_name' field";
                res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                body->clear();
                return;
            }

            parsed["action"] = "set";
            parsed["data"] = parsed["write_data"];
            parsed.erase("write_data");
            

            // Send to target and get response
            json targetResponse;
            {
                // Lock targetConfig for thread safety
                std::lock_guard<std::mutex> lock(targetConfig.mtx);
                targetResponse = sendTargetRequest(targetConfig.ip, targetConfig.port, parsed);
                targetResponse["write_data"] = targetResponse["data"];
                targetResponse.erase("data");

            }

            res->writeHeader("content-type", "application/json")->end(targetResponse.dump());

        } catch (const std::exception& e) {
            response["error"] = std::string("Exception: ") + e.what();
            res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(response.dump());
        }

        body->clear();
    });
})

        .post("/api/alarms/target/config", [](auto *res, auto *req) {
            // Buffer to accumulate body data
            std::shared_ptr<std::string> body = std::make_shared<std::string>();

            // Handle client abort
            res->onAborted([body]() {
                // Clear buffer or do cleanup if needed
                body->clear();
                std::cerr << "Request aborted by client" << std::endl;
            });

            // Handle incoming data chunks
            res->onData([res, body](std::string_view data, bool last) {
                body->append(data.data(), data.size());

                if (!last) {
                    // Wait for more data
                    return;
                }

                json response = json::object();

                try {
                    auto parsed = json::parse(*body);
                    if (!parsed.contains("ip") || !parsed["ip"].is_string()) {
                        response["error"] = "Missing or invalid 'ip' field";
                        res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }

                    std::string ip = parsed["ip"];
                    int port = parsed.value("port", 9001);
                    bool secure = parsed.value("secure", false);
                    {
                        std::lock_guard<std::mutex> lock(targetConfig.mtx);
                        targetConfig.ip = ip;
                        targetConfig.port = port;
                        targetConfig.secure = secure;
                    }
                    response["status"] = "Target config updated";
                    res->writeHeader("content-type", "application/json")->end(response.dump());
                } catch (const std::exception& e) {
                    response["error"] = std::string("Exception: ") + e.what();
                    res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(response.dump());
                }

                body->clear();
            });

            // Return nothing to indicate async response
        })
            

        .post("/api/alarms/config", [](auto *res, auto *req) {
            // Buffer to accumulate body data
            std::shared_ptr<std::string> body = std::make_shared<std::string>();

            // Handle client abort
            res->onAborted([body]() {
                body->clear();
                std::cerr << "Request aborted by client" << std::endl;
            });

            // Handle incoming data chunks
            res->onData([res, req, body](std::string_view data, bool last) {
                body->append(data.data(), data.size());

                if (!last) {
                    return;
                }

                json response = json::object();

                try {
                    json alarmDefs = json::array();
                    json alarmLevelActions = json::array();
                    json limitsValues = json::array();
                    json limitsDef = json::array();

                    if (!body->empty()) {
                        try {
                            auto parsed = json::parse(*body);
                            if (parsed.contains("alarm_definitions") && parsed["alarm_definitions"].is_array()) {
                                alarmDefs = parsed["alarm_definitions"];
                                alarmLevelActions = parsed.value("alarm_level_actions", json::array());
                                limitsValues = parsed.value("limits_values", json::array());
                                limitsDef = parsed.value("limits_def", json::array());
                            }
                        } catch (...) {
                            // Ignore JSON parse error here, we'll handle it below
                        }
                    }

                    // Validate alarm definitions
                    if (alarmDefs.empty()) {
                        response["error"] = "Missing alarm_definitions array";
                        res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }
                    if (limitsDef.empty()) {
                        response["error"] = "Missing limits_def array";
                        res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }
                    // limitsValues and alarmLevelActions can be empty, but warn if alarmLevelActions empty
                    if (alarmLevelActions.empty()) {
                        response["error"] = "Missing alarm_level_actions array";
                        res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }

                    // Build a map from alarm_num to actions array
                    std::unordered_map<int, json> alarmNumToActions;
                    for (const auto& actionEntry : alarmLevelActions) {
                        // Safely extract alarm_num
                        int alarmNum = -1;
                        if (actionEntry.contains("alarm_num") && actionEntry["alarm_num"].is_number_integer()) {
                            alarmNum = actionEntry["alarm_num"];
                        } else {
                            continue; // Skip invalid entries
                        }

                        // Create action object without alarm_num
                        json actionObj = actionEntry;
                        actionObj.erase("alarm_num");

                        // Fix nested "actions" field if it's a string containing JSON
                        if (actionObj.contains("actions") && actionObj["actions"].is_string()) {
                            try {
                                actionObj["actions"] = json::parse(actionObj["actions"].get<std::string>());
                            } catch (...) {
                                // Leave as string if parse fails
                            }
                        }

                        // Add the action object to the array for this alarm_num
                        alarmNumToActions[alarmNum].push_back(actionObj);
                    }

                    // Reconstruct fullConfig alarms array
                    json fullConfig;
                    fullConfig["alarms"] = json::array();

                    for (const auto& alarmDef : alarmDefs) {
                        json alarmEntry = alarmDef;  // Start by copying the entire alarmDef to preserve all fields
                        
                        // Safely extract alarm_num
                        int alarmNum = 0;
                        if (alarmDef.contains("alarm_num") && alarmDef["alarm_num"].is_number_integer()) {
                            alarmNum = alarmDef["alarm_num"];
                        }

                        // Handle actions array with flattening
                        if (alarmNumToActions.find(alarmNum) != alarmNumToActions.end()) {
                            json flattenedActions = json::array();
                            
                            for (const auto& actionObj : alarmNumToActions[alarmNum]) {
                                json flatAction = actionObj;

                                // Flatten nested "actions" array
                                if (flatAction.contains("actions") && flatAction["actions"].is_array()) {
                                    // Merge each key-value pair from inner actions into flatAction
                                    for (const auto& innerAction : flatAction["actions"]) {
                                        for (auto it = innerAction.begin(); it != innerAction.end(); ++it) {
                                            flatAction[it.key()] = it.value();
                                        }
                                    }
                                    // Remove the nested "actions" array after merging
                                    flatAction.erase("actions");
                                }

                                // Rename "enabled" to "level_enabled" if present
                                if (flatAction.contains("enabled")) {
                                    flatAction["level_enabled"] = flatAction["enabled"];
                                    flatAction.erase("enabled");
                                }

                                // Fix notes null to empty string
                                if (!flatAction.contains("notes") || flatAction["notes"].is_null()) {
                                    flatAction["notes"] = "";
                                }

                                flattenedActions.push_back(flatAction);
                            }
                            
                            alarmEntry["actions"] = flattenedActions;
                        } else {
                            alarmEntry["actions"] = json::array();
                        }

                        // Ensure notes is string, not null
                        if (!alarmEntry.contains("notes") || alarmEntry["notes"].is_null()) {
                            alarmEntry["notes"] = "";
                        }

                        // Ensure limits array is present (copy from alarmDef if exists)
                        if (!alarmEntry.contains("limits") || !alarmEntry["limits"].is_array()) {
                            alarmEntry["limits"] = json::array();
                        }

                        // Ensure _limit_def_matched is present (copy from alarmDef if exists)
                        if (!alarmEntry.contains("_limit_def_matched")) {
                            if (alarmDef.contains("_limit_def_matched")) {
                                alarmEntry["_limit_def_matched"] = alarmDef["_limit_def_matched"];
                            } else {
                                alarmEntry["_limit_def_matched"] = "";
                            }
                        }

                        fullConfig["alarms"].push_back(alarmEntry);
                    }
                    fullConfig["limits_def"] = limitsDef;
                        
                    response["full_config"] = fullConfig;

                    // Save to file
                    std::ofstream file("json/alarm_definition_saved.json");
                    if (!file.is_open()) {
                        response["error"] = "Failed to open alarm_definition_saved.json for writing";
                        res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }
                    file << fullConfig.dump(4);
                    file.close();

                    response["success"] = true;
                    response["message"] = "Alarm configuration updated successfully";
                    res->writeHeader("content-type", "application/json")->end(response.dump());

                } catch (const std::exception& e) {
                    json errorResponse = {{"error", std::string("Exception: ") + e.what()}};
                    res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(errorResponse.dump());
                }

                body->clear();
            });
        })

        .post("/api/pub_defs/save_var_list", [](auto *res, auto *req) {
            // Buffer to accumulate body data
            std::shared_ptr<std::string> body = std::make_shared<std::string>();

            // Handle client abort
            res->onAborted([body]() {
                body->clear();
                std::cerr << "Request aborted by client" << std::endl;
            });

            // Handle incoming data chunks
            res->onData([res, req, body](std::string_view data, bool last) {
                body->append(data.data(), data.size());

                if (!last) {
                    return;
                }

                json response = json::object();

                try {
                    // Get filename from query parameter or JSON body
                    std::string fileName;
                    
                    // Option 1: Get filename from query parameter
                    std::string query(req->getQuery());
                    // Simple parser for query params (you might want a more robust one)
                    if (query.find("file_name=") != std::string::npos) {
                        size_t start = query.find("file_name=") + 10;
                        size_t end = query.find("&", start);
                        if (end == std::string::npos) end = query.length();
                        fileName = query.substr(start, end - start);
                    }
                    
                    // Option 2: Get filename from JSON body (if not in query)
                    if (fileName.empty() && !body->empty()) {
                        try {
                            auto parsed = json::parse(*body);
                            if (parsed.contains("file_name") && parsed["file_name"].is_string()) {
                                fileName = parsed["file_name"];
                            }
                        } catch (...) {
                            // Ignore JSON parse error here, we'll handle it below
                        }
                    }

                    // Validate filename
                    if (fileName.empty()) {
                        response["error"] = "Missing filename parameter";
                        res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }

                    // Basic sanitization
                    if (fileName.find("..") != std::string::npos || fileName.find('/') != std::string::npos) {
                        response["error"] = "Invalid filename";
                        res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }

                    // Ensure directory exists (you might want to create it if it doesn't exist)
                    std::string filePath = "var_lists/" + fileName;

                    // Write body content to file
                    std::ofstream file(filePath, std::ios::binary);
                    if (!file.is_open()) {
                        response["error"] = "Failed to create file: " + fileName;
                        res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }

                    file << *body;
                    file.close();

                    response["success"] = true;
                    response["message"] = "File saved successfully";
                    response["file_name"] = fileName;
                    res->writeHeader("content-type", "application/json")->end(response.dump());

                } catch (const std::exception& e) {
                    json errorResponse = {{"error", std::string("Exception: ") + e.what()}};
                    res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(errorResponse.dump());
                }

                body->clear();
            });
        })


        .post("/api/pub_defs/load_var_list", [](auto *res, auto *req) {
            // Buffer to accumulate body data
            std::shared_ptr<std::string> body = std::make_shared<std::string>();

            // Handle client abort
            res->onAborted([body]() {
                // Clear buffer or do cleanup if needed
                body->clear();
                std::cerr << "Request aborted by client" << std::endl;
            });

            // Handle incoming data chunks
            res->onData([res, body](std::string_view data, bool last) {
                body->append(data.data(), data.size());

                if (!last) {
                    // Wait for more data
                    return;
                }

                json response = json::object();

                try {
                    auto parsed = json::parse(*body);
                    if (!parsed.contains("file_name") || !parsed["file_name"].is_string()) {
                        response["error"] = "Missing or invalid 'file_name' field";
                        res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }

                    std::string fileName = parsed["file_name"];
                    std::string filePath = "var_lists/" + fileName;

                    std::ifstream file(filePath);
                    if (!file.is_open()) {
                        response["error"] = "File not found: " + fileName;
                        res->writeStatus("404 Not Found")->writeHeader("content-type", "application/json")->end(response.dump());
                        body->clear();
                        return;
                    }

                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string fileContent = buffer.str();

                    auto fileJson = json::parse(fileContent);

                    res->writeHeader("content-type", "application/json")->end(fileJson.dump());
                } catch (const std::exception& e) {
                    response["error"] = std::string("Exception: ") + e.what();
                    res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(response.dump());
                }

                body->clear();
            });

            // Return nothing to indicate async response
        })

        // Add this handler in your uWS::App() chain:
        .post("/api/pub_defs/xload_var_list", [](auto *res, auto *req) {
            res->onData([res](std::string_view data, bool last) {
                static std::string body;
                body.append(data.data(), data.size());

                if (!last) {
                    std::cout << "Wait for more data\n";
                    return;
                }

                json response = json::object();

                try {
                    auto parsed = json::parse(body);
                    if (!parsed.contains("file_name") || !parsed["file_name"].is_string()) {
                        response["error"] = "Missing or invalid 'file_name' field";
                        res->writeStatus("400 Bad Request")->writeHeader("content-type", "application/json")->end(response.dump());
                        body.clear();
                        return;
                    }

                    std::string fileName = parsed["file_name"];
                    std::string filePath = "var_lists/" + fileName;

                    std::ifstream file(filePath);
                    if (!file.is_open()) {
                        response["error"] = "File not found: " + fileName;
                        res->writeStatus("404 Not Found")->writeHeader("content-type", "application/json")->end(response.dump());
                        body.clear();
                        return;
                    }

                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string fileContent = buffer.str();

                    // Optionally parse file content to JSON to validate
                    auto fileJson = json::parse(fileContent);

                    res->writeHeader("content-type", "application/json")->end(fileJson.dump());
                } catch (const std::exception& e) {
                    response["error"] = std::string("Exception: ") + e.what();
                    res->writeStatus("500 Internal Server Error")->writeHeader("content-type", "application/json")->end(response.dump());
                }

                body.clear();
            });
        })

        // .ws<PerSocketData>("/*", {
        //     .open = [](uWS::WebSocket<true, false, PerSocketData>* ws) {
        //         std::cout << "Client connected\n";
        //     },
        //     .message = [](uWS::WebSocket<true, false, PerSocketData>* ws, std::string_view message, uWS::OpCode opCode) {
        //         if (opCode == uWS::OpCode::TEXT) {
        //             handleMessage(ws, message);
        //         }
        //     },
        //     .close = [](uWS::WebSocket<true, false, PerSocketData>* ws, int code, std::string_view message) {
        //         std::cout << "Client disconnected\n";
        //     }
        // })

        // .ws<PerSocketData>("/*", {
        //     .open = [](auto* ws) {
        //         std::cout << "Client connected\n";
        //     },
        //     .message = [](auto* ws, std::string_view message, uWS::OpCode opCode) {
        //         if (opCode == uWS::OpCode::TEXT) {
        //             handleMessage(ws, message);
        //         }
        //     },
        //     .close = [](auto* ws, int code, std::string_view message) {
        //         std::cout << "Client disconnected\n";
        //     }
        // })
        .listen(bindIp, port, [bindIp, port](auto* token) {
            if (token) {
                std::cout << "Server listening on " << bindIp << ":" << port << std::endl;
            } else {
                std::cerr << "Failed to listen on " << bindIp << ":" << port << std::endl;
            }
        })
        .run();

    return 0;
}