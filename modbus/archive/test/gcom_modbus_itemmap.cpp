// given a file name
// creates a  
//      std::map<std::string, std::any>

#include <iostream>
#include <any>
#include <map>
#include <vector>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <regex>
#include <cxxabi.h>


#include "gcom_config.h"
#include "gcom_config_tmpl.h"


namespace mycfg {  // Assuming cfg namespace based on the given extract_maps function

struct item_struct {
    std::string id;
    std::string name;
    std::string rtype;
    int offset;
    int size;
    int device_id;
    std::any value=0;
};

}  // namespace mycfg

void extract_connection(std::map<std::string, std::any>gcom_map, const std::string& query, struct cfg& myCfg, bool debug);
bool extract_components(std::map<std::string, std::any>gcom_map, const std::string& query, struct cfg& myCfg, bool debug);

// skips packed registers


//       from components[x].component_id or "/components if missing"
//std::map<std::string,               
//           from components[x].id   must be present  
//         std::map<std::string, 
//            from        components[x].registers[y].map[z].id must be present
//                     std::map<std::string, 
//                             cfg::item_struct*>>>
//                                   id from components[x].registers[y].map[z].id
//                                   name from components[x].registers[y].map[z].name
//                                   rtype from components[x].registers[y].type
//                                   offset from components[x].registers[y].map[z].offset
//                                   size from components[x].registers[y].map[z].size or 1
//                                   device_id from components[x].device_id or 255


using ItemSharedPtr = std::shared_ptr<mycfg::item_struct>;
using ItemMap = std::map<std::string, std::map<std::string, std::map<std::string, ItemSharedPtr>>>;

// todo turn this into 
//using ItemSharedPtr = std::shared_ptr<cfg::map_struct*>;
//using ItemMap = std::map<std::string, std::map<std::string, std::map<std::string, ItemSharedPtr>>>;

// this shows the itemMap structure
void printItem(const ItemSharedPtr& item) {
    if (item) {
        std::cout << "Here is the item " << std::endl;
        std::cout << "     ID: " << item->id << "\n";
        std::cout << "     Name: " << item->name << "\n";
        std::cout << "     Type: " << item->rtype << "\n";
        std::cout << "     Offset: " << item->offset << "\n";
        std::cout << "     Size: " << item->size << "\n";
        std::cout << "     Device ID: " << item->device_id << "\n";
    } else {
        std::cout << "Item is nullptr!\n";
    }
}

void printItemMap(const ItemMap& items) {
    for (const auto& componentPair : items) {
        std::cout << "Component: " << componentPair.first << "\n";
        for (const auto& idPair : componentPair.second) {
            std::cout << "  ID: " << idPair.first << "\n";
            for (const auto& innerIdPair : idPair.second) {
                std::cout << "    Inner ID: " << innerIdPair.first << "\n";
                const ItemSharedPtr& item = innerIdPair.second;
                printItem(item);
            }
        }
    }
}

ItemSharedPtr findItem(const ItemMap& items, const std::string& component, const std::string& id, const std::string& name) {
    std::cout << " Seeking :"<<component<<"/"<<id<<"  "<<name  << std::endl;

    auto componentIt = items.find(component);
    if (componentIt != items.end()) {
        const auto& idMap = componentIt->second;
        auto idIt = idMap.find(id);
        if (idIt != idMap.end()) {
            for (const auto& innerIdPair : idIt->second) {
                const ItemSharedPtr& item = innerIdPair.second;
                if (item && item->id == name) {
                    return item;
                }
            }
        }
    }
    return nullptr;
}




