// slight rework of the memory transport system.
// we have a "simple" memory layout table that allows for bit definitions
// we can read / write to different bits and items
// we now need to package a write to accomodate  the uint16 transport system.
// note that layout table offsets are in unit8 units
// first we collect the write requests into a list 

// order the list by offset
// then create control/data vectors
// 16, 32 bits on a 16 bit  boundary no problem 
// 16, 32 bits not on a 16 bit  boundary  need to mask the first and last bytes
// 8 bits need to mask one or that other
#include <iostream>
#include <fstream>
#include <string>

#include <vector>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// g++ -o test masked_writes.cpp



// Track current offset in static variable
//static uint16_t current_offset = 0;


// Define constants for data types
enum RackDataType {
    RACK_DATA_TYPE_BITFIELD16,
    RACK_DATA_TYPE_BITFIELD32,
    RACK_DATA_TYPE_UINT8_VALUES,
    RACK_DATA_TYPE_UINT16_VALUES,
    RACK_DATA_TYPE_UINT32_VALUES,
    RACK_DATA_TYPE_OFFSET,
    RACK_DATA_TYPE_RESET_OFFSET,
    RACK_DATA_TYPE_NONE
};

// Define a union for data-specific information
union RackDataDetails {
    struct {
        uint16_t bit_start;
        uint16_t bit_size;
    } bitfield;
    uint16_t num_uint16;
};

// Define a struct for RackDataItem
struct RackDataItem {
    char name[32];
    uint32_t offset;
    uint16_t num16;
    RackDataType type;
    RackDataDetails data;
};

