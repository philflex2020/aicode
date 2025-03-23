#include <stdio.h>
#include <modbus.h>
#include <cerrno>

// rs485_server
//gcc -o rs485_server rs485_server.c -lmodbus
//gcc -o rs485_client rs485_client.c -lmodbus
int main() {
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