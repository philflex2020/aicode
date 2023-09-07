package main

import (
    "log"
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