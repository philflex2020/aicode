from flask import Flask, request, redirect, url_for, render_template
from werkzeug.utils import secure_filename
from flask_sock import Sock
import os
import json

app = Flask(__name__)
sock = Sock(app)  # Initialize flask_sock

# Ensure there's a directory for uploaded files
UPLOAD_FOLDER = './data_temp'
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

connected_clients = []  # List to keep track of connected WebSocket clients



# Define a route for the action of the form
@app.route('/upload', methods=['POST'])
def upload_file():
    # ... [file handling code remains the same]
    print(f" request.files {request.files}")
    if 'file' not in request.files:
        print(" upload no file")
        return redirect(request.url)
    file = request.files['file']
    print(f" upload file {file}")
    # if user does not select file, browser also
    # submit an empty part without filename
    if file.filename == '':
        return redirect(request.url)

    if file:
        filename = secure_filename(file.filename)
        file.save(os.path.join(app.config['UPLOAD_FOLDER'], filename))
        print(f" saved file {filename}")
        
        # Simulate running the app and generating path_ids
        path_ids = simulate_app(filename)  # This function is a placeholder for your actual app logic
        # Send the path_ids to all connected WebSocket clients
        for client in connected_clients:
            try:
                jdata = {'filename': filename, 'path_ids': "mypath_ids"}
                sdata = json.dumps(jdata)
                client.send(sdata)  # Send as a JSON-serializable object
                print(f" sent ws data {sdata}")

            except Exception as e:
                print(f"Error sending message to client: {e}")
        # # Emit the results to the client
        # socketio.emit('update_path_ids', {'filename': filename, 'path_ids': path_ids})
        
        return redirect(url_for('home'))

    return render_template('index.html')  # Returning at the end as a fallback

# def upload_file():
#     print(" upload called")
#     #return render_template('index.html')  # You'll need to create this HTML file
#     # check if the post request has the file part
#     print(f" request.files {request.files}")
#     if 'file' not in request.files:
#         return redirect(request.url)
#     file = request.files['file']
#     # if user does not select file, browser also
#     # submit an empty part without filename
#     if file.filename == '':
#         return redirect(request.url)
#     if file:
#         filename = secure_filename(file.filename)
#         file.save(os.path.join(app.config['UPLOAD_FOLDER'], filename))
        
#         # Here you might call your pcap_modbus function
#         # to process the file or redirect to another page
        
#         return redirect(url_for('home'))

def simulate_app(filename):
    # Simulate processing and return a list of path_ids
    return ['path_id1', 'path_id2', 'path_id3']  # Replace with actual l

# Rest of your Flask application...
#app = Flask(__name__)
#socketio = SocketIO(app)
@sock.route('/echo')
def echo(ws):
    connected_clients.append(ws)  # Add the new WebSocket connection to the list
    try:
        while True:
            data = ws.receive()  # Wait for data from the WebSocket
            print(f" data received {data}")
            # Here, you might handle incoming messages or keep the connection open
    finally:
        connected_clients.remove(ws)  # Clean up when the connection is closed

@app.route('/')
def home():
    return render_template('index.html')  # You'll need to create this HTML file

if __name__ == '__main__':
    app.run (debug=True)
