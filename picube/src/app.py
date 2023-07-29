from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import eventlet
import socket
import threading
import json

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret_key'
socketio = SocketIO(app, cors_allowed_origins='*', async_mode='eventlet')

# Dictionary to store objects with name, x, y, z, size, color, and slew rate
objects = {}

@socketio.on('connect')
def handle_connect():
    emit('list_objects', objects)

@socketio.on('list_objects')
def list_objects():
    emit('list_objects', objects)

@socketio.on('add_object')
def add_object(data):
    name = data['name']
    x, y, z = data['x'], data['y'], data['z']
    size = data['size']
    color = data['color']
    slew_rate = data['slew_rate']

    if name not in objects:
        objects[name] = {'x': x, 'y': y, 'z': z, 'size': size, 'color': color, 'slew_rate': slew_rate}
        emit('list_objects', objects)
    else:
        emit('message', {'text': f'Object "{name}" already exists.', 'type': 'error'})

@socketio.on('delete_object')
def delete_object(name):
    if name in objects:
        del objects[name]
        emit('list_objects', objects)
    else:
        emit('message', {'text': f'Object "{name}" not found.', 'type': 'error'})

@socketio.on('move_object')
def move_object(data):
    obj_name = data['name']
    x, y, z = data['x'], data['y'], data['z']
    slew_rate = data['slew_rate']

    if obj_name in objects:
        current_x, current_y, current_z = objects[obj_name]['x'], objects[obj_name]['y'], objects[obj_name]['z']
        steps = 100  # Number of steps for smooth motion

        for i in range(steps):
            new_x = current_x + (x - current_x) * i / steps
            new_y = current_y + (y - current_y) * i / steps
            new_z = current_z + (z - current_z) * i / steps

            objects[obj_name]['x'], objects[obj_name]['y'], objects[obj_name]['z'] = new_x, new_y, new_z
            emit('update_objects', objects)
            socketio.sleep(slew_rate / steps)

    else:
        emit('message', {'text': f'Object "{obj_name}" not found.', 'type': 'error'})

@app.route('/')
def index():
    return render_template('index.html')

def control_socket_thread():
    control_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    control_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    control_socket.bind(('0.0.0.0', 9001))
    control_socket.listen()

    print("Control socket listening on port 9001")

    while True:
        client_socket, _ = control_socket.accept()
        control_thread = threading.Thread(target=handle_control_connection, args=(client_socket,))
        control_thread.start()

def handle_control_connection(client_socket):
    while True:
        try:
            data = client_socket.recv(1024)
            if not data:
                break

            command = data.decode().strip()
            response = b''

            if command == 'list':
                response = json.dumps(objects).encode()
            elif command.startswith('add'):
                try:
                    _, name, x, y, z, size, color, slew_rate = command.split()
                    x, y, z, size, slew_rate = float(x), float(y), float(z), float(size), float(slew_rate)
                    color = color.strip()
                    data = {
                        'name': name,
                        'x': x,
                        'y': y,
                        'z': z,
                        'size': size,
                        'color': color,
                        'slew_rate': slew_rate
                    }
                    add_object(data)
                    response = b'Success'
                except ValueError:
                    response = b'Invalid parameters'
            elif command.startswith('delete'):
                try:
                    _, name = command.split()
                    delete_object(name)
                    response = b'Success'
                except ValueError:
                    response = b'Invalid parameters'
            elif command.startswith('move'):
                try:
                    _, name, x, y, z, slew_rate = command.split()
                    x, y, z, slew_rate = float(x), float(y), float(z), float(slew_rate)
                    data = {
                        'name': name,
                        'x': x,
                        'y': y,
                        'z': z,
                        'slew_rate': slew_rate
                    }
                    move_object(data)
                    response = b'Success'
                except ValueError:
                    response = b'Invalid parameters'
            elif command == 'help':
                help_message = """Available commands:
                - list: List all objects.
                - add <name> <x> <y> <z> <size> <color> <slew_rate>: Add a new object.
                - delete <name>: Delete an object by name.
                - move <name> <x> <y> <z> <slew_rate>: Move an object to new coordinates with a defined slew rate.
                - help: Show this help message."""
                response = help_message.encode()
            else:
                response = b'Unknown command'

            client_socket.sendall(response)
        except ConnectionResetError:
            # Handle the case when the client socket is closed unexpectedly
            break
        except Exception as e:
            print(f"Error while handling control connection: {e}")
            break

    client_socket.close()

if __name__ == "__main__":
    control_thread = threading.Thread(target=control_socket_thread)
    control_thread.start()
    socketio.run(app, host='0.0.0.0', port=8001)
