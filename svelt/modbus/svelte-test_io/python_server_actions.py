from flask import Flask, request, jsonify
from sys import argv
import subprocess
import requests
from time import sleep


if len(argv) < 2:
    print("Usage: python <scriptname> <server_ip>")
    exit(1)

# The IP address the server is running on
SERVER_IP = argv[1]

app = Flask(__name__)

@app.route('/run-command', methods=['POST'])
def run_command():
    # Get actions from the POSTed JSON
    actions = request.json.get('actions')
    
    results = []

    for action in actions:
        ip = action.get('ip')
        func = action.get('func')
        args = action.get('args')
        delay = action.get('delay', 0)
        action_id = action.get('id')

        sleep(delay)  # Introducing delay

        output = ""
        status = "success"

        if ip == SERVER_IP:
            # Execute locally
            try:
                if func == "ping":
                    # Using ping for the specified IP
                    cmd = ['ping', '-c', '1', ip]
                    output = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode('utf-8')

                elif func == "fetch":
                    # Using HTTP get request for the specified URL in args
                    response = requests.get(args)
                    output = response.text

                else:
                    # Unsupported function
                    status = "error"
                    output = f"Unsupported function: {func}"

            except Exception as e:
                # Generic error handling
                status = "error"
                output = str(e)
        else:
            # Forward the command to the remote server
            try:
                response = requests.post(f"http://{ip}:5000/run-command", json={"actions": [action]})
                remote_result = response.json()[0]  # We expect a list with a single result
                status = remote_result["status"]
                output = remote_result["output"]
            except requests.ConnectionError:
                status = "error"
                output = "Connection refused to destination server"
            except Exception as e:
                status = "error"
                output = str(e)

        # Append results for this action
        results.append({
            "id": action_id,
            "status": status,
            "output": output
        })
    
    return jsonify(results)

@app.route('/run-command', methods=['GET'])
def run_command():
    # Extract command details
    dest = request.args.get('route')
    ip = request.args.get('ip')
    port = request.args.get('port')
    action = request.args.get('action')
    deviceId = request.args.get('deviceId')
    type = request.args.get('type')
    offset = request.args.get('offset')
    value = request.args.get('value')
    rowId = request.args.get('rowId')

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
    cmd = ['ping', '-c', '1', ip]
    try:
        #output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True)
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
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


