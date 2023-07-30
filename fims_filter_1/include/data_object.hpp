//data_object.hpp:

//cpp
//Copy code
#ifndef DATA_OBJECT_HPP
#define DATA_OBJECT_HPP
#include <queue>
#include <map>
#include <string>

struct DataObject {
    std::string uri;
    std::string id;
    std::string value;
    bool isCommand; // Flag to indicate if it's a command
};

enum class DataFormat {
    SIMPLE,
    NESTED,
};

using DataMap = std::map<std::string, std::map<std::string, std::string>>;

#endif // DATA_OBJECT_HPP