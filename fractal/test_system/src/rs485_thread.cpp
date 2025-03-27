/**
 ******************************************************************************
 * @Copyright (C), 2025, Fractal EMS Inc. 
 * @file: rs485_thread.cpp
 * @author: Phil Wilshire
 * @Description:
 *     Modbus 485 thread Functions
 * @Others: None
 * @History: 1. Created by Phil Wilshire.
 * @version: V1.0.0
 * @date:    2025.03.27
 ******************************************************************************
 */
//  g++ -o rs485_thread -I /usr/include/modbus src/rs485_thread.cpp -lmodbus -lpthread
#include <iostream>
#include <modbus.h>
#include <errno.h>
#include <poll.h>
#include <vector>
#include <cstdint>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

// threaded server and client 
//
volatile bool running = true;
enum mode {
    NONE = 0,
    RS485_SERVER,
    RS485_CLIENT

};

// Callback function types
typedef uint8_t (*WriteCallback)(uint8_t fcode, uint16_t regAddr, uint16_t value, uint16_t *p_out);
typedef uint8_t (*ReadCallback)(uint8_t fcode, uint16_t regAddr, uint16_t *p_value);

typedef struct {
    const char *port_name;    // Serial port
    int baud;
    char parity;
    int data_bits;
    int stop_bits;
    int mode;   // client / server
    int fd;
    volatile int rs485_running;
    int server_id;
    WriteCallback write_callback;  // Pointer to write callback function
    ReadCallback read_callback;    // Pointer to read callback function
    std::string rdev;
    int btime;  // byte timeout
    int ftime;  // repeat time
    int port;   // if defined switches on relay operation
    int prot;   // protocol (0 == none) used in client mode 
    
//     uint8_t (*write_callback)(uint8_t fcode, uint16_t regAddr, uint16_t value, uint16_t *p_out);
//     uint8_t (*read_callback)(uint8_t fcode, uint16_t regAddr, uint16_t *p_value);
} ModbusRTUSettings;

// for modbus rs485client
struct ModbusMessage {
    int offset;
    int fc;
    std::vector<uint16_t> data;
};

class MessageQueue {
private:
    std::queue<ModbusMessage> queue;
    mutable std::mutex mutex;
    std::condition_variable cond;

public:
    void push(const ModbusMessage& message) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(message);
        cond.notify_one();
    }

    ModbusMessage pop() {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [this]{ return !queue.empty(); });
        auto message = queue.front();
        queue.pop();
        return message;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty();
    }
};


int rs485_baud_decode(int rs485_baud);
uint16_t rs485_baud_encode(int settings_baud);

uint16_t rs485_baud_encode(int settings_baud)
{
	// 5=4800, 6=9600, 7=14400 , 8=19200, 9=38400, 10=57600, 11=115200, 12=230400 13=460800, 14=921600 
	switch(settings_baud){

	case 4800:
		return 5;
		break;

	case 9600:
		return 6;
		break;

	case 14400:
		return 7;
		break;

	case  19200:
		return 8;
		break;

	case 34800:
		return 9;
		break;

	case 57600:
		return 10;
		break;

	case 115200:
		return 11;
		break;

	case 30400:
		return 12;
		break;

	case 460800:
		return 13;
		break;

	case 921600:
		return 14;
		break;

	default:
		return 0;
	}
	return 0;
} 


int rs485_baud_decode(int rs485_baud)
{
	int baud = 0;
	// 5=4800, 6=9600, 7=14400 , 8=19200, 9=38400, 10=57600, 11=115200, 12=230400 13=460800, 14=921600 
	switch(rs485_baud){
	case 5:
		baud = 4800;
		break;
	case 6:
		baud = 9600;
		break;
	case 7:
		baud = 14400;
		break;
	case 8:
		baud = 19200;
		break;
	case 9:
		baud = 34800;
		break;
	case 10:
		baud = 57600;
		break;
	case 11:
		baud = 115200;
		break;
	case 12:
		baud = 230400;
		break;
	case 13:
		baud = 460800;
		break;
	case 14:
		baud = 921600;
		break;
	default:
		baud = 0;
	}
	return baud;
} 

