import socket
import json
import subprocess
import threading
from DataMeshServer import DataMeshServer

server_hosts = {}

class DataMeshController:
    def __init__(self, port=5000):
        self.port = port
        self.server_threads = {}
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
            print(request)
            if "method" in request.keys():# and "port" in request.keys(): # and "test"  in request.keys():
                if request["method"] == "add" and "body" in request.keys():
                    print(" got method add and a body")
                    # app_name = request["app"] + ".py"
                    bodys = request["body"]
                    body = json.loads(bodys)
                    print(body)
                    port = body["port"]
                    # Check if server is already running on the requested port
                    if port not in self.server_threads:
                        print("Starting server on port:", port)

                        hostname = socket.gethostname()  # Assuming server_hosts is defined somewhere
                        dmserver = {}
                        dmserver["port"] = port
                        name = body.get("name", "dmserver")

                        dmserver["name"] = name
                        self.run_server(name, port)  # Handle starting the server

                        server_hosts[hostname]["ports"].append(dmserver)

                        run_message = json.dumps({"app": "running", "port": port})
                    else:
                        run_message = json.dumps({"app": "already running", "port": port})
                
                    client_socket.sendall(run_message.encode('utf-8'))
                    
                elif request["method"] == "show":
                    #app_name = request["app"] + ".py"
                    #self.run_app(app_name, request["port"])
                    run_message = json.dumps(server_hosts)
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

    def run_server(self, name, port):
        # Initialize the DataMeshServer here, assuming it's a class you've defined
        server = DataMeshServer(port)

        # Create a new thread for the server and start it
        server_thread = threading.Thread(target=server.start)
        server_thread.start()

        # Store the thread reference if needed for later management
        self.server_threads[port] = server_thread

        print(f"DataMeshServer {name} started on port {port} as a background thread.")

    def run_app(self, app_name, port):
        print(f"Running {app_name} on port {port}")
        # Launch the app as a subprocess
        #subprocess.Popen(["python3", app_name, str(port)])

# Example to start the controller
if __name__ == "__main__":
    hostname = socket.gethostname()
    print(hostname)
    local_ip = socket.gethostbyname(hostname)
    controller = DataMeshController()
    #port = 5000
    server_hosts[hostname] = {}
    server_hosts[hostname]["name"] = hostname
    server_hosts[hostname]["ip_address"] = local_ip
    server_hosts[hostname]["ports"] = []
    dmserver = {}
    dmserver["port"] = controller.port
    dmserver["app"] = "dmcontroller"

    server_hosts[hostname]["ports"].append(dmserver)
    # port = 345
    # print("main starting Server")
    # server = DataMeshServer(port)
    # server.start()
    # server_hosts[hostname]["ports"] = server_hosts[hostname]["ports"].append(server.port)
    controller.start()

    ##python3 pymsg.py -p5000 -mshow 
    ##python3 pymg.py -p5000 -mshow -u/mysys/ess/ess_1 -b'{"type":"ess_master"}'
