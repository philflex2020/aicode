from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit

app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret_key'
socketio = SocketIO(app)

# Define a list to store rectangular objects
objects = []

# Function to find an object by its name
def find_object_by_name(name):
    for obj in objects:
        if obj['name'] == name:
            return obj
    return None

# Route to serve the HTML page
@app.route('/')
def index():
    return render_template('index4.html')

# API route to list all objects
@app.route('/api/objects', methods=['GET'])
def list_objects():
    return jsonify(objects)

# API route to create a new object
@app.route('/api/objects', methods=['POST'])
def create_object():
    data = request.get_json()
    objects.append(data)
    return jsonify({'message': 'Object created successfully'})

# API route to delete an object by its name
@app.route('/api/objects/<name>', methods=['DELETE'])
def delete_object(name):
    obj = find_object_by_name(name)
    if obj:
        objects.remove(obj)
        return jsonify({'message': 'Object deleted successfully'})
    else:
        return jsonify({'message': 'Object not found'}), 404

# API route to update an object's properties by its name
@app.route('/api/objects/<name>', methods=['PUT'])
def update_object(name):
    obj = find_object_by_name(name)
    if obj:
        data = request.get_json()
        obj.update(data)
        return jsonify({'message': 'Object updated successfully'})
    else:
        return jsonify({'message': 'Object not found'}), 404

# WebSocket event handler for 'move_object'
@socketio.on('move_object')
def handle_move_object(data):
    name = data.get('name')
    obj = find_object_by_name(name)
    if obj:
        obj['x'] = data['x']
        obj['y'] = data['y']
        obj['z'] = data['z']
        emit('object_moved', obj, broadcast=True)  # Send the updated object to all connected clients

if __name__ == "__main__":
    socketio.run(app, host='0.0.0.0', port=8101, debug=True)