modbus_t *create_rtu_ctx(ModbusRTUSettings *settings)
{
    modbus_t *ctx = modbus_new_rtu(settings->port_name, settings->baud,
                                   settings->parity, settings->data_bits, settings->stop_bits);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create the libmodbus context\n");
        return NULL;
    }
    settings->fd = modbus_get_socket(ctx); 
    // Set byte timeout to 0.5 seconds
    uint32_t to_sec = 0;        // 0 seconds
    uint32_t to_usec = 500000;  // 500,000 microseconds

    modbus_set_byte_timeout(ctx, to_sec, to_usec);

    return ctx;
}

// Handle a Modbus query based on its function code
void handle_modbus_query(ModbusRTUSettings *settings, modbus_t *ctx, const uint8_t *query, int query_length) {
    uint8_t function_code = query[1];
    uint16_t reg_addr = (query[2] << 8) | query[3];
    bool reply_ok = true;

    switch (function_code) {
        case 0x06: { // Write single register
            uint16_t value = (query[4] << 8) | query[5];
            uint16_t out;
            if (settings->write_callback && settings->write_callback(function_code, reg_addr, value, &out)) {
                modbus_reply(ctx, query, query_length, NULL);
            } else {
                reply_ok = false;
            }
            break;
        }
        case 0x03: { // Read registers
            uint16_t value;
            if (settings->read_callback && settings->read_callback(function_code, reg_addr, &value)) {
                modbus_reply(ctx, query, query_length, NULL);
            } else {
                reply_ok = false;
            }
            break;
        }
        default:
            reply_ok = false;
            break;
    }

    if (!reply_ok) {
        modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
    }
}


void modbus_server_thread(MessageQueue& queue, void* arg) {
    ModbusRTUSettings *settings = (ModbusRTUSettings *)arg;
    bool reply_ok = true;

    while (running && settings->rs485_running) {


        modbus_t *ctx = create_rtu_ctx(settings);

        // modbus_t *ctx = modbus_new_rtu(settings->port_name, settings->baud_rate,
        //                                settings->parity, settings->data_bits, settings->stop_bits);
        if (ctx == NULL) {
            fprintf(stderr, "Unable to create the libmodbus context\n");
            return;
        }
        settings->fd = modbus_get_socket(ctx); 

        // // Set byte timeout to 0.5 seconds
        // uint32_t to_sec = 0;        // 0 seconds
        // uint32_t to_usec = 500000;  // 500,000 microseconds

        // modbus_set_byte_timeout(ctx, to_sec, to_usec);

        modbus_set_slave(ctx, settings->server_id);  // Set the Modbus server ID to 1
        if (modbus_connect(ctx) == -1) {
            fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
            modbus_free(ctx);
            return;
        }

        // Set up pollfd structure
        struct pollfd fds[1];

        fds[0].fd = settings->fd ;
        fds[0].events = POLLIN;
        // Server loop
        printf("RS485 server is running...\n");
        while (running && settings->rs485_running) {
            int rc = poll(fds, 1, 1000); // 1000 milliseconds timeout
            if (rc == -1) {
                perror("Poll failed");
                break;
            } else if (rc > 0) {
                if (fds[0].revents & POLLIN) {
                    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
                    rc = modbus_receive(ctx, query);
                    if (rc > 0) {
                        // Handle the query based on the Modbus function code
                        handle_modbus_query(settings, ctx, query, rc);
                    } else if (rc == -1) {
                        if (errno == ETIMEDOUT) {
                            fprintf(stderr, "A byte timeout occurred.\n");
                            // Handle the timeout, such as attempting to read again or logging the timeout event
                        } else {
                            fprintf(stderr, "Error receiving data: %s\n", modbus_strerror(errno));
                            // Handle other types of errors
                            break;
                        }
                    }
                }
            }
            // Check flags periodically
        }

        modbus_close(ctx);
        modbus_free(ctx);
    }
    return;
}

