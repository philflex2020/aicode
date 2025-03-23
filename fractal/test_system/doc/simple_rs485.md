Creating a simple standalone RS485 client and server using `libmodbus` requires setting up each side to communicate over a serial connection. Below, you will find a straightforward implementation of both the Modbus RTU server and client using `libmodbus` in C. This example assumes a Linux environment since `libmodbus` and serial communication are typically used in such settings.

### Modbus RTU Server (C Code)

The server listens for Modbus RTU requests over a serial line and responds to read and write requests for holding registers.

```c
#include <stdio.h>
#include <modbus.h>

int main() {
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;

    // Create a new RTU context with the specified serial parameters
    ctx = modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1);
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
```

### Modbus RTU Client (C Code)

The client sends a request to read from and write to the server's holding registers.

```c
#include <stdio.h>
#include <modbus.h>

int main() {
    modbus_t *ctx;
    uint16_t reg[1];

    // Create a new RTU context
    ctx = modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1);
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
```

### Compile and Run

To compile these programs, make sure you have `libmodbus` installed and link against it. Here's how you might compile them:

```bash
gcc -o modbus_server server.c -lmodbus
gcc -o modbus_client client.c -lmodbus
```

Run the server first, then the client. Make sure your RS485 device is correctly connected and configured on your machine (`/dev/ttyUSB0` is just an example and may differ on your system).

This example provides a basic setup for RS485 communication using Modbus RTU protocol. Adjust the serial port settings and register addresses based on your specific application needs.