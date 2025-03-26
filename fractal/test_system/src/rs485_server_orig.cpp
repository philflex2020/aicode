#include <stdio.h>
#include <modbus.h>
#include <cerrno>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>

// rs485_server
//gcc -o rs485_server rs485_server.c -lmodbus
//gcc -o rs485_client rs485_client.c -lmodbus

#include <stdio.h>
#include <modbus.h>


enum class RegisterType {
    HOLDING,
    INPUT,
    COIL,
    DISCRETE_INPUT
};

struct DataItem {
    uint16_t value = 0;  // Default to 0 for non-coil types
    std::string name;
};

using DataMap = std::map<int, DataItem>;

struct ModbusData {
    DataMap holdingRegisters;
    DataMap inputRegisters;
    DataMap coils;
    DataMap discreteInputs;
};

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

// This function constructs the basic response for Modbus RTU
// Assumes that the query is correctly formatted and the response buffer is large enough
int build_response_basis(modbus_t *ctx, const uint8_t* query, int query_length, uint8_t* response) {
    // Minimum length of query should be the function code + address (2 bytes) + data/quantity (2 bytes)
    if (query_length < 5) return 0;

    // Copying the address and function code
    response[0] = query[0];  // Slave address
    response[1] = query[1];  // Function code

    // Copy the function code and the starting address as they appear in the query
    for (int i = 2; i < 6; i++) {
        response[i] = query[i];
    }

    // The length up to now should be the initial bytes + the CRC (which will be added later)
    return 6;  // Return the length of the response up to this point
}