void modbus_client_thread(MessageQueue& queue, void* arg) {
    ModbusRTUSettings *settings = (ModbusRTUSettings *)arg;
    while (running && settings->rs485_running) {

        bool reply_ok = true;
        modbus_t *ctx = create_rtu_ctx(settings);
        if (!ctx) return;
        // add timeout to check for flags
        while (running && settings->rs485_running) {
            if (!queue.empty()) {
                int res = 0;
                ModbusMessage message = queue.pop();
                switch (message.fc) {
                    case MODBUS_FC_WRITE_SINGLE_REGISTER:
                        if(message.data.size() > 1)
                            message.fc = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
                        break;
                    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                        if(message.data.size() == 1)
                            message.fc = MODBUS_FC_WRITE_SINGLE_REGISTER;
                        break;
                    case MODBUS_FC_WRITE_SINGLE_COIL:
                        if(message.data.size() > 1)
                            message.fc = MODBUS_FC_WRITE_MULTIPLE_COILS;
                        break;
                    case MODBUS_FC_WRITE_MULTIPLE_COILS:
                        if(message.data.size() == 1)
                            message.fc = MODBUS_FC_WRITE_SINGLE_COIL;
                        break;
                    default:
                        fprintf(stderr, "Unsupported function code: %d\n", message.fc);

                    }
                // now use the adjusted fc
                switch (message.fc) {
                    case MODBUS_FC_WRITE_SINGLE_REGISTER:
                        res = modbus_write_register(ctx, message.offset, message.data[0]);
                        break;
                    case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                        res = modbus_write_registers(ctx, message.offset, message.data.size(), message.data.data());
                        break;
                    case MODBUS_FC_WRITE_SINGLE_COIL:
                        res = modbus_write_bit(ctx, message.offset, message.data[0] ? TRUE : FALSE);
                        break;
                    case MODBUS_FC_WRITE_MULTIPLE_COILS:
                        {
                            // Transform vector<uint16_t> to vector<uint8_t> required by libmodbus
                            std::vector<uint8_t> bits(message.data.size());
                            for (size_t i = 0; i < message.data.size(); i++) {
                                bits[i] = message.data[i] ? TRUE : FALSE;
                            }
                            res = modbus_write_bits(ctx, message.offset, bits.size(), bits.data());
                        }
                        break;
                    default:
                        fprintf(stderr, "Unsupported function code: %d\n", message.fc);
                }

                if (res == -1) {
                    fprintf(stderr, "Modbus operation failed: %s\n", modbus_strerror(errno));
                    if (errno == EBADF || errno == EMBXILADD) {
                        break; // Critical failure, exit loop
                    }
                }
            } else {
                // Sleep for a short time to prevent a busy-wait loop
                usleep(10000); // Sleep 10ms
            }
        }

        modbus_close(ctx);
        modbus_free(ctx);
    }
}


void enqueue_message(MessageQueue& queue, int fc, int offset, const std::vector<uint16_t>& data) {
    ModbusMessage message{fc, offset, data};
    queue.push(message);
}

void enqueue_message(MessageQueue& queue, int fc, int offset, uint16_t* data, int len) {
    // Construct a vector from the provided pointer and length
    std::vector<uint16_t> data_vector(data, data + len);
    enqueue_message(queue, fc, offset, data_vector);
}

void addWriteCallback(ModbusRTUSettings *settings, WriteCallback callback) {
    settings->write_callback = callback;
}

// Function to set the read callback
void addReadCallback(ModbusRTUSettings *settings, ReadCallback callback) {
    settings->read_callback = callback;
}