// Define macros for adding entries
#define MEM_DEF_BITFIELD16(name, bit_start, bit_size, num16) \
    { #name, current_offset, num16, RACK_DATA_TYPE_BITFIELD16, { .bitfield = {bit_start, bit_size}} }

#define MEM_DEF_BITFIELD32(name, bit_start, bit_size, num16) \
    { #name, current_offset, num16, RACK_DATA_TYPE_BITFIELD32, { .bitfield = {bit_start, bit_size}} }

#define MEM_DEF_UINT8(name, num16) \
    { #name, current_offset, num16, RACK_DATA_TYPE_UINT8_VALUES, { .num_uint16 = num16 } }
    //, current_offset += num16
#define MEM_DEF_UINT16(name, num16) \
    { #name, current_offset, num16, RACK_DATA_TYPE_UINT16_VALUES, { .num_uint16 = num16 } }
    //, current_offset += num16
#define MEM_DEF_UINT32(name, num16) \
    { #name, current_offset, num16, RACK_DATA_TYPE_UINT32_VALUES, { .num_uint16 = num16 } }
    //, current_offset += num16

    // Define a special marker to increment the offset manually
#define INCREMENT_OFFSET(num16) \
    { "__offset", current_offset, num16, RACK_DATA_TYPE_UINT16_VALUES, { .num_uint16 = num16 } }

   // Define a special marker to set the  offset manually
#define SET_OFFSET(num16) \
    { "__offset", current_offset, num16, RACK_DATA_TYPE_UINT16_VALUES, { .num_uint16 = 1 } }


// Write request structure
struct WriteRequest {
    uint32_t offset;     // Offset into shared memory
    uint32_t mask;       // Mask to apply (0xFFFF for full write)
    uint32_t value;      // Value to write
    uint32_t index;     // deata def index
};

#include <algorithm> // For std::sort

void sort_write_requests_by_offset(std::vector<WriteRequest>& write_requests) {
    std::sort(write_requests.begin(), write_requests.end(),
              [](const WriteRequest& a, const WriteRequest& b) {
                  return a.offset < b.offset;
              });
}

//these are decoded write requests that need to be translated into vector commands 
std::vector<WriteRequest> write_requests;



static uint16_t current_offset = 0; // Tracks the current offset dynamically

// Define rack_data
// we may use increment_offset to point to different offsets and loose the num16 number on the end
std::vector<RackDataItem> rack_data;
// RackDataItem rack_data[] =  {
//         MEM_DEF_UINT16(Val1, 2),            // uint16 on boundary OK 
//         MEM_DEF_UINT16(Val2, 2),            // unit16 on boundary OK
//         MEM_DEF_UINT8(Val3, 1),             // uint8  on boundary OK
//         MEM_DEF_UINT8(Val4, 2),             // uint8  off boundary  OK 
//         MEM_DEF_UINT16(Space1, 2),          // this aligns val5 to a 32 bit boundary
//         MEM_DEF_UINT32(Val5, 4),            // uint32 on boundary  OK 
//         MEM_DEF_UINT32(Val6, 4),            // unit32  on bundary OK 
//         MEM_DEF_UINT16(Val7, 2),            // uint16 on boundary OK 
//         MEM_DEF_UINT32(Val8, 4),            // unit32  off uint32 boundary 
//         MEM_DEF_UINT8(Val9, 1),             // uint8  used to throw the next one off boundary
//         MEM_DEF_UINT16(Val10, 2),            // uint16 off boundary 
//         MEM_DEF_UINT32(Val11, 4),            // unit32 off boundary 
//         //MEM_DEF_UINT8(Space2, 2),            // this aligns val5 to  16 bit boundary

//         MEM_DEF_BITFIELD16(Rack_Fault, 14, 1, 1),          // Uses bit 14 at offset 0
//         MEM_DEF_BITFIELD32(Rack_Alarm, 13, 1, 1),          // Uses bit 13 at offset 0
//         MEM_DEF_BITFIELD16(Rack_Fault_Code, 10, 3, 0),     // Uses bits 10-12 at offset 0
//         MEM_DEF_BITFIELD16(Bms_Status, 14, 1, 2),         // Uses bit 14 at offset 0
//         MEM_DEF_BITFIELD16(Bms_Alarm, 13, 1, 0),          // Uses bit 13 at offset 0
//         MEM_DEF_BITFIELD16(Bms_Fault_Code, 10, 3, 0),     // Uses bits 10-12 at offset 0
//         MEM_DEF_UINT16(Rack_Max_Curr, 2),          // Uses 2 uint16_t at offset 1
//         MEM_DEF_UINT16(Rack_Min_Curr, 2),          // Uses 2 uint16_t at offset 3
//         MEM_DEF_UINT16(Rack_Max_Volts, 2),            // Uses 2 uint16_t at offset 5
//         MEM_DEF_UINT16(Rack_Min_Volts, 2),             // Uses 2 uint16_t at offset 7
//         MEM_DEF_UINT8(Rack_Status, 1),             // Uses 2 uint16_t at offset 7
//         MEM_DEF_UINT32(Rack_Tot_Chg, 1),             // Uses 2 uint16_t at offset 7
//         MEM_DEF_UINT32(Rack_Tot_Dis, 1),             // Uses 2 uint16_t at offset 7
//         MEM_DEF_UINT8(Bms_Status, 1)            // Uses 2 uint16_t at offset 7
// };




// Name to index mapping (for fast lookup)
std::unordered_map<std::string, size_t> name_to_index;

// Add name-to-index mapping initialization function
void initialize_name_to_index(std::vector<RackDataItem>& rack_data, size_t num_items) {
    for (size_t i = 0; i < num_items; ++i) {
        name_to_index[rack_data[i].name] = i;
    }
}    



void initialize_rack_data(std::vector<RackDataItem>& rack_data) {
    rack_data.push_back(MEM_DEF_UINT16(Val1, 2));
    rack_data.push_back(MEM_DEF_UINT16(Val2, 2));
    rack_data.push_back(MEM_DEF_UINT8(Val3, 1));
    rack_data.push_back(MEM_DEF_UINT8(Val4, 2));
    rack_data.push_back(MEM_DEF_UINT16(Space1, 2));
    rack_data.push_back(MEM_DEF_UINT32(Val5, 4));
    rack_data.push_back(MEM_DEF_UINT32(Val6, 4));
    rack_data.push_back(MEM_DEF_UINT16(Val7, 2));
    rack_data.push_back(MEM_DEF_UINT32(Val8, 4));
    rack_data.push_back(MEM_DEF_UINT8(Val9, 1));
    rack_data.push_back(MEM_DEF_UINT16(Val10, 2));
    rack_data.push_back(MEM_DEF_UINT32(Val11, 4));

    rack_data.push_back(MEM_DEF_BITFIELD16(Rack_Fault, 14, 1, 1));
    rack_data.push_back(MEM_DEF_BITFIELD32(Rack_Alarm, 13, 1, 1));
    rack_data.push_back(MEM_DEF_BITFIELD16(Rack_Fault_Code, 10, 3, 0));
    rack_data.push_back(MEM_DEF_BITFIELD16(Bms_Status, 14, 1, 2));
    rack_data.push_back(MEM_DEF_BITFIELD16(Bms_Alarm, 13, 1, 0));
    rack_data.push_back(MEM_DEF_BITFIELD16(Bms_Fault_Code, 10, 3, 0));

    rack_data.push_back(MEM_DEF_UINT16(Rack_Max_Curr, 2));
    rack_data.push_back(MEM_DEF_UINT16(Rack_Min_Curr, 2));
    rack_data.push_back(MEM_DEF_UINT16(Rack_Max_Volts, 2));
    rack_data.push_back(MEM_DEF_UINT16(Rack_Min_Volts, 2));
    rack_data.push_back(MEM_DEF_UINT8(Rack_Status, 1));
    rack_data.push_back(MEM_DEF_UINT32(Rack_Tot_Chg, 1));
    rack_data.push_back(MEM_DEF_UINT32(Rack_Tot_Dis, 1));
    rack_data.push_back(MEM_DEF_UINT8(Bms_Status, 1));
}

// json file format
/*
[
    { "name": "Val8", "type": "RACK_DATA_TYPE_UINT32_VALUES", "num": 4 },
    { "name": "Val9", "type": "RACK_DATA_TYPE_UINT8_VALUES", "num": 1 },
    { "name": "Rack_Fault", "type": "RACK_DATA_TYPE_BITFIELD16", "num": 1, "start_bit": 14, "bit_size": 1 },
    { "name": "Rack_Alarm", "type": "RACK_DATA_TYPE_BITFIELD32", "num": 1, "start_bit": 13, "bit_size": 1 }
]
*/

void add_system_def(std::vector<RackDataItem>& rack_data, const std::string& name,
                    RackDataType type, uint16_t num_uint16 = 0, 
                    uint16_t start_bit = 0, uint16_t bit_size = 0);

// Function to initialize system definitions from a JSON file
void initialize_from_json(const std::string& file_path, std::vector<RackDataItem>& rack_data) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open JSON file.");
    }

    nlohmann::json json_data;
    file >> json_data;

    for (const auto& entry : json_data) {
        std::string name = entry.at("name").get<std::string>();
        std::string type_str = entry.at("type").get<std::string>();

        // Check for optional fields
        uint16_t num_uint16 = entry.contains("num") ? entry.at("num").get<uint16_t>() : 0;
        uint16_t start_bit = entry.contains("start_bit") ? entry.at("start_bit").get<uint16_t>() : 0;
        uint16_t bit_size = entry.contains("bit_size") ? entry.at("bit_size").get<uint16_t>() : 0;

        RackDataType type;
        if (type_str == "RACK_DATA_TYPE_UINT8_VALUES") {
            type = RACK_DATA_TYPE_UINT8_VALUES;
        } else if (type_str == "RACK_DATA_TYPE_UINT16_VALUES") {
            type = RACK_DATA_TYPE_UINT16_VALUES;
        } else if (type_str == "RACK_DATA_TYPE_UINT32_VALUES") {
            type = RACK_DATA_TYPE_UINT32_VALUES;
        } else if (type_str == "RACK_DATA_TYPE_BITFIELD16") {
            type = RACK_DATA_TYPE_BITFIELD16;
        } else if (type_str == "RACK_DATA_TYPE_BITFIELD32") {
            type = RACK_DATA_TYPE_BITFIELD32;
        } else {
            throw std::runtime_error("Unknown type in JSON file.");
        }

        add_system_def(rack_data, name, type, num_uint16, start_bit, bit_size);
    }
}

// json_file == rack_data.json
int load_json(std::string json_file) {
    std::vector<RackDataItem> rack_data;

    try {
        initialize_from_json(json_file, rack_data);

        for (const auto& item : rack_data) {
            std::cout << "Name: " << item.name 
                      << ", Offset: " << item.offset
                      << ", Num UInt16: " << item.num16
                      << ", Type: " << item.type
                      << ", Start Bit: " << item.data.bitfield.bit_start
                      << ", Bit Size: " << item.data.bitfield.bit_size << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

/// Function to programmatically add entries
//     add_system_def(rack_data, "Val8", RACK_DATA_TYPE_UINT32_VALUES, 4);
//     add_system_def(rack_data, "Val9", RACK_DATA_TYPE_UINT8_VALUES, 1);
//     add_system_def(rack_data, "Rack_Fault", RACK_DATA_TYPE_BITFIELD16, 1, 14, 1);
//     add_system_def(rack_data, "Rack_Alarm", RACK_DATA_TYPE_BITFIELD32, 1, 13, 1);

void add_system_def(std::vector<RackDataItem>& rack_data, const std::string& name,
                    RackDataType type, uint16_t num_uint16, 
                    uint16_t start_bit, uint16_t bit_size) {
    static uint16_t current_offset = 0;

    // Create a RackDataItem
    struct RackDataItem item;
    memset(&item, 0, sizeof(RackDataItem)); // Initialize to zero

    // Set the name
    strncpy(item.name, name.c_str(), sizeof(item.name) - 1);

    // Set the type and details
    item.type = type;
    item.num16 = num_uint16;
    if (type == RACK_DATA_TYPE_BITFIELD16 || type == RACK_DATA_TYPE_BITFIELD32) {
        item.data.bitfield.bit_start = start_bit;
        item.data.bitfield.bit_size = bit_size;
    } else {
        item.data.num_uint16 = num_uint16;
    }

    // Align offsets if necessary
    if (type == RACK_DATA_TYPE_UINT32_VALUES && current_offset % 2 != 0) {
        current_offset++; // Align to 32-bit boundary
    }
    item.offset = current_offset;

    // Increment the offset for the next item
    current_offset += num_uint16;

    // Add the item to the vector
    rack_data.push_back(item);
}


// Write a value to shared memory, generic 32 bit value , offset defined in the item
void write_value(void*shm_mem, const RackDataItem& item, uint32_t value) {
    uint8_t* memory = static_cast<uint8_t*>(shm_mem) + item.offset;

    switch (item.type) {
        case RACK_DATA_TYPE_BITFIELD16: {
            // Handle bitfield
            uint16_t current_value = *reinterpret_cast<uint16_t*>(memory);
            // todo put limits on bit_start
            uint16_t mask = ((1U << item.data.bitfield.bit_size) - 1) << item.data.bitfield.bit_start;

            // Clear the bits in the range and set the new value
            current_value = (current_value & ~mask) | ((value << item.data.bitfield.bit_start) & mask);
            *reinterpret_cast<uint16_t*>(memory) = current_value;

            std::cout << "Bitfield Write: Value=" << value << ", Mask=0x" << std::hex << mask
                      << ", Result=0x" << current_value << std::endl;
            break;
        }
        case RACK_DATA_TYPE_BITFIELD32: {
            // Handle bitfield
            uint32_t current_value = *reinterpret_cast<uint32_t*>(memory);
            uint32_t mask = ((1U << item.data.bitfield.bit_size) - 1) << item.data.bitfield.bit_start;

            // Clear the bits in the range and set the new value
            current_value = (current_value & ~mask) | ((value << item.data.bitfield.bit_start) & mask);
            *reinterpret_cast<uint32_t*>(memory) = current_value;

            std::cout << "Bitfield Write: Value=" << value << ", Mask=0x" << std::hex << mask
                      << ", Result=0x" << current_value << std::endl;
            break;
        }
        case RACK_DATA_TYPE_UINT8_VALUES:
            // Handle uint8 write
            *reinterpret_cast<uint8_t*>(memory) = static_cast<uint8_t>(value);
            std::cout << "UINT8 Write: Value=" << value << std::endl;
            break;
        case RACK_DATA_TYPE_UINT16_VALUES:
            // Handle uint16 write
            *reinterpret_cast<uint16_t*>(memory) = static_cast<uint16_t>(value);
            std::cout << "UINT16 Write: Value=" << value << std::endl;
            break;
        case RACK_DATA_TYPE_UINT32_VALUES:
            // Handle uint32 write
            *reinterpret_cast<uint32_t*>(memory) = static_cast<uint32_t>(value);
            std::cout << "UINT32 Write: Value=" << value << std::endl;
            break;
        case RACK_DATA_TYPE_OFFSET:
            std::cout << "DUMMY  offset increment" << std::endl;
            break;
        case RACK_DATA_TYPE_RESET_OFFSET:
            std::cout << "DUMMY  offset reset" << std::endl;
            break;
        default:
            std::cerr << "Unknown data type for write operation!" << std::endl;
            break;
    }
}


// Read a value from shared memory
uint32_t read_value(void* shm_mem,const RackDataItem& item) {
    uint8_t* memory = static_cast<uint8_t*>(shm_mem) + item.offset;

    switch (item.type) {
        case RACK_DATA_TYPE_BITFIELD16: {
            // Handle bitfield
            uint16_t current_value = *reinterpret_cast<uint16_t*>(memory);
            uint16_t mask = ((1U << item.data.bitfield.bit_size) - 1) << item.data.bitfield.bit_start;
            uint32_t result = (current_value & mask) >> item.data.bitfield.bit_start;

            std::cout << "Bitfield Read: Value=0x" << std::hex << result
                      << ", Mask=0x" << mask << ", Original=0x" << current_value << std::endl;
            return result;
        }
        case RACK_DATA_TYPE_BITFIELD32: {
            // Handle bitfield
            uint32_t current_value = *reinterpret_cast<uint32_t*>(memory);
            uint32_t mask = ((1U << item.data.bitfield.bit_size) - 1) << item.data.bitfield.bit_start;
            uint32_t result = (current_value & mask) >> item.data.bitfield.bit_start;

            std::cout << "Bitfield Read: Value=0x" << std::hex << result
                      << ", Mask=0x" << mask << ", Original=0x" << current_value << std::endl;
            return result;
        }

        case RACK_DATA_TYPE_UINT8_VALUES:
            // Handle uint16 read
            return *reinterpret_cast<uint8_t*>(memory);
        case RACK_DATA_TYPE_UINT16_VALUES:
            // Handle uint16 read
            return *reinterpret_cast<uint16_t*>(memory);
        case RACK_DATA_TYPE_UINT32_VALUES:
            // Handle uint16 read
            return *reinterpret_cast<uint32_t*>(memory);
        case RACK_DATA_TYPE_OFFSET:
        case RACK_DATA_TYPE_RESET_OFFSET:
            // Handle uint32 read
            return 0;
        default:
            std::cerr << "Unknown data type for read operation!" << std::endl;
            return 0;
    }
}

//std::vector<RackDataItem>& rack_data
uint32_t read_value_by_index(std::vector<RackDataItem>& rack_data, void* shm_mem, size_t index) {
    const RackDataItem& item = rack_data[index];
    return read_value(shm_mem, item);

}

uint32_t read_value_by_name(std::vector<RackDataItem>&rack_data, void* shm_mem, const char* name) {

    auto it = name_to_index.find(name);
    if (it == name_to_index.end()) {
        throw std::runtime_error("Invalid name: " + std::string(name));
    }
    size_t index = it->second;
    return read_value_by_index(rack_data, shm_mem, index);
}

void  write_value_by_index(std::vector<RackDataItem>& rack_data, void* shm_mem, size_t index, uint32_t value) {
    const RackDataItem& item = rack_data[index];
    return write_value(shm_mem, item, value);
}


void write_value_by_name(std::vector<RackDataItem>& rack_data, void* shm_mem, const char* name, uint32_t value) {
    auto it = name_to_index.find(name);
    if (it == name_to_index.end()) {
        throw std::runtime_error("Invalid name: " + std::string(name));
    }
    size_t index = it->second;
    write_value_by_index(rack_data, shm_mem, index, value);
}

// this one does all the work
// it generates multiple write requests , if needed , for the same offset  . The merge will compact them
void add_write_request_by_index(std::vector<RackDataItem>& rack_data, void* shm_mem, uint32_t index, uint32_t value) {
    const RackDataItem& item = rack_data[index];

    uint32_t mask = 0xffffffff;
    uint32_t aligned_value = value;
    uint16_t offset = item.offset;
    if (item.type == RACK_DATA_TYPE_UINT8_VALUES) 
    {
        mask = 0xff;
        if(item.offset & 1) 
        {
            offset--;
            mask = 0xff00;
            aligned_value <<= 8;
        }

    } else if (item.type == RACK_DATA_TYPE_UINT16_VALUES) 
    {
        mask = 0xffff;
        if(item.offset & 1) 
        {
            // this becomes two write requests one for each part of the object
            //offset--;
            write_requests.push_back({item.offset, 0xff00, (value&0xff)<<8, index});
            offset++;
            mask = 0x00ff;
            aligned_value = (value & 0xff00)>>8;
        }
    } else if (item.type == RACK_DATA_TYPE_UINT32_VALUES) 
    {
        // this is either two writes or three
        mask = 0xffff;
        if(item.offset & 1)   // three writes
        {
            // this becomes three write requests one for each part of the object
            //offset--;
            write_requests.push_back({item.offset, 0xff00, (value&0xff)<<8, index});
            offset++;
            write_requests.push_back({item.offset+1, 0xffff, (value>>8 &0xffff), index});
            offset += 2;
            mask = 0x00ff;
            aligned_value = (value>>16 & 0xff);
        }
        else  // two writes
        {
            write_requests.push_back({item.offset, 0xffff, (value&0xffff), index});
            offset += 2;
            mask = 0xffff;
            aligned_value = (value>>16 & 0xffff);
        }


    } else if (item.type == RACK_DATA_TYPE_BITFIELD16) 
    {
        mask = ((1 << item.data.bitfield.bit_size) - 1) << item.data.bitfield.bit_start;
        aligned_value = (value << item.data.bitfield.bit_start);
        if(item.offset & 1)   // two writes
        {
            // this becomes two write requests one for each part of the object
            //offset--;
            auto mymask = (mask&0xff)<<8;
            if(mymask)
                write_requests.push_back({item.offset, mymask, (aligned_value&0xff)<<8, index});

            printf(" Bitfield16  #1 mask 0x%04x   value 0x%04x  offset %d \n", mymask, (aligned_value&0xff)<<8, item.offset);

            offset++;
            mask = (mask&0xff00)>>8;
            aligned_value = (aligned_value & 0xff00)>>8;
            printf(" Bitfield16  #2 mask 0x%04x   value 0x%04x  offset %d \n", mask, aligned_value, offset);
        }
    } else if (item.type == RACK_DATA_TYPE_BITFIELD32 ) 
    {
        mask = ((1 << item.data.bitfield.bit_size) - 1) << item.data.bitfield.bit_start;
        aligned_value = (value << item.data.bitfield.bit_start);

        if(item.offset & 1)   // three writes
        {
            // this becomes three write requests one for each part of the object
            //offset--;
            auto mymask = (mask&0xff)<<8;
            if(mymask)
                write_requests.push_back({item.offset, mymask, (aligned_value&0xff)<<8, index});
            printf(" Bitfield32  #1 mask 0x%04x   value 0x%04x  offset %d \n", mymask, (aligned_value&0xff)<<8, item.offset);
            offset++;
            mymask = (mask)>>8 & 0xffff;
            if(mymask)
                write_requests.push_back({item.offset+1, mymask, (aligned_value>>8 &0xffff), index});
            printf(" Bitfield32  #2 mask 0x%04x   value 0x%04x  offset %d \n", mymask, (aligned_value>>8&0xffff), item.offset+1);
            offset += 2;
            mask = (mask)>>24 & 0xff;
            aligned_value = (aligned_value>>24 & 0xff);
            printf(" Bitfield32  #3 mask 0x%04x   value 0x%04x  offset %d \n", mask, aligned_value, item.offset);
        }
        else   // two writes
        {
            auto mymask = (mask&0xffff);
            if(mymask)
                write_requests.push_back({item.offset, 0xffff, (aligned_value&0xffff), index});
            offset += 2;
            mask = (mask>>16)&0xffff;
            aligned_value = (aligned_value>>16 & 0xffff);
        }
    }
    // Align the value to the bitfield's position
    //printf ( " Offset %04d Index %04d Mask  %08x    Value %08x (%d)\n", item.offset , index, mask , aligned_value, value);
    if(mask)
        write_requests.push_back({offset, mask, aligned_value, index});
}

void show_requests(std::vector<WriteRequest>& write_requests)
{
    for (auto &it : write_requests)
    {
        printf ( " Offset %04d Index %04d Mask  %08x    Value %x\n", it.offset , it.index, it.mask , it.value);

    }
}

void merge_requests(std::vector<WriteRequest>& merged_requests, std::vector<WriteRequest>& write_requests) {
    if (write_requests.empty()) {
        return;
    }

    // Ensure the write_requests are sorted by offset
    std::sort(write_requests.begin(), write_requests.end(),
              [](const WriteRequest& a, const WriteRequest& b) {
                  return a.offset < b.offset;
              });

    // Initialize with the first request
    WriteRequest current = write_requests[0];

    for (size_t i = 1; i < write_requests.size(); ++i) {
        const WriteRequest& req = write_requests[i];

        if (req.offset == current.offset) {
            current.mask |= req.mask;
            current.value = ((current.value & ~req.mask) | (req.value & req.mask));
        } else if (req.offset == current.offset+1) {
            current.mask |= req.mask;
            current.value = (current.value |req.value);
            //merged_requests.push_back(current);
            //current = req;
        } else {
            // Push the merged request and move to the next offset
            merged_requests.push_back(current);
            current = req;
        }
    }

    // Push the last merged request
    merged_requests.push_back(current);
}

void add_write_request_by_name(std::vector<RackDataItem>& rack_data,void* shm_mem, const char* name, uint32_t value) {

    auto it = name_to_index.find(name);
    if (it == name_to_index.end()) {
        throw std::runtime_error("Invalid name: " + std::string(name));
    }
    size_t index = it->second;
    add_write_request_by_index(rack_data, shm_mem, index, value);
}

// Function to calculate offsets
// offsets are defines as uint8 boundaries
void calculate_offsets(std::vector<RackDataItem>&  data, size_t count) {
    uint16_t current_offset = 0;
    RackDataType previous_type = RACK_DATA_TYPE_NONE; // Assume starting with bitfield

    for (int i = 0; i < count; ++i) {
        printf( " Index %d  current_offset %d num16 %d\n", i , current_offset,data[i].num16);
        // Align offset if transitioning between types
        if (previous_type != RACK_DATA_TYPE_NONE) {
            if (data[i].type != previous_type) {
                if (previous_type == RACK_DATA_TYPE_UINT8_VALUES) {
                    current_offset++;
                } else if (previous_type == RACK_DATA_TYPE_UINT16_VALUES) { 
                    current_offset+=2;
                } else if (previous_type == RACK_DATA_TYPE_UINT32_VALUES) { 
                    current_offset+=4;
               } else if (previous_type == RACK_DATA_TYPE_BITFIELD32) { 
                    current_offset+=4;
                } else if (previous_type == RACK_DATA_TYPE_BITFIELD16) { 
                    current_offset+=2;               
                }
            } else {
                // only move offsets on non bitfield types or bitfields witn num_uint16 set
                if (data[i].type == RACK_DATA_TYPE_UINT8_VALUES) {
                    current_offset++;
                } else if (data[i].type == RACK_DATA_TYPE_UINT16_VALUES) { 
                    current_offset+=2;
                } else if (data[i].type == RACK_DATA_TYPE_UINT32_VALUES) { 
                    current_offset+=4;
                } else if (data[i].type ==  RACK_DATA_TYPE_BITFIELD32) {
                    current_offset+=data[i].data.num_uint16;
                } else if (data[i].type ==  RACK_DATA_TYPE_BITFIELD16) {
                    current_offset+=data[i].num16;
                // run  absolute and releative adjustments 
                } else if (data[i].type == RACK_DATA_TYPE_OFFSET) {
                    current_offset+= data[i].num16;
                    
                } else if (data[i].type == RACK_DATA_TYPE_RESET_OFFSET) {
                    current_offset = data[i].num16;

                }
            }
        }
        // Update offset and set for the current item
        data[i].offset = current_offset;

        // Update previous type
        previous_type = data[i].type;
    }
}

// decode , detects masked (high order bit) and jump, TODO reset count
int masked_decode(void* shm_mem, std::vector<uint16_t>& control, std::vector<uint16_t>& data) {
    // Check for invalid input
    if (!shm_mem) {
        std::cerr << " error: null shared memory address " << std::endl;
        return -1; // Invalid input
    }
    if (control.empty()) {
        std::cerr << " error: control vector empty" << std::endl;
        return -1; // Invalid input
    }
    if (data.empty()) {
        std::cerr << " error: data vector empty " << std::endl;
        return -1; // Invalid input
    }

    uint16_t* mem_ptr = reinterpret_cast<uint16_t*>(shm_mem); // Cast memory to uint16_t pointer
    size_t data_index = 0; // Index into the data vector

    // Iterate through the control vector
    for (size_t i = 0; i < control.size(); i += 2) {
        uint16_t offset = control[i];      // Offset into shm_mem
        uint16_t count = control[i + 1];  // Number of data pairs to process
        uint16_t mask;

        // Handle dummy entry for large offset adjustments
        if (offset == 0xFFFE && count == 0) {
        // Move the pointer to the specified offset
            mem_ptr += offset;
            continue; // Skip to the next control entry
        }

        // Move the pointer to the specified offset
        mem_ptr += offset;

        // Check if high-order bit is set in count (masked or unmasked decode)
        bool is_masked = (count & 0x8000) != 0;
        count &= 0x7FFF; // Remove the high-order bit to get the actual count

        if (is_masked) {
            // todo check size
            mask = control[i + 2];  // Number of data pairs to process
            i++;
            // Process masked data pairs
            for (uint16_t j = 0; j < count; ++j) {
                // Ensure we have enough data in the data vector
                // if (data_index + 1 >= data.size()) {
                //     std::cerr << "Error: Insufficient data in the data vector for masked decode!" << std::endl;
                //     return -1;
                // }
                if (data_index  >= data.size()) {
                    std::cerr << "Error: Insufficient data in the data vector for masked decode!" << std::endl;
                    return -1;
                }

                //uint16_t mask = data[data_index];       // Data mask
                uint16_t value = data[data_index]; // Data value

                // Apply the mask and value to the current memory location
                *mem_ptr = (*mem_ptr & ~mask) | (value & mask);

                // Move to the next data pair
                data_index++;

                // Increment the memory pointer
                ++mem_ptr;
            }
        } else {
            // Process unmasked data
            for (uint16_t j = 0; j < count; ++j) {
                // Ensure we have enough data in the data vector
                if (data_index >= data.size()) {
                    std::cerr << "Error: Insufficient data in the data vector for unmasked decode!" << std::endl;
                    return -1;
                }

                uint16_t value = data[data_index]; // Data value

                // Write the value directly to the memory location
                *mem_ptr = value;

                // Move to the next data value
                data_index++;

                // Increment the memory pointer
                ++mem_ptr;
            }
        }
    }

    return 0; // Success
}

void print_data(std::vector<RackDataItem>& rack_data, int num)
{

  // Print the rack_data to verify
    std::cout   << "Name" 
                << "\t\tOffset "
                << "\tType "
                << "\tNum16 "
                << "\tDetails  "
                << std::endl;

    for (int i = 0; i < num; i++) {
        auto item = rack_data[i];
        std::string stype;
        switch ( item.type)
        {
            case RACK_DATA_TYPE_UINT8_VALUES:
                stype = "Uint8";
                break;
            case RACK_DATA_TYPE_UINT16_VALUES:
                stype = "Uint16";
                break;
            case RACK_DATA_TYPE_UINT32_VALUES:
                stype = "Uint32";
                break;
            case RACK_DATA_TYPE_BITFIELD16:
                stype = "Bits16";
                break;
            case RACK_DATA_TYPE_BITFIELD32:
                stype = "Bits32";
                break;
            default:
                stype =  "Unknown";
                break;
        }
        std::cout   << item.name << "\t"
                    << item.offset<< "\t"
                    << stype<< "\t"
                    << item.num16 << "\t";

        if (item.type == RACK_DATA_TYPE_BITFIELD16) {
            std::cout << "Start Bit: " << item.data.bitfield.bit_start
                      << ", Size: " << item.data.bitfield.bit_size;
        } else if (item.type == RACK_DATA_TYPE_BITFIELD32) {
            std::cout << "Start Bit: " << item.data.bitfield.bit_start
                      << ", Size: " << item.data.bitfield.bit_size;
        } 
        std::cout << "\n";
    }

}


void test_generate_vectors(const std::vector<WriteRequest>& merged_requests,
                      std::vector<uint16_t>& control_vec,
                      std::vector<uint16_t>& data_vec,
                    std::vector<RackDataItem>& data_type_table, int num_items) {
    uint16_t last_offset = 0;      // Tracks the last offset
    uint16_t last_count = 0;       // Tracks the last count
    uint16_t last_mask = 0xFFFF;   // Tracks the last mask (default unmasked)
    bool is_last_masked = false;   // Tracks whether the last operation was masked

    for (const auto& req : merged_requests) {
        uint16_t offset = req.offset / 2; // Convert to uint16_t boundary
        bool is_masked = req.mask != 0xFFFF; // Determine if this is a masked write

        // Handle large offset jumps
        if (offset > last_offset + last_count && (offset - (last_offset + last_count)) > 0xFFFD) {
            control_vec.push_back(0xFFFE); // Dummy jump offset
            control_vec.push_back(0);     // Count 0 for dummy entry
            last_offset += 0xFFFE;        // Adjust the offset
            last_count = 0;               // Reset count after jump
        }

        // Check if we can merge this entry with the last one
        if (offset == last_offset + last_count &&
            is_masked == is_last_masked &&
            (!is_masked || (is_masked && req.mask == last_mask))) {
            // Merge: Increment count
            last_count++;
            control_vec[control_vec.size() - 1] = (is_masked ? 0x8000 : 0) | last_count;
        } else {
            // Push the previous entry if we were merging
            if (last_count > 0) {
                if (is_last_masked) {
                    control_vec.push_back(last_mask);
                }
            }

            // Start a new control entry
            control_vec.push_back(offset - last_offset);      // Relative offset
            control_vec.push_back((is_masked ? 0x8000 : 0) | 1); // Initial count
            if (is_masked) {
                control_vec.push_back(req.mask); // Mask for masked writes
            }

            // Reset tracking variables for new entry
            last_offset = offset;
            last_count = 1;
            last_mask = req.mask;
            is_last_masked = is_masked;
        }

        // Add data to the data vector
        data_vec.push_back(req.value);
    }

    // Handle the final entry if it was being merged
    if (last_count > 0 && is_last_masked) {
        control_vec.push_back(last_mask);
    }
}

// turn the merged requests into control and data vectors
// offsets are uint8 but the sm protocol uses uint16
// 
// todo  put mask into control done 
//       skip all 0xffff masks  done
//       merge counts  done
void generate_vectors(const std::vector<WriteRequest>& merged_requests,
                      std::vector<uint16_t>& control_vec,
                      std::vector<uint16_t>& data_vec,
                      std::vector<RackDataItem>&  data_type_table, int num_items) {
    uint16_t last_offset = 0; // Tracks the last offset
    uint16_t last_count = 0;  // Tracks the last count
    uint16_t last_mask = 0xffff;  // Tracks the last mask
    uint16_t merge_count = 0;  // Tracks the merge_count 
    bool do_push;

    for (const auto& req : merged_requests) {
        uint16_t offset = req.offset / 2; // Convert to uint16 boundary
        uint16_t count;

        // Handle large offset jumps
        if (offset > last_offset + last_count && (offset - (last_offset + last_count)) > 0xFFFD) {
            control_vec.push_back(0xFFFE); // Special jump
            control_vec.push_back(0x0000);
            last_offset += 0xFFFE; // Advance the offset pointer
        }
        count = (offset - last_offset);
        //printf( " check offset %x  (req %x) count %d  mask %04x last_mask %04x  \n", offset, req.offset, count, req.mask, last_mask);
        if (offset > 0 && count == 0 && req.mask == last_mask)
        { 
            //control_vec[control_vec.size() - 1]
            merge_count = 0;
            if(req.mask != 0xffff)
                merge_count++;

            uint16_t merge_val = control_vec.size() - 1 - merge_count;
            //uint16_t merge_offset = control_vec.size() - 2 - merge_count;
            //printf( " we could probably merge this one at offset %x  merge_count %d  merge_val %d \n", offset, merge_count,  control_vec[merge_val]);
            //the count we want to increment is either the last or the next to last (mask) value
            // or control_vec[merge_val] increment this and disable the push
            control_vec[merge_val] = control_vec[merge_val]+1;
            do_push = false;//true;
        }
        else
        {
            do_push = true;
            merge_count = 0;
        }
        if(do_push)
        {
            control_vec.push_back(offset - last_offset);
            if(req.mask== 0xffff)
            {
                control_vec.push_back(1);  // this is not a masked data opeation
            }
            else
            {
                control_vec.push_back(1|0x8000);  // this is a masked data opeation
                control_vec.push_back(req.mask);
            }
        }
        last_mask = req.mask;

        data_vec.push_back(req.value);
        last_offset = offset + 1;
        last_count = 1;
    }
}

void show_vector(const std::vector<uint16_t>& vec) {
    //std::cout << "Vector Contents:" << std::endl;

    for (size_t i = 0; i < vec.size(); ++i) {
        // Print index and value in hexadecimal
        std::cout << "[" << std::setw(4) << std::setfill('0') << i << "] "
                  << "0x" << std::setw(4) << std::setfill('0') << std::hex << vec[i]
                  << std::endl;
    }
    std::cout << std::dec; // Reset to decimal format for other outputs
}


void run_as_test(uint16_t* shm_area, uint16_t* targ_area)
{
     size_t num_items = rack_data.size();
        // todo we dont need masks
    //write_value_by_name(rack_data, shm_area, "Rack_Max_Curr", 0x345);
    uint16_t *rtd = (uint16_t*)shm_area;
    rtd[0] = 0x234;

    //rtd = (uint32_t*)(shm_area+4);
    rtd[1] = 0x432;

    uint8_t *std = (uint8_t*)shm_area;
    std[4] = 0x56;

    //rtd = (uint32_t*)(shm_area+4);
    std[5] = 0x78;

   uint32_t *utd = (uint32_t*)shm_area;

    utd[2] = 0x12345678; 
    utd[3] = 0x87654321; 

    write_value_by_name(rack_data, shm_area, "Rack_Tot_Dis", 0x234);
    write_value_by_name(rack_data, shm_area, "Val8", 0x55);
    write_value_by_name(rack_data, shm_area, "Val9", 0x66);  // here starts the fun Val9 == uint32->Uint8
    write_value_by_name(rack_data, shm_area, "Val10", 0x77); // val10 == uint16             uint8 -> unt16 
    write_value_by_name(rack_data, shm_area, "Val11", 0x88); // val11 == uint32             uint16->uint32
    write_value_by_name(rack_data, shm_area, "Rack_Fault", 0x1); // val11 == uint32             uint16->uint32
    write_value_by_name(rack_data, shm_area, "Rack_Alarm", 0x0); // val11 == uint32             uint16->uint32
    // write_value_by_name(rack_data, shm_area, "Bms_Status", 0x23);
    // write_value_by_name(rack_data, shm_area, "Rack_Status", 0x123);
    // write_value_by_name(rack_data, shm_area, "Rack_Fault", 0x1);
    // write_value_by_name(rack_data, shm_area, "Rack_Alarm", 0x1);
    // write_value_by_name(rack_data, shm_area, "Rack_Min_Curr", 0x1234);
 
    add_write_request_by_name(rack_data, shm_area, "Val1", 0x234);      // Full uint16_t
    add_write_request_by_name(rack_data, shm_area, "Val2", 0x432);      // Full uint16_t
//    printf(" write 1 ok \n");
    add_write_request_by_name(rack_data, shm_area, "Val3", 0x56);      // Full uint8_t
    add_write_request_by_name(rack_data, shm_area, "Val4", 0x78);      // Full uint8_t

//    printf(" write 2 ok \n");

    add_write_request_by_name(rack_data, shm_area, "Val5", 0x12345678);      // Full uint16_t
    add_write_request_by_name(rack_data, shm_area, "Val6", 0x87654321);      // Full uint16_t
    add_write_request_by_name(rack_data, shm_area, "Val8", 0x55);      // Full uint16_t

    add_write_request_by_name(rack_data, shm_area, "Val9", 0x66);  // here starts the fun  uint8 
    add_write_request_by_name(rack_data, shm_area, "Val10", 0x77); // unit16    17 - 18
    add_write_request_by_name(rack_data, shm_area, "Val11", 0x88); // uint32    19 - 1c
    add_write_request_by_name(rack_data, shm_area, "Rack_Fault", 0x1); // uint32    19 - 1c
    add_write_request_by_name(rack_data, shm_area, "Rack_Alarm", 0x0); // uint32    19 - 1c

    //shm_area[b] = 0x7766  ==> targ_area[b] = 0x7700   // should be 7766
    //shm_area[c] = 0x8800  ==> targ_area[c] = 0xff88   // should be 88ff
    //shm_area[d] = 0x0  ==> targ_area[d] = 0x0
    //shm_area[e] = 0x0  ==> targ_area[e] = 0xff00

    add_write_request_by_name(rack_data, shm_area, "Rack_Tot_Dis", 0x234);      // Full uint16_t
    // add_write_request_by_name(rack_data, shm_area, "Bms_Status", 0x23);      // Full uint16_t
    // add_write_request_by_name(rack_data, shm_area,"Rack_Status", 0x123 /*0x4000*/); 
    // add_write_request_by_name(rack_data, shm_area,"Rack_Fault", 0x1 /*0x4000*/); // Bit 14
    // add_write_request_by_name(rack_data, shm_area,"Rack_Alarm", 0x1 /*0x2000*/);  // Bit 13
    // add_write_request_by_name(rack_data, shm_area, "Rack_Min_Curr", 0x1234);      // Full uint16_t

    printf(" Unsorted requests \n");

    show_requests(write_requests);
    printf(" ------------------------\n");
    sort_write_requests_by_offset(write_requests);

    printf(" Sorted requests \n");
    show_requests(write_requests);
    printf(" ------------------------\n");
    std::vector<WriteRequest> merged_requests;
    
    merge_requests(merged_requests, write_requests);
    printf(" merged requests \n");
    show_requests(merged_requests);
    printf(" ------------------------\n");



    


    // Control vector (offset, count pairs)
    std::vector<uint16_t> control;
    // Data vector (mask/value pairs)
    std::vector<uint16_t> data;


    generate_vectors(merged_requests,
                      control,
                      data,
                      rack_data, num_items);



    printf(" control vector \n");
    show_vector(control);
    printf(" ---------\n");
    printf(" data vector \n");
    show_vector(data);
    printf(" ---------\n");

    // Call the function
    int result = masked_decode(targ_area, control, data);


    control.clear();
    data.clear();    
    // Call the function
    //int result = masked_decode(shm_area, control, data);


    // Check for errors
    if (result == 0) {
        std::cout << "Memory after decode:" << std::endl;
        for (size_t i = 0; i < 32; ++i) {
            std::cout << "shm_area[" << i << "] = 0x" << std::hex << shm_area[i];
            std::cout << "  ==> targ_area[" << i << "] = 0x" << std::hex << targ_area[i] << std::endl;
        }
    } else {
        std::cerr << "Error occurred during decode!" << std::endl;
    }

   // Print the rack_data to verify
    print_data(rack_data, num_items);


    auto val = read_value_by_name(rack_data, shm_area, "Rack_Max_Curr");
    std::cout << " Value read " << val << "\n";

    val = read_value_by_name(rack_data, shm_area, "Val1");
    std::cout << " Src Val1 read " << val << "\n";

    val = read_value_by_name(rack_data, shm_area, "Val2");
    std::cout << " Src Val2 read " << val << "\n";

    val = read_value_by_name(rack_data, targ_area, "Val1");
    std::cout << " Targ Val1 read " << val << "\n";

    val = read_value_by_name(rack_data, targ_area, "Val2");
    std::cout << " Targ Val2 read " << val << "\n";

    val = read_value_by_name(rack_data, shm_area, "Val3");
    std::cout << " Src Val3 read " << (int)val << "\n";

    val = read_value_by_name(rack_data, shm_area, "Val4");
    std::cout << " Src Val4 read " << (int)val << "\n";

    val = read_value_by_name(rack_data, targ_area, "Val3");
    std::cout << " Targ Val3 read " << (int)val << "\n";

    val = read_value_by_name(rack_data, targ_area, "Val4");
    std::cout << " Targ Val4 read " << (int)val << "\n";


    val = read_value_by_name(rack_data, shm_area, "Val5");
    std::cout << " Src Val5 read " << (int)val << "\n";

    val = read_value_by_name(rack_data, shm_area, "Val6");
    std::cout << " Src Val6 read " << (int)val << "\n";

    val = read_value_by_name(rack_data, targ_area, "Val5");
    std::cout << " Targ Val5 read " << (int)val << "\n";

    val = read_value_by_name(rack_data, targ_area, "Val6");
    std::cout << " Targ Val6 read " << (int)val << "\n";


    val = read_value_by_name(rack_data, shm_area, "Val8");
    printf(" Src %s read 0X%x\n", "Val8", val);

    val = read_value_by_name(rack_data, shm_area, "Val9");
    printf(" Src %s read 0X%x\n", "Val9", val);


    val = read_value_by_name(rack_data, targ_area, "Val8");
    printf(" targ %s read 0X%x\n", "Val8", val);

    val = read_value_by_name(rack_data, targ_area, "Val9");
    printf(" targ %s read 0X%x\n", "Val9", val);

    val = read_value_by_name(rack_data, targ_area, "Val10");
    printf(" targ %s read 0X%x\n", "Val10", val);

    val = read_value_by_name(rack_data, shm_area, "Val11");
    printf(" Src %s read 0X%x\n", "Val11", val);

    val = read_value_by_name(rack_data, targ_area, "Val11");
    printf(" targ %s read 0X%x\n", "Val11", val);

    val = read_value_by_name(rack_data, shm_area, "Rack_Fault");
    printf(" Src %s read 0X%x\n", "Rack_Fault", val);

    val = read_value_by_name(rack_data, targ_area, "Rack_Fault");
    printf(" targ %s read 0X%x\n", "Rack_Fault", val);

    val = read_value_by_name(rack_data, shm_area, "Rack_Alarm");
    printf(" Src %s read 0X%x\n", "Rack_Alarm", val);

    val = read_value_by_name(rack_data, targ_area, "Rack_Alarm");
    printf(" targ %s read 0X%x\n", "Rack_Alarm", val);

    return ;
}

void run_as_server(int port, int json_port);
void run_as_client(char* ip, int port, int json_port);

std::unordered_map<std::string , uint16_t*> shm_map;
std::unordered_map<std::string , std::vector<RackDataItem>*> ref_map;


void register_shm(uint16_t* shm_area, const char* sm_name)
{
    shm_map[sm_name] = shm_area;
}

void register_defs(std::vector<RackDataItem>& rack_data, const char* def_name)
{
    ref_map[def_name] = &rack_data;
}


int main(int argc, char * argv[]) {

    initialize_rack_data(rack_data);
    uint16_t shm_area[1024] = {0};
    uint16_t targ_area[1024] = {0xff};
    size_t num_items = rack_data.size();
    for (size_t i = 0; i < 32; ++i) {
            shm_area[i] = 0;
            targ_area[i] = 0xffff;
    }

    initialize_name_to_index(rack_data, num_items);


    // Calculate offsets dynamically
    //size_t num_items = sizeof(rack_data) / sizeof(rack_data[0]);
    calculate_offsets(rack_data, num_items);
    register_shm(shm_area, "my_shm");
    register_defs(rack_data, "my_def");

    try {
        if (argc == 1) {
            // No arguments: Run as test
            run_as_test(shm_area, targ_area);
        } else if (argc == 3 && strcmp(argv[1], "-t") == 0 && strcmp(argv[2], "server") == 0) {
            // Server mode
            int port = 8080;      // Default data port
            int json_port = 8081; // Default JSON command port

            // std::cout << "Enter server data port (default 8080): ";
            // std::cin >> port;

            // std::cout << "Enter server JSON command port (default 8081): ";
            // std::cin >> json_port;

            run_as_server(port, json_port);
        } else if (argc == 3 && strcmp(argv[1], "-t") == 0 && strcmp(argv[2], "client") == 0) {
            int port = 8080;      // Default data port
            int json_port = 8081; // Default JSON command port
            char* ip = (char*)"127.0.0.1";
            // Client mode
            run_as_client(ip ,port, json_port);
        } else {
            // Invalid arguments
            std::cerr << "Usage:\n";
            std::cerr << "  " << argv[0] << "          Run as test\n";
            std::cerr << "  " << argv[0] << " -t server    Run as server\n";
            std::cerr << "  " << argv[0] << " -t client    Run as client\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

    



#include <stdio.h>
#include <ctype.h>

void hexdump(const void *data, size_t size) {
    const unsigned char *byte = (const unsigned char *)data;
    for (size_t i = 0; i < size; i += 16) {
        // Print the offset
        printf("%08zx  ", i);

        // Print hex values
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", byte[i + j]);
            } else {
                printf("   "); // Padding for alignment
            }
        }

        // Print ASCII representation
        printf(" |");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%c", isprint(byte[i + j]) ? byte[i + j] : '.');
            } else {
                printf(" ");
            }
        }
        printf("|\n");
    }
}


// Function prototypes
void handle_data_connection(int client_socket);
void handle_json_connection(int client_socket);


void reuse_port(int sock)
{
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

}

// Run as server
void run_as_server(int port, int json_port) {
    std::cout << "Starting server on data port: " << port 
              << " and JSON command port: " << json_port << std::endl;

    // Create sockets for data and JSON communication
    int data_socket = socket(AF_INET, SOCK_STREAM, 0);

    /// Enable SO_REUSEADDR and SO_REUSEPORT

    int json_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (data_socket < 0 || json_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    reuse_port(data_socket);
    reuse_port(json_socket);

    sockaddr_in data_addr = {};
    sockaddr_in json_addr = {};

    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = INADDR_ANY;
    data_addr.sin_port = htons(port);

    json_addr.sin_family = AF_INET;
    json_addr.sin_addr.s_addr = INADDR_ANY;
    json_addr.sin_port = htons(json_port);

    // Bind sockets
    if (bind(data_socket, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        perror("Data socket bind failed");
        exit(EXIT_FAILURE);
    }

    if (bind(json_socket, (struct sockaddr*)&json_addr, sizeof(json_addr)) < 0) {
        perror("JSON socket bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen on both sockets
    if (listen(data_socket, 5) < 0 || listen(json_socket, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is running and waiting for connections...\n";

    // Accept connections in separate threads
    std::thread([&]() {
        while (true) {
            int client_socket = accept(data_socket, nullptr, nullptr);
            if (client_socket < 0) {
                perror("Data connection accept failed");
                continue;
            }
            std::thread(handle_data_connection, client_socket).detach();
        }
    }).detach();

    std::thread([&]() {
        while (true) {
            int client_socket = accept(json_socket, nullptr, nullptr);
            if (client_socket < 0) {
                perror("JSON connection accept failed");
                continue;
            }
            std::thread(handle_json_connection, client_socket).detach();
        }
    }).detach();

    // Keep the server running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Handle data connection
void handle_data_connection(int client_socket) {
    char buffer[1024] = {};
    while (true) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cerr << "Data connection closed or error occurred\n";
            close(client_socket);
            break;
        }

        buffer[bytes_received] = '\0';
        std::cout << "Data received: " << buffer << std::endl;
        hexdump(buffer, bytes_received);


        // Echo back the data (example functionality)
        //send(client_socket, buffer, bytes_received, 0);
    }
}

uint16_t* get_mem_from_name(std::string& sm_name)
{

    return shm_map[sm_name];
}

std::vector<RackDataItem>* get_defs_from_name(std::string& ref_name)
{
    return ref_map[ref_name];
}


uint32_t get_sm_value(std::string& sm_name, std::string& reg_name, int num)
{
    uint16_t*shm_mem = get_mem_from_name(sm_name);
    std::vector<RackDataItem>* rack_data = get_defs_from_name(sm_name);
    return read_value_by_name(*rack_data, (void*)shm_mem, reg_name.c_str());
}

// {"action": "get", "name":"Rack_Fault"}
// qt get {"action":"get", "seq": 123, "sm_name": "rack_4", "reg_type": "mb_input", "offset":1000, "num":10}
// socket get {"action":"get", "seq": 123, "sm_name": "rack_4", "reg_name": "Rack_Fault"}
// contains
int handle_get(int client_socket, json& command){
    int seq = -1, num = -1;
    std::string sm_name;
    std::string reg_name;
    uint32_t value;
    if (command.contains("seq")) {
        seq = command["seq"];
    }
    if (command.contains("num")) {
        num = command["num"];
    }
    if (command.contains("sm_name")) {
        sm_name = command["sm_name"];
    }
    if (command.contains("reg_name")) {
        reg_name = command["reg_name"];
    }
    value  = get_sm_value(sm_name, reg_name, num);

    std::stringstream ss;
    // Example GET handling
    ss <<  "{"
        << "\"status\": \"success\""
        << ", \"value\": " << value
        << ", \"seq\":" << seq
        << "}";

    std::string response;
    response = ss.str();
    return send(client_socket, response.c_str(), response.size(), 0);
}

int handle_set(int client_socket, json& command){
    // Example GET handling
    std::string response = "{\"status\": \"success\", \"value\": 123}";
    return send(client_socket, response.c_str(), response.size(), 0);
}

int handle_def(int client_socket, json& command){
    // Example GET handling
    std::string response = "{\"status\": \"success\", \"value\": 123}";
    return send(client_socket, response.c_str(), response.size(), 0);
}


// Handle JSON connection
void handle_json_connection(int client_socket) {
    char buffer[1024] = {};
    while (true) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cerr << "JSON connection closed or error occurred\n";
            close(client_socket);
            break;
        }

        buffer[bytes_received] = '\0';
        std::cout << "JSON command received: " << buffer << std::endl;

        try {
            json command = json::parse(buffer);

            if (command.contains("action") || command.contains("help")) {
                std::string action = command["action"];
                if (action == "help") {
                    std::string response = "{\"action\": \"set\", \"name\":\"Rack_Fault\",\"value\": 123}\n";
                        response += "{\"action\": \"get\", \"name\":\"Rack_Fault\"}\n";
                        response += "{\"action\": \"def\", \"name\": \"Pcs_Fault\", \"type\": \"RACK_DATA_TYPE_BITFIELD16\", \"num\": 1, \"start_bit\": 10, \"bit_size\": 1 }\n";
                        response += "{\"action\": \"show\"\n";;
                    send(client_socket, response.c_str(), response.size(), 0);

                } else if (action == "get") {
                    handle_get(client_socket, command); 
                    // Example GET handling
                    //std::string response = "{\"status\": \"success\", \"value\": 123}";
                    //send(client_socket, response.c_str(), response.size(), 0);
                } else if (action == "set") {
                    handle_set(client_socket, command); 
                    // // Example SET handling
                    // std::string response = "{\"status\": \"success\"}";
                    // send(client_socket, response.c_str(), response.size(), 0);
                } else if (action == "def") {
                    // Example DEF handling
                    std::string response = "{\"status\": \"definition added\"}";
                    send(client_socket, response.c_str(), response.size(), 0);
                } else {
                    std::string response = "{\"error\": \"unknown action\"}";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            } else {
                std::string response = "{\"error\": \"invalid command format\"}";
                send(client_socket, response.c_str(), response.size(), 0);

            }
        } catch (const std::exception& e) {
            std::string error_response = std::string("{\"error\": \"") + e.what() + "\"}";
            send(client_socket, error_response.c_str(), error_response.size(), 0);
                std::string response = "\n{\"action\": \"set\", \"name\":\"Rack_Fault\",\"value\": 123}\n";
                        response += "{\"action\": \"get\", \"name\":\"Rack_Fault\"}\n";
                        response += "{\"action\": \"def\", \"name\": \"Pcs_Fault\", \"type\": \"RACK_DATA_TYPE_BITFIELD16\", \"num\": 1, \"start_bit\": 10, \"bit_size\": 1 }\n";
                        response += "{\"action\": \"show\"\n";;
                    send(client_socket, response.c_str(), response.size(), 0);
        }
    }
}


// working on the def look at defs.cpp
/*
  [ 
    {"sm_name":"rack", "typedef":"Faults",  "name": "Pcs_Fault",            "type": "bitfield16", "bit_size": 1 },
    {"sm_name":"rack", "typedef":"Faults",  "name": "ChargeCurrent",        "type": "uint32" [, "num": 8] ["offset":34]},  
    {"sm_name":"rack", "typedef":"Summary", "name": "faults",               "type": "Faults" [, "num": 20]},
    {"sm_name":"rack", "typedef":"Config",  "name": "MaxChargeCurrent",     "type": "uint32" [, "num": 20]}, 
    {"sm_name":"rack", "typedef":"Config",  "name": "MaxDischargeCurrent",  "type": "uint32" [, "num": 20]}
  ]
*/
 

void run_as_client(char* ip, int port, int json_port)
{
    printf("under costruction");
}

/*
To handle these three options in your program, you can use `argc` and `argv` to process command-line arguments. Here's how you can structure your program:

### Code Implementation

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <cstring> // For strcmp
#include <stdexcept>

// Function prototypes
void run_as_test();
void run_as_server(int port, int json_port);
void run_as_client();

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            // No arguments: Run as test
            run_as_test();
        } else if (argc == 3 && strcmp(argv[1], "-t") == 0 && strcmp(argv[2], "server") == 0) {
            // Server mode
            int port = 8080;      // Default data port
            int json_port = 8081; // Default JSON command port

            std::cout << "Enter server data port (default 8080): ";
            std::cin >> port;

            std::cout << "Enter server JSON command port (default 8081): ";
            std::cin >> json_port;

            run_as_server(port, json_port);
        } else if (argc == 3 && strcmp(argv[1], "-t") == 0 && strcmp(argv[2], "client") == 0) {
            // Client mode
            run_as_client();
        } else {
            // Invalid arguments
            std::cerr << "Usage:\n";
            std::cerr << "  " << argv[0] << "          Run as test\n";
            std::cerr << "  " << argv[0] << " -t server    Run as server\n";
            std::cerr << "  " << argv[0] << " -t client    Run as client\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

// Run in test mode
void run_as_test() {
    std::cout << "Running in test mode...\n";

    // Add your test logic here
    std::vector<int> test_data = {1, 2, 3, 4, 5};
    for (int val : test_data) {
        std::cout << "Processing: " << val << std::endl;
    }

    std::cout << "Test completed successfully!\n";
}

// Run as server
void run_as_server(int port, int json_port) {
    std::cout << "Starting server on data port: " << port 
              << " and JSON command port: " << json_port << std::endl;

    // Placeholder for server logic
    // Add your server networking code to handle data and JSON commands
    std::cout << "Server running... Waiting for connections...\n";

    // Example: Listen for JSON commands on `json_port` and process get, set, def
    // Logic should be implemented here to process JSON input
}

// Run as client
void run_as_client() {
    std::cout << "Running in client mode...\n";

    // Placeholder for client logic
    // Add your client networking code to communicate with the server
    std::cout << "Connecting to server...\n";
}
```

### Explanation of the Code

1. **No Arguments (`run_as_test`)**:
   - When no arguments are provided, the program runs in test mode.
   - You can add your own test logic to verify functionality.

2. **Server Mode (`run_as_server`)**:
   - The server listens on two ports:
     - One for data communication.
     - Another for receiving JSON commands (e.g., `get`, `set`, and `def`).
   - You can use libraries like `std::net` or a networking library (e.g., `asio`, `libuv`) to implement the server.

3. **Client Mode (`run_as_client`)**:
   - The client connects to the server and communicates via the specified protocol.
   - You can add logic for sending commands to the server and processing responses.

### JSON Command Handling

In server mode, you can add functions to parse and process JSON commands using a JSON library like `nlohmann/json`:

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

void process_json_command(const std::string& command) {
    try {
        json cmd = json::parse(command);

        if (cmd.contains("action")) {
            std::string action = cmd["action"];
            if (action == "get") {
                // Handle get command
                std::cout << "Processing GET command...\n";
            } else if (action == "set") {
                // Handle set command
                std::cout << "Processing SET command...\n";
            } else if (action == "def") {
                // Handle def command
                std::cout << "Processing DEF command...\n";
            } else {
                throw std::runtime_error("Unknown action: " + action);
            }
        } else {
            throw std::runtime_error("Invalid JSON command");
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON Processing Error: " << e.what() << std::endl;
    }
}
```

### Example Commands

- **Get Command**:
  ```json
  { "action": "get", "key": "Val1" }
  ```

- **Set Command**:
  ```json
  { "action": "set", "key": "Val1", "value": 123 }
  ```

- **Define Command**:
  ```json
  { "action": "def", "key": "NewVal", "type": "uint16", "num_uint16": 2 }
  ```

### Testing

- **Test**: Run the program without arguments to execute your test logic.
- **Server**: Run with `-t server`, provide ports, and start the server.
- **Client**: Run with `-t client` to interact with the server.
*/