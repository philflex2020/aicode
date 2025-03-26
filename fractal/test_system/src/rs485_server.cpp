#include <iostream>
#include <modbus.h>
#include <errno.h>
#include <vector>
#include <cstdint>
#include <map>

// concept is that we deinf a name that gives us a connection a data_server and a client or a server

// Assuming hex_dump function and other relevant functions are already defined elsewhere in your code.
//void hex_dump(const char *desc, const uint8_t *data, int len);

// Function to print the data in hexadecimal format
void hex_dump(const char *desc, const uint8_t *data, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)data;

    // Output description if given.
    if (desc != NULL)
        printf("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf("  %s\n", buff);

            // Output the offset.
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf("  %s\n", buff);
}

enum class RegisterType {
    HOLDING,
    INPUT,
    COIL,
    DISCRETE_INPUT
};

//this is just  a link to a name ( we'll hold a value as well for now)
struct DataItem {
    uint16_t value = 0; 
    uint32_t id;
    std::string name;
};

using DataMap = std::map<int, DataItem>;

struct ModbusData {
    DataMap holdingRegisters;
    DataMap inputRegisters;
    DataMap coils;
    DataMap discreteInputs;
};

// Assuming modbusData structure and set_register_value/get_register_value functions are defined.
ModbusData modbusData;

uint16_t get_register_value(std::map<int, DataItem>& register_map, int address) {
    // Check if the address exists in the map
    auto it = register_map.find(address);
    if (it != register_map.end()) {
        return it->second.value;  // Return the existing value
    } else {
        // If not present, initialize with a default value
        DataItem item;
        register_map[address] = item;  // Initialize with default value, e.g., 0
        register_map[address].value = 0;  // Initialize with default value, e.g., 0
        return 0;  // Return the newly set default value
    }
}

void set_register_value(std::map<int, DataItem>& register_map, int address, uint16_t value) {
    register_map[address].value = value;  // Set or overwrite the value at the address
}


std::vector<uint8_t> pack_bits(const std::vector<uint8_t>& bits) {
    size_t byte_count = (bits.size() + 7) / 8;
    std::vector<uint8_t> packed(byte_count, 0);
    for (size_t i = 0; i < bits.size(); ++i) {
        if (bits[i]) {
            packed[i / 8] |= (1 << (i % 8));
        }
    }
    return packed;
}

std::vector<uint8_t> unpack_bits(const uint8_t* data, size_t bit_count) {
    std::vector<uint8_t> bits(bit_count);
    for (size_t i = 0; i < bit_count; ++i) {
        bits[i] = (data[i / 8] >> (i % 8)) & 0x01;
    }
    return bits;
}

void print_bits(const std::vector<uint8_t>& bits) {
    for (size_t i = 0; i < bits.size(); ++i) {
        std::cout << static_cast<int>(bits[i]);
        if (i % 8 == 7) std::cout << ' ';
    }
    std::cout << "\n";
}


void print_query_details(modbus_t *ctx, uint8_t *query, int rc) {
    int length = modbus_get_header_length(ctx);
    if (rc < 5) {  // Minimum length for a function with data
        printf("Query too short (%d) to process\n", length);
       //return;
    }

    int slave_id = query[0];
    int function_code = query[1];
    int address = (query[2] << 8) + query[3];

    // For typical read and write functions
    int quantity = (query[4] << 8) + query[5];  // This applies for read coils, read holding registers, etc.

    printf("Received function %d from slave %d at address %d for %d registers/values\n",
           function_code, slave_id, address, quantity);
}