// Example callback functions
uint8_t ExampleWriteCallback(uint8_t fcode, uint16_t regAddr, uint16_t value, uint16_t *p_out) {
    printf("Write callback called with value: %d\n", value);
    *p_out = value;  // Echo back the value written
    return 1;        // Success
}

uint8_t ExampleReadCallback(uint8_t fcode, uint16_t regAddr, uint16_t *p_value) {
    printf("Read callback called\n");
    *p_value = 1234; // Provide a dummy value
    return 1;        // Success
}

ModbusRTUSettings server_settings;
ModbusRTUSettings client_settings;

int rs485_test(const std:: string & server, const std::string& client) {

    client_settings.port_name = client.c_str();
    client_settings.baud = 19200;
    client_settings.parity = 'N';
    client_settings.data_bits = 8;
    client_settings.stop_bits = 1;
    client_settings.mode = RS485_CLIENT;
    client_settings.fd = -1;
    client_settings.rs485_running = 1;
    client_settings.server_id = 1;
    client_settings.write_callback = nullptr;
    client_settings.read_callback = nullptr;

    server_settings.port_name = server.c_str();
    server_settings.baud = 19200;
    server_settings.parity = 'N';
    server_settings.data_bits = 8;
    server_settings.stop_bits = 1;
    server_settings.mode = RS485_SERVER;
    server_settings.fd = -1;
    server_settings.rs485_running = 1;
    server_settings.server_id = 1;
    server_settings.write_callback = nullptr;
    server_settings.read_callback = nullptr;

     // Set the callbacks dynamically
    addWriteCallback(&server_settings, ExampleWriteCallback);
    addReadCallback(&server_settings, ExampleReadCallback);

    MessageQueue queue;

    std::thread server_thread(modbus_server_thread, std::ref(queue), (void *)&server_settings);
    // if (pthread_create(&server_thread, NULL, modbus_rs485_server, &settings) != 0) {
    //     fprintf(stderr, "Failed to create RS485 server thread\n");
    //     return -1;
    // }

    std::thread client_thread(modbus_client_thread, std::ref(queue), (void *)&client_settings);

    // Example of enqueueing a message
    std::vector<uint16_t> data = {0x1234, 0x5678};
    enqueue_message(queue, 3, 10, data);

    // Join the thread when appropriate
    if(client_thread.joinable())client_thread.join();
    if(server_thread.joinable())server_thread.join();
    return 0;
}