//
// this is for random sets , we are able to bin the data into CompressedResults
// for processing pubs we.ll need to get back to the register structure , if all the mapped items are included in the message then the whole register set can be 
// processed.
// this is really for the modbus server.
// the gcom_map is the items
//
bool parseFimsMessage(const ItemMap& items,  std::vector<std::pair<ItemSharedPtr, std::any>>& result,
                                                                    const std::string&method, const std::string& uri, const std::string& body) {
    simdjson::dom::parser parser;
    simdjson::dom::object json_obj;
    auto error = parser.parse(body).get(json_obj);
    if (error) {
        std::cout << "parse body failed [" << body << "]"<< std::endl;
        return false; // JSON parsing failed
    }
   //std::cout << "parse body OK" << std::endl;

    auto uri_tokens = split_string(uri, '/');
    if (uri_tokens.size() < 2) {
        std::cout << "uri_tokens fail size :"<< uri_tokens.size() <<" uri :"<< uri << std::endl;
        return false; // Invalid URI
    }
    //std::cout << "uri_tokens OK size :"<< uri_tokens.size() << std::endl;

    const auto& first_field = uri_tokens[1];
    if (items.find(first_field) == items.end()) {
        std::cout << "components field not mapped in config :"<< first_field << std::endl;
        return false; // The first field of the URI doesn't match with the map
    }
    //std::cout << "first_field :"<< first_field << " OK" << std::endl;
    const auto& item_id = uri_tokens[2];
    //std::cout << "item_id :"<< item_id << " OK" << std::endl;

    // If the URI provides a direct identification to an item
    // ie /components/comp_sel_2440/heartbeat 1234
    // or /components/comp_sel_2440/heartbeat '{"value":1234}'
    if (uri_tokens.size() == 4) {
        if (json_obj.size() != 1) {

            std::cout << "single uri body invalid :"<< body << std::endl;
            return false; // Expected a direct value
        }

        for (auto [key, value] : json_obj) {
            if (key == item_id) {
                ItemSharedPtr foundItem = items.at(first_field).begin()->second.at(item_id);
                std::any foundValue;
                extractJsonValue(value, foundValue);
                result.emplace_back(foundItem, foundValue);
                return true; // Successfully matched a specific item
            }
        }
        return false; // No match found
    }

    // Check the body for multiple items
    for (auto [key, value] : json_obj) {

        std::string key_str = std::string(key);
        //std::cout << " seeking item " << key_str << " in config" << std::endl;

        auto it = items.at(first_field).find(item_id);
        if (it != items.at(first_field).end()) {

            auto it = items.at(first_field).at(item_id).find(key_str);
            if (it != items.at(first_field).at(item_id).end()) {
                ItemSharedPtr foundItem = items.at(first_field).at(item_id).at(key_str);

                //ItemSharedPtr foundItem = (it->second.begin())->second;
                std::any foundValue;

                if (value.is_object() && value["value"].error() == simdjson::SUCCESS) {
                    extractJsonValue(value["value"], foundValue);
                } else {
                    extractJsonValue(value, foundValue);
                }
                //std::cout << " Found item " << key_str << " in config  " << std::endl;
                result.emplace_back(foundItem, foundValue);
            }
        }
        else
        {
            std::cout << " Unable to find item " << key_str << " in config , ignoring it " << std::endl;
            return false;
        }
    }

    return true; // Parsing and matching process completed successfully (even if no matches are found)
}

// after getting in a fims message we'll get a vector of items found in the message and values 
// if this is a SET message then we'll have to compress the result vector and the encode the values into the compressed buffer.
// a SET message will also set the local values.
// the iothread can then send the compressed buffer to the device and return the result to the main thread.
// the pubs already have compressed resuts ready to send in that case the iothread will return data in the iobuffer and we'll have to decode that into the actual values.
// a GET message will only access the local data values
//
void printResultVector(const std::vector<std::pair<ItemSharedPtr, std::any>>& result) {
    for (const auto& [itemPtr, anyValue] : result) {
        // Accessing members of the item struct via the shared_ptr
        //printItem(itemPtr);

        std::cout << "Item Details:\n";
        std::cout << "         ID: " << itemPtr->id << "\n";
        std::cout << "         Name: " << itemPtr->name << "\n";
        std::cout << "         Type: " << itemPtr->rtype << "\n";
        std::cout << "         Offset: " << itemPtr->offset << "\n";
        std::cout << "         Size: " << itemPtr->size << "\n";
        std::cout << "         Device ID: " << itemPtr->device_id << "\n";

        // Extracting and printing value from std::any
        if (anyValue.type() == typeid(bool)) {
            std::cout << "             Value: " << std::boolalpha << std::any_cast<bool>(anyValue) << "\n";
        } else if (anyValue.type() == typeid(int64_t)) {
            std::cout << "             Value: " << std::any_cast<int64_t>(anyValue) << "\n";
        } else if (anyValue.type() == typeid(int)) {
            std::cout << "             Value: " << std::any_cast<int>(anyValue) << "\n";
        } else if (anyValue.type() == typeid(std::string)) {
            std::cout << "             Value: " << std::any_cast<std::string>(anyValue) << "\n";
        } else if (anyValue.type() == typeid(double)) {
            std::cout << "             Value: " << std::any_cast<double>(anyValue) << "\n";
        } else {
            std::cout << "using unhandled type :" << anyValue.type().name();
        } // Add more types as needed
// You can add more types if needed

        std::cout << "--------------------\n";
    }
}


struct CompressedItem {
    std::string pg_name; // which publish group did this come from 
    double timeId;     // this is the time from get_time_double that we submitted this item
    int itemId;        // in a list of items this is the itemid or sequence number
    int device_id;
    std::string rtype;
    int start_offset;
    int end_offset;
    int number_of_registers;
    
    int size;  // Note: this is cumulative size now.
    //std::vector<std::any> values; // Storing the related values
    std::vector<std::pair<ItemSharedPtr, std::any>> values;
    unsigned int buffer[125];
};

