#include <stdio.h>
#include <modbus.h>
#include <cerrno>

// rs485_server
//gcc -o rs485_server rs485_server.c -lmodbus
//gcc -o rs485_client rs485_client.c -lmodbus

#include <stdio.h>
#include <modbus.h>

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

void xprint_query_details(modbus_t *ctx, uint8_t *query) {
//    int header_length = modbus_get_header_length(ctx);
//    int function_code = query[header_length - 1];  // Function code is the byte after the header
            // Print details about the request (extracting relevant data from the query)
            int header_length = modbus_get_header_length(ctx);
            int function_code = query[header_length - 1];
            int address = (query[header_length] << 8) + query[header_length + 1];
            int num = (query[header_length + 2] << 8) + query[header_length + 3];
            
            printf("Received function %d at address %d for %d registers\n", function_code, address, num);

    // // Extracting the starting address and number of registers from the query
    // // These are stored in the bytes following the function code, big-endian format
    // int address = (query[header_length] << 8) + query[header_length + 1];
    // int num = (query[header_length + 2] << 8) + query[header_length + 3];

    // // Depending on the function code, you might want to handle more or less data
    // printf("Data received - Function Code: %d, Starting Register: %d, Number of Registers: %d\n", function_code, address, num);

    // For function codes that involve writing, print the new values (assuming holding registers for simplicity)
    if (function_code == 0x10 || function_code == 0x06) { // Preset Multiple Registers or Preset Single Register
        printf("Values: ");
        for (int i = 0; i < num; i++) {
            int value = (query[header_length + 5 + 2 * i] << 8) + query[header_length + 5 + 2 * i + 1];
            printf("%d ", value);
        }
        printf("\n");
    }
}


int start_server() {
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;

    // Create a new RTU context with the specified serial parameters
    ctx = modbus_new_rtu("/dev/ttyUSB5", 115200, 'N', 8, 1);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create the libmodbus context\n");
        return -1;
    }

    // Set the Modbus address of this server
    modbus_set_slave(ctx, 1);

    // Allocate memory for the Modbus mapping
    mb_mapping = modbus_mapping_new(0, 0, 100, 0);
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

    for (;;) {
        uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];
        int rc = modbus_receive(ctx, query);

        if (rc > 0) {
            hex_dump("Received Data", query, rc);  // Call to hex dump function
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
    start_server();
    return 0;
}