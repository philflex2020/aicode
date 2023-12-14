import socket
import json
import argparse

def echo_client(host, port, uri, body):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        json_data = json.dumps({"uri": uri, "body": body})
        s.sendall(json_data.encode())
        data = s.recv(1024)
    print(f"Received: {data.decode()}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Echo client - sends and receives JSON data.')
    parser.add_argument('-u', '--uri', required=True, help='URI to send to the server')
    parser.add_argument('-b', '--body', required=True, help='Body content to send to the server')

    args = parser.parse_args()

    HOST = '127.0.0.1'
    PORT = 65432

    echo_client(HOST, PORT, args.uri, args.body)

