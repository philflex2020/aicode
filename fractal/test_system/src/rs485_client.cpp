#include <stdio.h>
#include <modbus.h>
#include <cerrno>

// rs485_client

int main() {
    modbus_t *ctx;
    uint16_t reg[1];

    // Create a new RTU context
    ctx = modbus_new_rtu("/dev/ttyUSB6", 115200, 'N', 8, 1);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create the libmodbus context\n");
        return -1;
    }

    // Set the Modbus address of the remote server
    modbus_set_slave(ctx, 1);

    // Establish a Modbus RTU connection
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    // Read holding registers
    if (modbus_read_registers(ctx, 0, 1, reg) == -1) {
        fprintf(stderr, "Failed to read register: %s\n", modbus_strerror(errno));
    } else {
        printf("Register 0: %d\n", reg[0]);
    }

    // Write to a register
    reg[0] = 54321;
    if (modbus_write_register(ctx, 0, reg[0]) == -1) {
        fprintf(stderr, "Failed to write register: %s\n", modbus_strerror(errno));
    } else {
        printf("Written 54321 to register 0\n");
    }

    // Close the connection and clean up
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}

