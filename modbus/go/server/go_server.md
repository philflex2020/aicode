Sure, if you'd prefer to use Go (Golang) to create a Modbus server, you can use the `goburrow/modbus` library. This library provides a way to create a Modbus server (and client) in Go.

Here's a simple example of a Modbus TCP server using `goburrow/modbus`:

1. Install the Go language if you haven't already. You can download it from the [official website](https://golang.org/dl/).

2. Install the `goburrow/modbus` package by running the following command:

```
go get github.com/goburrow/modbus
```

3. Create a new Go file, for example `modbus_server.go`, and write the following code:

```go
package main

import (
    "log"
    "net"
    "time"

    "github.com/goburrow/modbus"
)

func main() {
    // Create a new Modbus TCP server
    handler := modbus.NewTCPServerHandler("localhost:5020")
    handler.Timeout = 10 * time.Second
    handler.IdleTimeout = 60 * time.Second

    err := handler.ListenAndServe()
    if err != nil {
        log.Fatal(err)
    }
    defer handler.Close()

    // Set the Modbus server address
    handler.Server().SetAddress(1, 100) // Set address 1 to value 100
}
```

In this example, the server will listen on `localhost` and port `5020`. You can change the address and port as needed. The server will set the value of register 1 to 100.

4. Run the server by executing the following command in the terminal:

```
go run modbus_server.go
```

Now you have a simple Modbus TCP server running in Go. You can expand this example to read a JSON config file to set up registers and respond to a Modbus client as per your requirements.