import socket
import threading
import time
import sys
import sdl2.ext
from sdl2 import SDL_QUIT, SDL_WINDOWPOS_CENTERED, SDL_WINDOW_SHOWN

Quit = False
# Global dictionary to store object information
objects = {}

def plot_objects():
    sdl2.ext.init()
    window = sdl2.ext.Window("3D Objects", size=(800, 600), flags=SDL_WINDOW_SHOWN)
    window.show()

    running = True
    while running:
        events = sdl2.ext.get_events()
        for event in events:
            if event.type == SDL_QUIT:
                running = False
                break

        renderer = sdl2.ext.Renderer(window)
        renderer.draw_color = sdl2.ext.Color(0, 0, 0)
        renderer.clear()

        for obj_name, obj in objects.items():
            x, y, z = obj['x'], obj['y'], obj['z']
            size = obj['size']
            color = obj['color']

            sdl2.ext.draw_cube(renderer, (x, y, z, size), color)

        renderer.present()
        time.sleep(0.1)

    sdl2.ext.quit()

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
    
    if command == 'PLOT':
        obj_name = data.split(' ')[1]
        plot_objects()
    elif command == 'MOVE':
        obj_name = data.split(' ')[1]
        x, y, z = map(float, data.split(' ')[2:5])
        slew_rate = float(data.split(' ')[5])
        move_object(obj_name, x, y, z, slew_rate)
    elif command == 'LIST':
        list_objects()
    elif command == 'QUIT':
        Quit = True

    client_socket.close()

def start_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('localhost', 9909))
    server_socket.listen(5)

    print("Server is listening on port 9909")

    while not Quit:
        client_socket, addr = server_socket.accept()
        client_handler = threading.Thread(target=handle_client, args=(client_socket,))
        client_handler.start()

if __name__ == "__main__":
    # Example objects (you can receive these from the socket)
    objects['cube'] = {
        'x': 0,
        'y': 0,
        'z': 0,
        'size': 100,
        'color': sdl2.ext.Color(255, 0, 0),
    }

    # Start the server to listen for socket commands
    start_server()