// run on a server
void process_modbus_request(modbus_t* ctx, uint8_t* query, int rc, modbus_mapping_t* mb_mapping) {
    print_query_details(ctx, query, rc);
    int header_length = modbus_get_header_length(ctx);
    int slave_id = query[0];
    uint8_t function_code = query[1];
    uint16_t address = (query[2] << 8) + query[3];
    uint16_t num = (query[4] << 8) + query[5];
    printf("Received function %d from slave %d at address %d for %d registers/values\n",
           function_code, slave_id, address, num);

    DataMap* target_map = nullptr;
    switch (function_code) {
        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            target_map = &modbusData.holdingRegisters;
            break;
        case MODBUS_FC_READ_INPUT_REGISTERS:
            target_map = &modbusData.inputRegisters;
            break;
        case MODBUS_FC_READ_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_SINGLE_COIL:
            target_map = &modbusData.coils;
            break;
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            target_map = &modbusData.discreteInputs;
            break;
        default:
            std::cerr << "Unsupported function code received: " << static_cast<int>(function_code) << std::endl;
            return;
    }

    printf(" still running #1 \n");
    if (
        function_code == MODBUS_FC_READ_COILS ||
        function_code == MODBUS_FC_READ_DISCRETE_INPUTS) {
            std::vector<uint8_t> bit_values;
            for (int i = 0; i < num; ++i) {
                uint16_t v = get_register_value(*target_map, address + i);
                bit_values.push_back(v & 0x01);  // Only LSB used
            }
            
            auto packed = pack_bits(bit_values);
            
            std::vector<uint8_t> response;
            response.push_back(query[0]);           // Slave ID
            response.push_back(function_code);      // Function code
            response.push_back(packed.size());      // Byte count
            response.insert(response.end(), packed.begin(), packed.end());
            
            modbus_reply(ctx, response.data(), response.size(), nullptr);
            return;
        
    } else if (
        function_code == MODBUS_FC_READ_HOLDING_REGISTERS ||
        function_code == MODBUS_FC_READ_INPUT_REGISTERS) {
            // Handling read request
            std::vector<uint8_t> response(3 + 2 * num); // Slave ID, Function Code, Byte Count + Data
            response[0] = query[0];  // Slave ID
            response[1] = query[1];  // Function Code
            response[2] = num * 2;   // Byte Count (number of bytes to follow)
            printf(" still running #2\n");

            for (int i = 0; i < num; ++i) {
                printf(" still running #3 i %d\n", i);
                uint16_t value = get_register_value(*target_map, address + i);
                response[3 + 2 * i] = value >> 8;
                response[4 + 2 * i] = value & 0xFF;
            }

            printf(" still running #4\n");
            modbus_reply(ctx, query, rc, mb_mapping);
            //modbus_reply(ctx, response.data(), 3 + 2 * num, mb_mapping);
            return;
            
    } else if ( function_code == MODBUS_FC_WRITE_MULTIPLE_COILS) {
            int byte_count = query[6];
            const uint8_t* data_start = &query[7];
            auto bits = unpack_bits(data_start, num);
        
            for (int i = 0; i < num; ++i) {
                set_register_value(*target_map, address + i, bits[i] ? 1 : 0);
            }
        
            // Respond with echo of address + quantity
            uint8_t response[6] = {
                query[0], query[1],
                static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address & 0xFF),
                static_cast<uint8_t>(num >> 8), static_cast<uint8_t>(num & 0xFF)
            };
            modbus_reply(ctx, response, 6, nullptr);
            return;
    } else if (function_code == MODBUS_FC_WRITE_SINGLE_COIL) {
        uint16_t value = (query[4] << 8) + query[5];
        set_register_value(*target_map, address, (value == 0xFF00) ? 1 : 0);
        modbus_reply(ctx, query, rc, nullptr);  // Echo full request
        return;
    } else if (function_code == MODBUS_FC_WRITE_MULTIPLE_REGISTERS ||
               function_code == MODBUS_FC_WRITE_SINGLE_REGISTER ){
        // Handling write request

        if (function_code == MODBUS_FC_WRITE_SINGLE_REGISTER ){
            printf(" still running #2a value %d\n", num);
            set_register_value(*target_map, address, num);
        } else {

            for (int i = 0; i < num; ++i) {
                uint16_t value = (query[7 + 2 * i] << 8) + query[8 + 2 * i];
                printf(" still running #3a i %d value %d\n", i , value);
                //set_register_value(*target_map, address + i, value);
            }
        }
        modbus_reply(ctx, query, rc, mb_mapping);  // Echo the request as the response
    }
}

int main(int argc , char * argv[]) {
    std::string server("/dev/ttyUSB5");
    if(argc>1)
        server = std::string(argv[1]);
    modbus_t* ctx = modbus_new_rtu(server.c_str(), 9600, 'N', 8, 1);
    modbus_mapping_t* mb_mapping = modbus_mapping_new(256, 256, 256, 256);
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }
//    int fd = modbus_get_socket(ctx);
//    std::cout << " server ["<<server <<"] using fd  " << fd << std::endl;
// Set the Modbus address of this server
    modbus_set_slave(ctx, 1);

    mb_mapping->tab_registers[0] = 12345;
    uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

    while (true) {
        //std::cout <<" mb server  waiting for data "<< std::endl;
        int rc = modbus_receive(ctx, query);
        int err = errno;
        std::cout <<" mb server  got rc  ["<<(int) rc << "]"<< std::endl;

        if (rc > 0) {
            hex_dump("Received Data", query, rc);
            //modbus_reply(ctx, query, rc, mb_mapping);

            process_modbus_request(ctx, query, rc, mb_mapping);
        } else if (rc == -1) {
            fprintf(stderr, "Connection failed: %s\n", modbus_strerror(err));

            break;  // Error or client disconnected
        }
    }

    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}
