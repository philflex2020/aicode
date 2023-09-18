go mod init github.com/sysdcs/echo
go mod tidy
go build -o echo cmd/main.go
./echo -c test/gcom_test_client.json -mode modbus -output test
