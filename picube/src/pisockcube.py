import socket
import threading
import time
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

# Global dictionary to store object information
objects = {}

def plot_object(obj_name):
    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')
    obj = objects[obj_name]
    vertices = obj['vertices']
    faces = obj['faces']
    color = obj['color']
    ax.add_collection3d(Poly3DCollection(vertices, facecolors=color, linewidths=1, edgecolors='r', alpha=.25))
    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_zlabel('Z')
    plt.show()

def move_object(obj_name, x, y, z, slew_rate):
    obj = objects[obj_name]
    current_x, current_y, current_z = obj['x'], obj['y'], obj['z']
    steps = int(max(abs(x - current_x), abs(y - current_y), abs(z - current_z)) / slew_rate)

    for i in range(steps + 1):
        new_x = current_x + (x - current_x) * i / steps
        new_y = current_y + (y - current_y) * i / steps
        new_z = current_z + (z - current_z) * i / steps
        obj['x'], obj['y'], obj['z'] = new_x, new_y, new_z
        time.sleep(0.1)

def list_objects():
    for obj_name, obj in objects.items():
        print(f"Object '{obj_name}': X={obj['x']}, Y={obj['y']}, Z={obj['z']}")

def handle_client(client_socket):
    data = client_socket.recv(1024).decode()
    command = data.split(' ')[0]
    print(command)
    
    if command == 'PLOT':
        obj_name = data.split(' ')[1]
        plot_object(obj_name)
    elif command == 'MOVE':
        obj_name = data.split(' ')[1]
        x, y, z = map(float, data.split(' ')[2:5])
        slew_rate = float(data.split(' ')[5])
        move_object(obj_name, x, y, z, slew_rate)
    elif command == 'LIST':
        list_objects()

    client_socket.close()

def start_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('localhost', 9999))
    server_socket.listen(5)

    print("Server is listening on port 9999")

    while True:
        client_socket, addr = server_socket.accept()
        client_handler = threading.Thread(target=handle_client, args=(client_socket,))
        client_handler.start()

if __name__ == "__main__":
    # Example objects (you can receive these from the socket)
    objects['cube'] = {
        'vertices': [[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0], [0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1]],
        'faces': [[0, 1, 2, 3], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [0, 3, 7, 4], [4, 5, 6, 7]],
        'color': 'b',
        'x': 0,
        'y': 0,
        'z': 0,
    }

    # Start the server to listen for socket commands
    start_server()