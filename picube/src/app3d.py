from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

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
    return render_template('index3.html')

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

# API route to move an object by its name
@app.route('/api/objects/<name>/move', methods=['PUT'])
def move_object(name):
    obj = find_object_by_name(name)
    if obj:
        data = request.get_json()
        obj['x'] = data['x']
        obj['y'] = data['y']
        obj['z'] = data['z']
        return jsonify({'message': 'Object moved successfully'})
    else:
        return jsonify({'message': 'Object not found'}), 404

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=8100, debug=True)
