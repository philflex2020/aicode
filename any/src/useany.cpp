#include <iostream>
#include <any>
#include <map>
#include <vector>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <string>
//g++ -std=c++17 -o any src/useany.cpp  -lsimdjson -lspdlog

std::map<std::string, std::any> jsonToMap(simdjson::dom::object obj);

std::string mapToString(const std::map<std::string, std::any>& m);

std::string anyToString(const std::any& value) {
    std::cout << "Type of value: " << value.type().name() << std::endl;

    try {
        return std::to_string(std::any_cast<int>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        return std::to_string(std::any_cast<double>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        return std::to_string(std::any_cast<long>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        return std::any_cast<bool>(value) ? "true" : "false";
    } catch (const std::bad_any_cast&) {}
    try {
        return std::any_cast<std::string>(value);
    } catch (const std::bad_any_cast&) {}
    try {
        return mapToString(std::any_cast<std::map<std::string, std::any>>(value));
    } catch (const std::bad_any_cast&) {}
    try {
        std::vector<std::any> vec = std::any_cast<std::vector<std::any>>(value);
        std::string result = "[";
        for (const auto& v : vec) {
            result += anyToString(v) + ", ";
        }
        if (result.length() > 1) {
            result = result.substr(0, result.length() - 2);  // remove the last ", "
        }
        result += "]";
        return result;
    } catch (const std::bad_any_cast& e) {
        std::cout << "Bad type "<< e.what() << std::endl;
    }
    return "unknown";
}


std::string mapToString(const std::map<std::string, std::any>& m) {
    std::string result = "{";
    for (const auto& [key, value] : m) {
        result += "\"" + key + "\": " + anyToString(value) + ", ";
    }
    if (result.length() > 1) {
        result = result.substr(0, result.length() - 2);  // remove the last ", "
    }
    result += "}";
    return result;
}


std::any jsonToAny(simdjson::dom::element elem) {
    switch (elem.type()) {
        case simdjson::dom::element_type::INT64:
            return int64_t(elem);
        case simdjson::dom::element_type::UINT64:
            return uint64_t(elem);
        case simdjson::dom::element_type::DOUBLE:
            return double(elem);
        case simdjson::dom::element_type::STRING:
            return std::string(elem);
        case simdjson::dom::element_type::ARRAY: {
            simdjson::dom::array arr = elem;
            std::vector<std::any> vec;
            for (simdjson::dom::element child : arr) {
                vec.push_back(jsonToAny(child));
            }
            return vec;
        }
        case simdjson::dom::element_type::OBJECT:
            return jsonToMap(elem);
        default:
            return nullptr;
    }
}

std::map<std::string, std::any> jsonToMap(simdjson::dom::object obj) {
    std::map<std::string, std::any> m;

    for (auto [key, value] : obj) {
        m[std::string(key)] = jsonToAny(value);
    }

    return m;
}

int main() {
    simdjson::dom::parser parser;
    //simdjson::dom::object obj = 
    simdjson::dom::object obj;
    simdjson::error_code error;
    //auto mystr = R"({"key": [1, 2, 3], "anotherKey": {"subKey": "value"}})";
    auto mystr = R"({"/status/pcs": { "voltage": 1345 , "current": 34356 }, "/status/bms":{"SOC":34.5 , "capacity": 34567}})";
    auto result = parser.parse(mystr, strlen(mystr));
    
    if (result.error()) {
        //std::cerr << error << std::endl;
        std::cerr << simdjson::error_message(result.error()) << std::endl;
        return EXIT_FAILURE;
    }
    obj = result.value();
    std::map<std::string, std::any> m = jsonToMap(obj);

    // Now m contains the JSON data
    for (auto mx : m) {
    std::cout << " Key " << mx.first << std::endl;
    try {
        auto mymap = std::any_cast<std::map<std::string, std::any>>(mx.second);
        for (auto my : mymap) {
            std::cout << "     Item " << my.first << std::endl;
        }
        } catch (const std::bad_any_cast&) {
            std::cout << "     Value cannot be cast to map<string, any>" << std::endl;
        }
    }

    // for (auto mx : m) {
    //     std::cout << " Key " << mx.first << std::endl;
    //     for (auto my : mx.second) {
    //         std::cout << "     Item " << my.first << std::endl;
    //     }
    // }
    spdlog::info("Map content: {}", mapToString(m));
    
    return 0;
}