// Function to print the data in hexadecimal format
void HexDump(const char *desc, const uint8_t *data, int len) {
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

void process_modbus_request(modbus_t* ctx, uint8_t* query, int query_length) {
    int header_length = modbus_get_header_length(ctx);
    uint8_t function_code = query[header_length - 1];
    uint16_t address = (query[header_length] << 8) + query[header_length + 1];
    uint16_t num = (query[header_length + 2] << 8) + query[header_length + 3];

    RegisterType type;
    DataMap* target_map = nullptr;

    // Determine the type of register and the corresponding map
    switch (function_code) {
        case MODBUS_FC_READ_HOLDING_REGISTERS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            type = RegisterType::HOLDING;
            target_map = &modbusData.holdingRegisters;
            break;
        case MODBUS_FC_READ_INPUT_REGISTERS:
            type = RegisterType::INPUT;
            target_map = &modbusData.inputRegisters;
            break;
        case MODBUS_FC_READ_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_SINGLE_COIL:
            type = RegisterType::COIL;
            target_map = &modbusData.coils;
            break;
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            type = RegisterType::DISCRETE_INPUT;
            target_map = &modbusData.discreteInputs;
            break;
        default:
            // Unsupported function code
            return;
    }

        // Perform write operations if applicable
    if (function_code == MODBUS_FC_WRITE_SINGLE_REGISTER || function_code == MODBUS_FC_WRITE_MULTIPLE_REGISTERS ||
        function_code == MODBUS_FC_WRITE_SINGLE_COIL || function_code == MODBUS_FC_WRITE_MULTIPLE_COILS) {
        // Assuming values start at byte 7 following function, address, and count
        for (int i = 0; i < num; ++i) {
            uint16_t value = (query[header_length + 5 + 2 * i] << 8) + query[header_length + 6 + 2 * i];
            set_register_value(*target_map, address + i, value);
        }
    }

    // Prepare a response (assuming read for simplicity in example)
    std::vector<uint8_t> response_buffer(256);


    // Append register values or statuses to response
    if (function_code == MODBUS_FC_READ_HOLDING_REGISTERS ||
        function_code == MODBUS_FC_READ_INPUT_REGISTERS ||
        function_code == MODBUS_FC_READ_COILS ||
        function_code == MODBUS_FC_READ_DISCRETE_INPUTS) {
        std::vector<uint16_t> response_values;
        for (int i = 0; i < num; ++i) {
            uint16_t value = get_register_value(*target_map, address + i);
            response_values.push_back(value);
        }        
    }
    int response_length = build_response_basis(ctx, query, query_length, response_buffer.data());
    int index = response_length;

    
    // Send response
    modbus_reply(ctx, response_buffer.data(), index, nullptr);
}

// start_server from a string  "/dev/ttyUSB5:115200:N:8:1" 
//int start_server(rsserver, ctx, data_items) {

// rsserver 
// modbus_t * set_up_rtu(const std::string& rsserver, int device_id);
 // start_server(const std::string& rsserver , modbus_t *ctx  std::map<std::string,dataItem>data_items) 


modbus_t* set_up_rtu(const std::string& config, int device_id) {
    std::istringstream iss(config);
    std::string port;
    int baud;
    char parity;
    int data_bit, stop_bit;

    std::getline(iss, port, ':');
    iss >> baud >> parity >> data_bit >> stop_bit;

    modbus_t* ctx = modbus_new_rtu(port.c_str(), baud, parity, data_bit, stop_bit);
    if (!ctx) {
        std::cerr << "Unable to create the libmodbus context" << std::endl;
        return nullptr;
    }

    modbus_set_slave(ctx, device_id);
    return ctx;
}


int start_server(const std::string& rsserver) {
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;

    // Create a new RTU context with the specified serial parameters
    //ctx = modbus_new_rtu("/dev/ttyUSB5", 9600, 'N', 8, 1);
    ctx = modbus_new_rtu(rsserver.c_str(), 9600, 'N', 8, 1);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create the libmodbus context\n");
        return -1;
    }

    // Set the Modbus address of this server
    modbus_set_slave(ctx, 1);

    // Allocate memory for the Modbus mapping
    mb_mapping = modbus_mapping_new(256, 256, 256, 256);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate mapping: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    // Set up a sample value: write 12345 to the first holding register
    mb_mapping->tab_registers[0] = 12345;

    // Start listening for incoming queries
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        modbus_mapping_free(mb_mapping);
        return -1;
    }
    int fd = modbus_get_socket(ctx);
    std::cout << " server ["<<rsserver <<"] using fd  " << fd << std::endl;
    for (;;) {
        uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];
        // {        
        //     uint8_t buffer[256];  // Buffer to hold incoming data
        //     ssize_t bytes_read;

        //     bytes_read = read(fd, buffer, sizeof(buffer));
        //     if (bytes_read == -1) {
        //         std::cout << " error with  fd " << fd << std::endl;
        //         perror("read");
        //         // handle error
        //         return 0;
        //     } else if (bytes_read == 0) {
        //         printf("Connection closed\n");
        //         modbus_close(ctx);
        //         if (modbus_connect(ctx) == -1) {
        //             printf(" Unable to connect to ctx \n");
        //             return 0;
        //         }
        //         fd = modbus_get_socket(ctx);
        //         std::cout << " Got a new modbus fd " << fd << std::endl;
        //         // Set the Modbus address of this server
        //         modbus_set_slave(ctx, 1);
        //         // handle disconnection
        //     } else {
        //         printf("Read %zd bytes from the serial port\n", bytes_read);
        //     // Process the bytes read as needed
        //     HexDump("Received Data", buffer, bytes_read);  // Assuming hex_dump is defined elsewhere
        //     }
        // }
        int rc = modbus_receive(ctx, query);
        std::cout <<" mb server  got rc  ["<<(int) rc << "]"<< std::endl;
        if (rc > 0) {
            HexDump("Received Data", query, rc);  // Call to hex dump function
            print_query_details(ctx, query, rc);
            modbus_reply(ctx, query, rc, mb_mapping);
        } else if (rc == -1) {
            // Connection closed or error
            break;
        }
    }

    printf("Quit the loop: %s\n", modbus_strerror(errno)); 

    // Clean up
    modbus_mapping_free(mb_mapping);
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}

int main(int argc, char * argv[])
{
    std::string server("/dev/ttyUSB5");
    if(argc>1)
        server = std::string(argv[1]);
    start_server(server);
    return 0;
}