bool compressResults(const std::vector<std::pair<ItemSharedPtr, std::any>>& results,
                     std::vector<CompressedItem>& compressedResults) {
    
    // First, sort the results by device_id, rtype, and offset.
    auto sortedResults = results; // Make a copy to sort
    std::sort(sortedResults.begin(), sortedResults.end(),
              [](const std::pair<ItemSharedPtr, std::any>& a, const std::pair<ItemSharedPtr, std::any>& b) {
                  if (a.first->device_id != b.first->device_id) return a.first->device_id < b.first->device_id;
                  if (a.first->rtype != b.first->rtype) return a.first->rtype < b.first->rtype;
                  return a.first->offset < b.first->offset;
              });

    for (size_t i = 0; i < sortedResults.size(); ++i) {
        const auto& currentPair = sortedResults[i];
        
        // Start a new compressed item
        CompressedItem compressedItem;
        compressedItem.device_id = currentPair.first->device_id;
        compressedItem.rtype = currentPair.first->rtype;
        compressedItem.start_offset = currentPair.first->offset;
        compressedItem.end_offset = currentPair.first->offset + currentPair.first->size - 1; // Initial value
        compressedItem.values.push_back(currentPair);
        
        // Try to group following items if they match in device_id, rtype, and offset sequence
        while (i + 1 < sortedResults.size()) {
            const auto& nextPair = sortedResults[i + 1];
            if (nextPair.first->device_id == compressedItem.device_id &&
                nextPair.first->rtype == compressedItem.rtype &&
                nextPair.first->offset == compressedItem.end_offset + 1) {
                // Group together
                compressedItem.end_offset = nextPair.first->offset + nextPair.first->size - 1;
                compressedItem.values.push_back(nextPair);
                ++i; // Move to the next result
            } else {
                break; // Exit if we can't group the next item
            }
        }
        
        compressedResults.push_back(compressedItem);
    }
    
    return true; 
}

void printCompressedResults(const std::vector<CompressedItem>& compressedResult) {
    for (const auto& item : compressedResult) {
        std::cout << "    Compressed Item Details:\n";
        std::cout << "          Device ID: " << item.device_id << "\n";
        std::cout << "          Type: " << item.rtype << "\n";
        std::cout << "          Start Offset: " << item.start_offset << "\n";
        std::cout << "          End Offset: " << item.end_offset << "\n";
        
        std::cout << "          Values:\n";
        for (const auto& [itemPtr, anyValue] : item.values) {
            //printItem(itemPtr);

            // Accessing members of the original item struct via the shared_ptr
            std::cout << "          - Item ID: " << itemPtr->id << ", Name: " << itemPtr->name << ", Value: ";

            // Extracting and printing value from std::any
            if (anyValue.type() == typeid(bool)) {
                std::cout << std::boolalpha << std::any_cast<bool>(anyValue);
            } else if (anyValue.type() == typeid(int64_t)) {
                std::cout << std::any_cast<int64_t>(anyValue);
            } else if (anyValue.type() == typeid(int)) {
                std::cout << std::any_cast<int>(anyValue);
            } else if (anyValue.type() == typeid(std::string)) {
                std::cout << std::any_cast<std::string>(anyValue);
            } else if (anyValue.type() == typeid(double)) {
                std::cout << std::any_cast<double>(anyValue);
            } else {
                std::cout << "using unhandled type :" << anyValue.type().name();
            } // Add more types as needed
            std::cout << "\n";
        }

        std::cout << "--------------------\n";
    }
}

// this create the Item map 
// from the gcom_map

ItemMap extract_structure(ItemMap &structure, const std::map<std::string, std::any>& jsonData) {
    bool debug = false;
    //ItemMap structure;

    // Extract components array
    auto rawComponents = getMapValue<std::vector<std::any>>(jsonData, "components");
    if (!rawComponents.has_value()) {
        std::cout << __func__ << " error no  components in jsonData"<<std::endl;
        return structure;
        //throw std::runtime_error("Missing components in JSON data.");
    }
    std::cout << __func__ << " >>>> found components in jsonData"<<std::endl;

    for (const std::any& rawComponent : rawComponents.value()) {
        if (rawComponent.type() == typeid(std::map<std::string, std::any>)) {
            std::map<std::string, std::any> componentData = std::any_cast<std::map<std::string, std::any>>(rawComponent);

            std::string componentId;
            getItemFromMap(componentData, "component_id", componentId, std::string("components"), true, true, false).value();
            std::cout << __func__ << " >>>> found componentsId :"<< componentId<<" in jsonData"<<std::endl;
            std::string id; 
            getItemFromMap(componentData, "id", id, std::string(), true, false, false).value();

            int device_id; 
            getItemFromMap(componentData, "device_id", device_id, 255, true, true, false).value();

            auto rawRegisters = getMapValue<std::vector<std::any>>(componentData, "registers");
            if (!rawRegisters.has_value()) {
                throw std::runtime_error("Missing registers in component data.");
            }
            std::cout << __func__ << " >>>> found registers in jsonData"<<std::endl;

            for (const std::any& rawRegister : rawRegisters.value()) {
                if (rawRegister.type() == typeid(std::map<std::string, std::any>)) {
                    std::map<std::string, std::any> registerData = std::any_cast<std::map<std::string, std::any>>(rawRegister);

                    std::string rtype; 
                    getItemFromMap(registerData, "type", rtype, std::string(), true, false,  debug).value();

                    auto rawMaps = getMapValue<std::vector<std::any>>(registerData, "map");
                    if (!rawMaps.has_value()) {
                        continue;  // No maps to process in this register
                    }

                    for (const std::any& rawMap : rawMaps.value()) {
                        if (rawMap.type() == typeid(std::map<std::string, std::any>)) {
                            std::map<std::string, std::any> mapData = std::any_cast<std::map<std::string, std::any>>(rawMap);

                            // this needs to be struct cfg::map_struct
                            //std::vector<cfg::map_struct> extract_maps(struct cfg::reg_struct* reg,  std::any& rawMapData) {
                            // but we may need a different root object here
                            // look at extract components

                            ItemSharedPtr item = std::make_shared<mycfg::item_struct>();
                            //item->id = 
                            getItemFromMap(mapData, "id", item->id, std::string("some_id"), true, false, false);
                            //item->name = 
                            getItemFromMap(mapData, "name", item->name, std::string("some_name"), true, false,  false);
                            item->rtype = rtype;
                            //item->offset = 
                            getItemFromMap(mapData, "offset", item->offset, 0, true,false,  debug);
                            //item->size = 
                            getItemFromMap(mapData, "size", item->size, 1, true, true, false);
                            item->device_id = device_id;

                            structure[componentId][id][item->id] = item;
                        }
                    }
                }
            }
        }
    }

    return structure;
}
//}  // namespace mycfg