// concept is that we define a name that gives us a connection a data_server and a client or a server

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
    //print_query_details(ctx, query, rc);
    int header_length = modbus_get_header_length(ctx);
    int slave_id = query[0];
    uint8_t function_code = query[1];
    uint16_t address = (query[2] << 8) + query[3];
    uint16_t num = (query[4] << 8) + query[5];
    printf("Received function %d from slave %d at address %d for %d registers/values\n",
           function_code, slave_id, address, num);

    DataMap* target_map = nullptr;

    if (
        function_code == MODBUS_FC_READ_COILS ||
        function_code == MODBUS_FC_READ_DISCRETE_INPUTS) {

        if (function_code == MODBUS_FC_READ_COILS) {
            target_map = &modbusData.coils;
            for (int i = 0; i < num; ++i) {
                uint16_t value = get_register_value(*target_map, address + i);
                mb_mapping->tab_bits[address - mb_mapping->start_bits + i] = value;
            }
        } else if (function_code == MODBUS_FC_READ_DISCRETE_INPUTS) {
            target_map = &modbusData.discreteInputs;
            for (int i = 0; i < num; ++i) {
                uint8_t value = get_register_value(*target_map, address + i); // Similar to get_coil_value but for inputs
                mb_mapping->tab_input_bits[address - mb_mapping->start_input_bits + i] = value;
            }
        }        
       
    } else if (
        function_code == MODBUS_FC_READ_INPUT_REGISTERS) {
            target_map = &modbusData.inputRegisters;
            for (int i = 0; i < num; ++i) {
                uint16_t value = get_register_value(*target_map, address + i);
                mb_mapping->tab_registers[ mb_mapping->start_registers + i] = value;
            }

    } else if ( function_code == MODBUS_FC_READ_HOLDING_REGISTERS) {
            target_map = &modbusData.holdingRegisters;
            for (int i = 0; i < num; ++i) {
                uint16_t value = get_register_value(*target_map, address + i);
                mb_mapping->tab_registers[ mb_mapping->start_registers + i] = value;
            }

    } else if ( function_code == MODBUS_FC_WRITE_MULTIPLE_COILS) {
        target_map = &modbusData.coils;
        int byte_count = query[6];
        const uint8_t* data_start = &query[7];
        auto bits = unpack_bits(data_start, num);
    
        for (int i = 0; i < num; ++i) {
            set_register_value(*target_map, address + i, bits[i] ? 1 : 0);
        }
    
    } else if (function_code == MODBUS_FC_WRITE_SINGLE_COIL) {
        target_map = &modbusData.coils;
        uint16_t value = (query[4] << 8) + query[5];

        set_register_value(*target_map, address, (value == 0xFF00) ? 1 : 0);

    } else if (function_code == MODBUS_FC_WRITE_MULTIPLE_REGISTERS ||
               function_code == MODBUS_FC_WRITE_SINGLE_REGISTER ){
        target_map = &modbusData.holdingRegisters;

        if (function_code == MODBUS_FC_WRITE_SINGLE_REGISTER ){
            printf(" still running #2a value %d\n", num);
            set_register_value(*target_map, address, num);
        } else {
            for (int i = 0; i < num; ++i) {
                uint16_t value = (query[7 + 2 * i] << 8) + query[8 + 2 * i];
                //printf(" still running #3a i %d value %d\n", i , value);
                set_register_value(*target_map, address + i, value);
            }
        }
    }
    modbus_reply(ctx, query, rc, mb_mapping);

}

int main(int argc , char * argv[]) {
    std::string server("/dev/ttyUSB5");
    std::string client("/dev/ttyUSB6");
    if(argc>1)
        server = std::string(argv[1]);
    if(argc>2)
        client = std::string(argv[2]);
    rs485_test(server, client);


    // while(true)
    // {
    //     printf(" Connecting to [%s] \n", server.c_str());
    //     modbus_t* ctx = modbus_new_rtu(server.c_str(), 9600, 'N', 8, 1);
    //     if (modbus_connect(ctx) == -1) {
    //         fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
    //         modbus_free(ctx);
    //         return -1;
    //     }
    //     printf(" Connect to [%s]  OK \n", server.c_str());
    //     modbus_mapping_t* mb_mapping = modbus_mapping_new(256, 256, 256, 256);
    //     printf(" Mapping  OK \n");

    // //    int fd = modbus_get_socket(ctx);
    // //    std::cout << " server ["<<server <<"] using fd  " << fd << std::endl;
    // // Set the Modbus address of this server
    //     modbus_set_slave(ctx, 1);

    //     mb_mapping->tab_registers[0] = 12345;
    //     uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

    //     while (true) {
    //         std::cout <<" mb server  waiting for data "<< std::endl;
    //         int rc = modbus_receive(ctx, query);
    //         int err = errno;
    //         std::cout <<" mb server  got rc  ["<<(int) rc << "]"<< std::endl;

    //         if (rc > 0) {
    //             hex_dump("Received Data", query, rc);
    //             //modbus_reply(ctx, query, rc, mb_mapping);
    //             std::cout <<" mb server  processing request "<< std::endl;

    //             process_modbus_request(ctx, query, rc, mb_mapping);
    //         } else if (rc == -1) {
    //             fprintf(stderr, "Connection failed: %s\n", modbus_strerror(err));

    //             break;  // Error or client disconnected
    //         }
    //     }

    //     modbus_close(ctx);
    //     modbus_free(ctx);
    // }
    return 0;
}
