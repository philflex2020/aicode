import socket
import json
import argparse


##python3 pyclient.py -mrun -u/mysys/ess/ess_1 -b'{"type":"ess_master"}'
##python3 pyclient.py -mrun -u/mysys/ess/ess_1/bms/bms_2 -b'{"type":"bms_master", "data":"data for bms2"}

def py_client(host, port, uri, method, body):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        if body:
            json_data = json.dumps({"uri": uri, "body": body, "method":method})
        else:
            json_data = json.dumps({"uri": uri, "method":method})

        s.sendall(json_data.encode())
        data = s.recv(4096)
    print(f"{data.decode()}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Echo client - sends and receives JSON data.')
    parser.add_argument('-a', '--addr',   required=False,  default="127.0.0.1", help='Who to send to')
    parser.add_argument('-p', '--port',   required=False,  type=int, default=5000,            help='port to use')
    parser.add_argument('-m', '--method', required=True,                        help='Method to send to the server')
    parser.add_argument('-u', '--uri',    required=False,  default="/self",      help='URI to send to the server')
    parser.add_argument('-b', '--body',   required=False,                       help='Body content to send to the server')

    args = parser.parse_args()

    HOST = args.addr #'127.0.0.1'
    PORT = args.port  #345

    py_client(HOST, PORT, args.uri, args.method, args.body)