double get_time_double();



class PublishGroup {
private:

public:
    double timeId;     // this is the time from get_time_double that we submitted this item
    int numItems;      //  this is the number of items in the vector.

    std::string name;
    int frequency;
    int offset_time;
    std::vector<CompressedItem> compressedItems;
    // Constructor
    PublishGroup(const std::string& component_id, const std::string& id, 
                 int freq, int offset) 
        : name(component_id + "_" + id), frequency(freq), offset_time(offset) {}

    // Add a CompressedItem to the list
    void addCompressedItem(const CompressedItem& item) {
        compressedItems.push_back(item);
    }

    // (Optional) Retrieve the list of CompressedItem objects
    const std::vector<CompressedItem>& getCompressedItems() const {
        return compressedItems;
    }

    // Method to handle publishing based on the timer
    void publish(std::vector<CompressedItem>& requests) {
        auto itemId = 0;
        timeId = get_time_double();
        for (auto& item : compressedItems) {
            item.pg_name = name;   // who sent this
            item.timeId = timeId;  // I'm assuming timeId is defined somewhere in your class
            item.itemId = itemId++;
            requests.push_back(item);  // Appending to the passed-in requests vector
        }
    }
};


// this is a floating object at the moment 
// each request needs to be sent to the iothread get queue
std::vector<CompressedItem> pub_requests;
//these will get sent on to the ioThreads

void publish_cb(void * timer, void *data) {
    // Cast data pointer back to publish_group
    PublishGroup* pg = static_cast<PublishGroup*>(data);
    pg->publish(pub_requests);

    // For now, let's just print a greeting
    std::cout << "Hi from publish_cb! Processing data for publish group: " << pg->name << std::endl;
}

// Dummy timer create function
void timer_create(const std::string& name, int frequency, int offset, void (*callback)(void*, void*), void* data) {
    // For now, let's just print the timer setup
    // Cast the void pointer back to PublishGroup*
    PublishGroup* pg = static_cast<PublishGroup*>(data);
    std::cout << "Timer created with name: " << name << ", frequency: " << pg->frequency << ", offset: " << offset << std::endl;
}

void register_publish_groups_with_timer(const std::vector<std::shared_ptr<PublishGroup>>& publishGroups) {
    for (auto& pgPtr : publishGroups) {
        // Construct timer name from publish group name
        std::string timerName = "timer_for_" + pgPtr->name;

        // Register with timer_create
        // We'll use get() to obtain the raw pointer from shared_ptr
        timer_create(timerName, pgPtr->frequency, pgPtr->offset_time, publish_cb, (void*)pgPtr.get());
    }
}


void show_publish_groups(std::vector<std::shared_ptr<PublishGroup>> &publishGroups) {
    for (auto& pg : publishGroups) {
        std::cout << "Publish Group name: " << pg->name
                  << ", frequency: " << pg->frequency
                  << ", offset: " << pg->offset_time << std::endl;

        // Showing Compressed Items
        std::cout << "  Compressed Items: " << std::endl;
        printCompressedResults(pg->compressedItems);

    }
}

// TODO stick this somewhere
std::map<std::string, std::shared_ptr<PublishGroup>> publishGroupMap;


// this is run once after start up 
// it will create the different publish_groups
// when connected to a timer, the callback will get a requests list generated and send the requrst to the iothread pub queue.
// as each result gets back from the iothread the main thread will collect all the results into a pub_vector 
// when they are all back we can then pub the results out.
// of course we'll have to decode the results and save the values in the local Itemmap

