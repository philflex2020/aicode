from flask import Flask, request, jsonify
from sys import argv
import subprocess
import requests

if len(argv) < 2:
    print("Usage: python <scriptname> <server_ip>")
    exit(1)

# The IP address the server is running on
SERVER_IP = argv[1]

app = Flask(__name__)

@app.route('/run-command', methods=['GET'])
def run_command():
    # Extract command details
    dest = request.args.get('route')
    ip = request.args.get('ip')
    cmd = request.args.get('cmd')
    args = request.args.get('args')
    jobid = request.args.get('jobid')

    #Check if the destination is different from the server's IP
    if dest and dest != SERVER_IP:
        try:
            # Forward the request
            forwarded_url = f"http://{dest}:5000/run-command?{request.query_string.decode('utf-8')}"
            response = requests.get(forwarded_url)
            return (response.text, response.status_code, response.headers.items())
        except requests.ConnectionError:
            return jsonify({
                "status": "error",
                "dest" : dest,
                "message": "Connection refused to destination server"
            }), 200 #503  # Service Unavailable status code

    # Execute the command and get the result (pseudo-code, replace with actual implementation)
    # result = execute_the_command(ip, port, action, deviceId, type, offset, value)
    # For demonstration, we'll use ping
    #cmd = ['ping', '-c', '1', ip]
    try:
        #output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True)
        output = subprocess.check_output(cmd, args, stderr=subprocess.STDOUT)
        result = {
            "status": "success",
            "output": output.decode('utf-8')  # Decode bytes to string
        }
    except subprocess.CalledProcessError as e:
        result = {
            "status": "error",
            "output": e.output.decode('utf-8'),  # Decode bytes to string,
            "message": str(e)
        }
    print(result)
    return jsonify(result)

    # For now, return a dummy response
    #return jsonify(result="pass",value=12345)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)


