//#pragma once
#ifndef GCOM_CONFIG_TMPL_H
#define GCOM_CONFIG_TMPL_H

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <any>
#include <map>
#include <vector>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <string>
#include <fstream>
#include <sstream>


template <typename T>
std::optional<bool> getItemFromMap(const std::map<std::string, std::any>& m, const std::string& query, T& target, const T& defaultValue, bool def, bool required, bool debug );

template<typename T>
std::optional<T> getMapValue(const std::map<std::string, std::any>& m, const std::string& query);

//template<typename T>
//std::optional<T> getItemFromMap(const std::map<std::string, std::any>& m, const std::string& query, const T& defaultValue, bool def, bool required, bool debug);


template<typename T>
std::optional<T> getMapValue(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    try {
        while (std::getline(ss, key, '.')) {
            auto it = currentMap->find(key);
            if (it == currentMap->end()) {
                return std::nullopt;
            }

            if (it->second.type() == typeid(std::map<std::string, std::any>)) {
                currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
            } else if (it->second.type() == typeid(T) && !std::getline(ss, key, '.')) {
                return std::any_cast<T>(it->second);
            } else {
                return std::nullopt;
            }
        }
    } catch (const std::bad_any_cast& e) {
        // Handle or log the exception, for example:
        std::cerr << "Failed to cast value in map with query '" << query << "': " << e.what() << std::endl;
        return std::nullopt;
    }

    return std::nullopt;
}
//                getItemFromMap(mapData,   "id",                map.id,         std::string("Some_id"),       true, true, true);

template <typename T>
std::optional<bool> getItemFromMap(const std::map<std::string, std::any>& m, const std::string& query, T& target,const T& defaultValue,bool def,bool required,bool debug) {

    bool ok = true;
// template <typename T>
// std::optional<T> getItemFromMap(const std::map<std::string, std::any>& m, std::string& query, T& target, T& defaultValue, bool def, bool required, bool debug ) {
    auto result = getMapValue<T>(m, query);
    
    if (result.has_value()) {
        if(debug) std::cerr << "Found "<< query << " " << result.value() << std::endl;
        target = result.value();
        return ok;
    } else if (def && required) {
        if(debug) std::cerr << "Used Default " << query << " " << defaultValue << std::endl;
        target = defaultValue;//result.value();
        return ok;
    }
    return false;
}

template <typename T>
bool  getItemFromMapOk(const std::map<std::string, std::any>& m, const std::string& query, T& target, const T& defaultValue, bool def, bool required, bool debug ) {
//     // function body
// }

// template <typename T>
// std::optional<T> getItemFromMap(const std::map<std::string, std::any>& m, std::string& query, T& target, T& defaultValue, bool def, bool required, bool debug ) {
    auto result = getMapValue<T>(m, query);
    
    if (result.has_value()) {
        if(debug) std::cerr << "Found "<< query << " " << result.value() << std::endl;
        target = result.value();
        return true;
    } else if (def && required) {
        if(debug) std::cerr << "Used Default " << query << " " << defaultValue << std::endl;
        target = defaultValue;//result.value();
        return true;
    }
    return false;
}




#endif