bool extract_publish_groups(std::vector<std::shared_ptr<PublishGroup>> &publishGroups, const ItemMap& items, const std::map<std::string, std::any>& jsonData) {
    bool debug = false;
    // Extract components array
    auto rawComponents = getMapValue<std::vector<std::any>>(jsonData, "components");
    if (!rawComponents.has_value()) {
        std::cout << __func__ << " error: no components in jsonData" << std::endl;
        return false;
    }
    std::cout << __func__ << " >>>> found components in jsonData" << std::endl;

    for (const std::any& rawComponent : rawComponents.value()) {
        if (rawComponent.type() == typeid(std::map<std::string, std::any>)) {
            std::map<std::string, std::any> componentData = std::any_cast<std::map<std::string, std::any>>(rawComponent);
            std::string componentId;
            getItemFromMap(componentData, "component_id", componentId, std::string("components"), true, true, false);
            std::string comp_id; 
            getItemFromMap(componentData, "id", comp_id, std::string(), true, false, false);
            int frequency;
            getItemFromMap(componentData, "frequency", frequency, 1000, true, false, false);
            int offset_time;
            getItemFromMap(componentData, "offset_time", offset_time, 0, true, false, false);
            int comp_device_id;
            getItemFromMap(componentData, "device_id", comp_device_id, 255, true, false,  debug);


            std::string groupName = componentId + "_" + comp_id;
            auto pubGroup = std::make_shared<PublishGroup>(componentId, comp_id, frequency, offset_time); // Frequency and offset are hardcoded to 0 for now.

            auto rawRegisters = getMapValue<std::vector<std::any>>(componentData, "registers");
            if (!rawRegisters.has_value()) {
                throw std::runtime_error("Missing registers in component data.");
            }

            for (const std::any& rawRegister : rawRegisters.value()) {
                if (rawRegister.type() == typeid(std::map<std::string, std::any>)) {
                    std::map<std::string, std::any> registerData = std::any_cast<std::map<std::string, std::any>>(rawRegister);

                    CompressedItem compressedItem;
                    int device_id;
                    getItemFromMap(registerData, "device_id", device_id, comp_device_id, true, true,  debug);
                    compressedItem.device_id = device_id;
                    

                    std::string rtype;
                    if (getItemFromMap(registerData, "type", rtype, std::string(), true, false,  debug).has_value()) {
                        compressedItem.rtype = rtype;
                    }

                    int start_offset;
                    if (getItemFromMap(registerData, "starting_offset", start_offset, 0, true, false,  debug).has_value()) {
                        compressedItem.start_offset = start_offset;
                    }
                    compressedItem.end_offset = compressedItem.start_offset; // Assuming end_offset is the same for now.

                    int number_of_registers;
                    if (getItemFromMap(registerData, "number_of_registers", number_of_registers, 1, true, false,  debug).has_value()) {
                        compressedItem.end_offset = compressedItem.start_offset + number_of_registers - 1;
                        compressedItem.number_of_registers = number_of_registers;
                    }
                    auto rawMaps = getMapValue<std::vector<std::any>>(registerData, "map");
                    if (!rawMaps.has_value()) {
                        continue;  // No maps to process in this register
                    }

                    for (const std::any& rawMap : rawMaps.value()) {
                        if (rawMap.type() == typeid(std::map<std::string, std::any>)) {
                            std::map<std::string, std::any> mapData = std::any_cast<std::map<std::string, std::any>>(rawMap);

                            std::string id; 
                            getItemFromMap(mapData, "id", id, std::string("some_id"), true, false, false);
                            // need to push this into the values vec structure[componentId][id][item->id] = item;
                            // 

                            std::cout << " adding comp :["<< componentId << "]  id :[" <<comp_id << "] item : "<< id << std::endl;
                            auto itemP = findItem(items, componentId, comp_id, id);
                            if (itemP)
                            {

                                std::cout << " found comp :["<< componentId << "]  id :[" <<comp_id << "] item : "<< id << std::endl;
                                std::any foundValue = itemP->value;
                                compressedItem.values.emplace_back(itemP, foundValue);
                            }
                        }
                    }

                    // Add the CompressedItem to the pubGroup
                    pubGroup->addCompressedItem(compressedItem);
                }
            }
            publishGroups.push_back(pubGroup);
        }
    }

    return true;
}





/*
uri:/components/comp_sel_2440/fuse_monitoring  body:false"
uri:/components/comp_sel_2440/fuse_monitoring  body:{value:false}"
uri:/components/comp_sel_2440                  body:{fuse_monitoring:false}"
uri:/components/comp_sel_2440                  body:{fuse_monitoring: {value:false}}"

uri:/components/comp_sel_2440/voltage          body:1234"
uri:/components/comp_sel_2440/voltage          body:{value:1234}"
uri:/components/comp_sel_2440                  body:{voltage:1234}"
uri:/components/comp_sel_2440                  body:{voltage: {value:1234}}"

uri:/components/comp_sel_2440/status           body:"running""
uri:/components/comp_sel_2440/status           body:{value:"running"}"
uri:/components/comp_sel_2440                  body:{status:"running"}"
uri:/components/comp_sel_2440                  body:{status: {value:"running"}}"

uri:/components/comp_sel_2440                  body:{voltage:1234, fuse_monitoring:false, status:"running" }"
uri:/components/comp_sel_2440                  body:{voltage:{value:1234}, fuse_monitoring:{value:false}, status:{value:"running" }}"
*/



