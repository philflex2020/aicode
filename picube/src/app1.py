from flask import Flask, render_template, request
from flask_socketio import SocketIO, emit
import threading
import time

# Global dictionary to store object information
objects = {}

app = Flask(__name__)
socketio = SocketIO(app)

@app.route('/')
def index():
    return render_template('index.html')

def plot_objects():
    while True:
        socketio.emit('update_objects', objects)
        time.sleep(0.1)

def move_object(obj_name, x, y, z, slew_rate):
    obj = objects[obj_name]
    current_x, current_y, current_z = obj['x'], obj['y'], obj['z']

    # Ensure that steps is at least 1 to avoid division by zero
    steps = max(int(max(abs(x - current_x), abs(y - current_y), abs(z - current_z)) / slew_rate), 1)

    for i in range(steps + 1):
        # Calculate new position directly without using steps
        new_x = current_x + (x - current_x) * i / steps
        new_y = current_y + (y - current_y) * i / steps
        new_z = current_z + (z - current_z) * i / steps
        obj['x'], obj['y'], obj['z'] = new_x, new_y, new_z
        time.sleep(0.1)

def move_object2(obj_name, x, y, z, slew_rate):
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
    emit('list_objects', objects)

@socketio.on('connect')
def handle_connect():
    print('Client connected')
    threading.Thread(target=plot_objects).start()

# @socketio.on('move_object')
# def handle_move_object(data):
#     obj_name = data['name']
#     x, y, z = data['x'], data['y'], data['z']
#     slew_rate = data['slew_rate']
#     move_object(obj_name, x, y, z, slew_rate)

@socketio.on('move_object')
def handle_move_object(data):
    obj_name = data['name']
    x, y, z = data['x'], data['y'], data['z']
    slew_rate = data['slew_rate']
    move_object(obj_name, x, y, z, slew_rate)
    emit('list_objects', objects)

@socketio.on('list_objects')
def handle_list_objects():
    #list_objects()
    emit('list_objects', objects)

if __name__ == "__main__":
    # Example objects (you can receive these from the socket)
    objects['cube1'] = {
        'x': 0,
        'y': 0,
        'z': 0,
        'size': 10,
        'color': 'red',
    }
    objects['cube2'] = {
        'x': 20,
        'y': 20,
        'z': 20,
        'size': 15,
        'color': 'blue',
    }

    socketio.run(app, host='localhost', port=9900)
