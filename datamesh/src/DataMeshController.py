import socket
import json
import subprocess
import threading

class DataMeshController:
    def __init__(self, port=5000):
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('', self.port))
        self.sock.listen(5)

    def start(self):
        print(f"DataMeshController started on port {self.port}")
        while True:
            client, addr = self.sock.accept()
            threading.Thread(target=self.handle_client, args=(client, addr)).start()

    def handle_client(self, client_socket, addr):
        # Send a hello message
        #hello_message = json.dumps({"message": "hello", "from": "DataMeshController"})
        #client_socket.sendall(hello_message.encode('utf-8'))

        # Receive and process the request
        data = client_socket.recv(1024)
        try:
            request = json.loads(data.decode('utf-8'))
            if "command" in request.keys() and "port" in request.keys() and "test"  in request.keys():
                if request["command"] == "run":
                    app_name = request["app"] + ".py"
                    self.run_app(app_name, request["port"])
                    run_message = json.dumps({"app": "running"})
                    client_socket.sendall(run_message.encode('utf-8'))
                else:
                    run_message = json.dumps({"app": "error"})
                    client_socket.sendall(run_message.encode('utf-8'))
            else:
                run_message = json.dumps({"message": "error"})
                client_socket.sendall(run_message.encode('utf-8'))

        except:
            run_message = json.dumps({"json": "error"})
            client_socket.sendall(run_message.encode('utf-8'))

        client_socket.close()

    def run_app(self, app_name, port):
        print(f"Running {app_name} on port {port}")
        # Launch the app as a subprocess
        #subprocess.Popen(["python3", app_name, str(port)])

# Example to start the controller
if __name__ == "__main__":
    controller = DataMeshController()
    controller.start()