// use the split thing these are deprecated
// from parseFimsMessage
// auto uri_tokens = split_string(uri, '/');
    // if (uri_tokens.size() < 2) {
    //     std::cout << "uri_tokens fail size :"<< uri_tokens.size() <<" uri :"<< uri << std::endl;
    //     return false; // Invalid URI
    // }




// std::map<std::string, std::any> mergeMap(const std::string& uri1, const std::string& body1, const std::string& uri2, const std::string& body2) {
//     std::map<std::string, std::any> baseMap = parseMessage(uri1, body1);
//     std::map<std::string, std::any> secondaryMap = parseMessage(uri2, body2);
    
//     mergeSubMaps(baseMap, secondaryMap);

//     return baseMap;
// }








// utility may not be used now
// deprecated
template <typename T>
std::optional<T> getMapFeature(const std::map<std::string, std::any>& dataMap, const std::string& key) {
    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;  // Return an empty optional if the type is wrong
        }
    }
    return std::nullopt;  // Return an empty optional if the key is not found
}
//auto connectionName = getMapFeature<std::string>(data, "connection.name");
//auto maxNumConnections = getMapFeature<int32_t>(data, "connection.max_num_connections");
// deprecated
std::string getMapFeatureType(const std::map<std::string, std::any>& dataMap, const std::string& key) {
    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        const std::type_info& type = it->second.type();
        return type.name();
    }
    return "Unknown"; // or any other indication for non-existing keys
}

// deprecated
std::optional<std::vector<std::any>> getMapArrayFeature(const std::map<std::string, std::any>& dataMap, const std::string& key) {
    auto it = dataMap.find(key);
    if (it != dataMap.end()) {
        try {
            return std::any_cast<std::vector<std::any>>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;  // Return an empty optional if the type is wrong
        }
    }
    return std::nullopt;  // Return an empty optional if the key is not found
}
//deprecated
std::optional<std::any> getMapArrayFeatureItem(const std::map<std::string, std::any>& dataMap, const std::string& key, size_t index) {
    auto arrayOpt = getMapArrayFeature(dataMap, key);
    if (arrayOpt.has_value() && index < arrayOpt.value().size()) {
        return arrayOpt.value()[index];
    }
    return std::nullopt;  // Return an empty optional if the item doesn't exist
}

//deprecated
// auto registers = getMapArrayFeature(data, "components.registers");
size_t getMapArrayFeatureSize(const std::map<std::string, std::any>& dataMap, const std::string& key) {
    auto arrayOpt = getMapArrayFeature(dataMap, key);
    if (arrayOpt.has_value()) {
        return arrayOpt.value().size();
    }
    return 0;  // Return 0 if the array feature doesn't exist
}



// debug routine 
std::string gcom_show_FirstLevel(const std::map<std::string, std::any>& m, std::string key) {
    std::ostringstream oss;

    oss << "\""<<key<<"\" :{\n";

    bool isFirstItem = true;
    for (const auto& [key, value] : m) {
        if (!isFirstItem) {
            oss << ",\n";
        }
        oss << "   \"" << key << "\": ";

        if (value.type() == typeid(int)) {
            oss << std::any_cast<int>(value);
        } else if (value.type() == typeid(int64_t)) {
            oss << std::any_cast<int64_t>(value);
        } else if (value.type() == typeid(double)) {
            oss << std::any_cast<double>(value);
        } else if (value.type() == typeid(std::string)) {
            oss << "\"" << std::any_cast<std::string>(value) << "\"";  // Quoted because it's a string in JSON
        } else {
            // Unknown types will not be added to the JSON to maintain valid format
            continue;
        }

        isFirstItem = false;
    }

    oss << "\n}\n";
    std::cout << oss.str();

    return oss.str();
}


// deprecated
std::optional<int> getMapInt1(const std::map<std::string, std::any>& m, const std::string& query) {
    const std::any* current = &m.at(query); // Assuming the key exists

    if (current->type() == typeid(int)) {
        return std::any_cast<int>(*current);
    } 

    return std::nullopt;
}
// deprecated
std::optional<int> getMapInt(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(int) && !std::getline(ss, key, '.')) {
            return std::any_cast<int>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

// deprecated
std::optional<bool> getMapBool(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(bool) && !std::getline(ss, key, '.')) {
            return std::any_cast<bool>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

//deprecated
std::optional<std::map<std::string, std::any>> getMapObj1(const std::map<std::string, std::any>& m, const std::string& query) {
    const std::any* current = &m.at(query); // Assuming the key exists

    if (current->type() == typeid(std::map<std::string, std::any>)) {
        return std::any_cast<std::map<std::string, std::any>>(*current);
    } 

    return std::nullopt;
}
//deprecated
std::optional<std::map<std::string,std::any>> getMapObj(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(std::map<std::string, std::any>) && !std::getline(ss, key, '.')) {
            return std::any_cast<std::map<std::string,std::any>>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}
//deprecated
std::optional<std::vector<std::any>> getMapArray1(const std::map<std::string, std::any>& m, const std::string& query) {
    const std::any* current = &m.at(query); // Assuming the key exists

    if (current->type() == typeid(std::vector<std::any>)) {
        return std::any_cast<std::vector<std::any>>(*current);
    } 

    return std::nullopt;
}

//deprecated
std::optional<std::vector<std::any>> getMapArray(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(std::vector<std::any>) && !std::getline(ss, key, '.')) {
            return std::any_cast<std::vector<std::any>>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

//deprecated
std::optional<std::string> getMapString(const std::map<std::string, std::any>& m, const std::string& query) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return std::nullopt;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else if (it->second.type() == typeid(std::string) && !std::getline(ss, key, '.')) {
            return std::any_cast<std::string>(it->second);
        } else {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

// deprecated
bool getMapType(const std::map<std::string, std::any>& m, const std::string& query, const std::type_info& targetType) {
    std::stringstream ss(query);
    std::string key;
    const std::map<std::string, std::any>* currentMap = &m;

    while (std::getline(ss, key, '.')) {
        auto it = currentMap->find(key);
        if (it == currentMap->end()) {
            return false;
        }

        if (it->second.type() == typeid(std::map<std::string, std::any>)) {
            currentMap = &std::any_cast<const std::map<std::string, std::any>&>(it->second);
        } else {
            return it->second.type() == targetType;
        }
    }

    return false;
}

int some_test (std::map<std::string, std::any> &m)
{
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


// "bit_strings": [
//     "Stopped",
//     "Running"
// ]






// std::vector<std::string> extract_bitstrings(struct cfg::map_struct* map,  std::any& rawStringData) {
//     std::vector<std::string> bit_str;
//     // did we get a vector of strings
//     if (rawStringData.type() == typeid(std::vector<std::any>)) {
//                 std::vector<std::string> rawStrings = std::any_cast<std::vector<std::string>>(rawStringData);
//         for (const std::any& rawString : rawStrings) {
//             if (rawString.type() == typeid(std::string)) {
//                 std::string stringData = std::any_cast<std::string>(rawString);
//                 map->bit_str.push_back(stringData);
//             }
//         }
//     }
//     return map->bit_str;
// }






// may well be deprecated
std::string gcom_show_pubs(struct cfg& myCfg, bool debug = false) {
    std::ostringstream json;
    
    json << "{\n";

    std::vector<std::string> freqStrings;

    for (const auto& freqPair : pubs) {
        int freq = freqPair.first;
        std::ostringstream freqStream;

        freqStream << "\t\"" << freq << "\": {\n";

        std::vector<std::string> offsetStrings;

        for (const auto& offsetPair : freqPair.second) {
            int offset_time = offsetPair.first;
            std::ostringstream offsetStream;

            offsetStream << "\t\t\"" << offset_time << "\": {\n";

            std::vector<std::string> regStrings;

            for (const cfg::reg_struct* regPtr : offsetPair.second) {
                std::ostringstream regStream;
                regStream << "\t\t\t\"" << regPtr->type << "\": [ [" << regPtr->device_id<< "," <<regPtr->starting_offset << ", " << regPtr->number_of_registers << "] ]";
                regStrings.push_back(regStream.str());
            }

            offsetStream << join(",\n", regStrings.begin(), regStrings.end());
            offsetStream << "\n\t\t}";

            offsetStrings.push_back(offsetStream.str());
        }

        freqStream << join(",\n", offsetStrings.begin(), offsetStrings.end());
        freqStream << "\n\t}";

        freqStrings.push_back(freqStream.str());
    }

    json << join(",\n", freqStrings.begin(), freqStrings.end());
    json << "\n}";

    if(debug) std::cout << "\"pubs\" : \n" << json.str() << "\n";

    return json.str();
}


// may well be deprecated
struct type_map* gcom_get_comp(std::string comp, std::string id, bool debug=false) {
    std::ostringstream oss;

    if (comps.find(comp) != comps.end()) {
        if (comps[comp].find(id) != comps[comp].end()) {
            if (debug) {
                oss << "\"gcom_get_comp\" :{\n"
                    << "                  \"status\": \"success\",\n"
                    << "                  \"comp\": \"" << comp << "\",\n"
                    << "                  \"id\": \"" << id << "\"\n"
                    << "                  }";
                std::cout << oss.str() << std::endl;
            }
            return &comps[comp][id];
        }
    }

    if (debug) {
        oss.str("");  // Clear the stringstream for new data
        oss << "\"gcom_get_comp\" :{\n"
            << "                   \"status\": \"error\",\n"
            << "                   \"comp\": \"" << comp    << "\",\n"
            << "                   \"id\": \"" << id        << "\",\n"
            << "                   \"message\": \"comp '" << comp << "' with id '" << id << "' not found\"\n"
            << "                   }";
        std::cout << oss.str() << std::endl;
    }

    return nullptr;
}

// start of the encode / decode story
// encode a var from fims (any) map into a register
// must have at least 4 registers 
//struct cfg::comp_struct item;
//item.starting_bit_pos;
//item.shift;
//item.scale 
//item.normal_set  
//item.is_signed  
//item.invert_mask  
//item.is_float  
//item.is_float64  
//item.is_word_swapped  


// item.uses_masks  new
// item.invert_mask todo
// item.care_mask
// item.is_signed



// all todo

//item.is_bit_string_type  //todo
//item.is_individual_bits  //TODO
//item.bit_index
//item.is_bit_field
//item.bit_str std::vector<std::string>  ->num_bit_strs
//item.bit_str_known std::vector<bool>  ->num_bit_strs
//item.bit_str
//item.is_enum

// to string method for decoded_vals (for publishing/get purposes):
//template<Register_Types Reg_Type>
//static




// contains lots of early ( like last week) stuff may need to revisit

bool gcom_config_test(std::map<std::string, std::any>gcom_map, struct cfg& myCfg)
{
    gcom_show_FirstLevel(gcom_map, "connection");
    if (gcom_map.find("connection") != gcom_map.end())
    {
        auto& connectionAny = gcom_map["connection"];

        if (connectionAny.type() == typeid(std::map<std::string, std::any>)) {
            std::map<std::string, std::any> connectionMap = std::any_cast<std::map<std::string, std::any>>(connectionAny);
            //std::cout << " Connection ... "<< std::endl;
            gcom_show_FirstLevel(connectionMap, "types");
        }
    }
    
    bool ok = true; 
    bool debug = false;
    getItemFromMap(gcom_map, "connection.port",       myCfg.connection.port,       503,                      true,true, debug);
    //ok = 
    getItemFromMap(gcom_map, "connection.ip_address", myCfg.connection.ip_address, std::string("172.3.0.2"), true,true, debug);
    //ok = 
    getItemFromMap(gcom_map, "connection.debug",      myCfg.connection.debug,      false,                    true,true, debug);
    //ok = 
    getItemFromMap(gcom_map, "connection.device_id",  myCfg.connection.device_id,  1,                        true,true, debug);

    extract_components(gcom_map,"components",myCfg);

    // now create a list of types and offsets
    // used when we get a register type and offset  
    gcom_extract_types(myCfg);
    gcom_show_types(myCfg);

    // test this list  
    //auto rtype = 
    gcom_get_type( "Discrete Inputs", 395,  debug);


    // now create a uri/id
    // map a type->offset to a structure with component, register, and a map
    gcom_extract_comps(myCfg);
    //auto ctype = 
    gcom_get_comp( "comp_sel_2440", "fuse_monitoring",  debug);

    // now create pub_lists frequency, offset , components  and pub_map
    // get subs
    gcom_extract_subs(myCfg);
    gcom_show_subs(myCfg,  debug);
    // get pubs
    gcom_extract_pubs(myCfg);
    gcom_show_pubs(myCfg,  debug);

    //
    ItemMap structure;
    extract_structure(structure, gcom_map);
    //ItemMap structure;
    //ItemMap structure = extract_structure(gcom_map);
    printItemMap(structure);

    ItemSharedPtr item = findItem(structure, "/components", "comp_sel_2440", "door_latch");
    if (item) {
        printItem(item);
        //std::cout << "Found item with ID: " << item->id << ", Name: " << item->name << std::endl;
     } else {
         std::cout << "Item not found!" << std::endl;
     }


    return ok;
}
//
// read a config
// send it a uri as from fims listen 
// see if you get a results vector
//
// now this is getting closer to the real thing.
bool gcom_msg_test(std::map<std::string, std::any>gcom_map, struct cfg& myCfg)
{
    // pull out the components into myCfg
    extract_components(gcom_map, "components", myCfg);

    // extract the ItemMap
    ItemMap gcom_cfg;
    extract_structure(gcom_cfg, gcom_map);

    std::vector<std::pair<ItemSharedPtr, std::any>> result;
    parseFimsMessage(gcom_cfg, result, "set", "/components/comp_sel_2440",  "{\"door_latch\": false, \"fire_relay\":true,\"disconnect_switch\":true}");
    parseFimsMessage(gcom_cfg, result, "set", "/components/comp_sel_2440",  "{\"heartbeat\":234}");

    auto res = result.size();
    std::cout << " result size :" << res << std::endl;
    std::cout << " result vector  >>>>:" << std::endl;
    printResultVector(result);

    std::vector<CompressedItem> compressedResult;
    compressResults(result, compressedResult); 

    //TODO encode results on a SET
    std::cout << " compressed vector  >>>>:" << std::endl;
    printCompressedResults(compressedResult);


    bool ok = true;
    return ok;
}



