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

def format_ping_output(output):
    lines = output.split('\n')
    
    # Filter out empty lines and split each line into a list of words
    word_lines = [line.split() for line in lines if line]

    formatted_output = {}
    formatted_output["Destination"] = word_lines[1][1]

    # Extract the replies
    replies = [line for line in word_lines if "Reply" in line and "from" in line]
    formatted_output["Replies"] = []
    for reply in replies:
        formatted_output["Replies"].append({
            "from": reply[2],
            "bytes": reply[4]
        })
#            "time": reply[6],
#           "TTL": reply[-1]

    # Extract statistics
    for line in word_lines:
        if "Packets:" in line:
            formatted_output["Sent"] = line[line.index("Sent") + 2]
            formatted_output["Received"] = line[line.index("Received") + 2]
            formatted_output["Lost"] = line[line.index("Lost") + 2].split(' ')[0]
        if "Approximate" in line:
            formatted_output["Minimum"] = line[line.index("Minimum") + 2]
            formatted_output["Maximum"] = line[line.index("Maximum") + 2]
            formatted_output["Average"] = line[line.index("Average") + 2]

    return formatted_output

def fold_ormat_ping_output(output):
    lines = output.split('\n')
    
    # Filter out empty lines and split each line into a list of words
    lines = [line.split() for line in lines if line]

    formatted_output = {}
    formatted_output["Destination"] = lines[1][1]

    # Extract the replies
    replies = [line for line in lines if "Reply from" in line]
    formatted_output["Replies"] = []
    for reply in replies:
        formatted_output["Replies"].append({
            "from": reply[2],
            "bytes": reply[4],
            "time": reply[6],
            "TTL": reply[-1]
        })

    # Extract statistics
    for line in lines:
        if "Packets:" in line:
            stats = line.split(',')
            formatted_output["Sent"] = stats[0].split('=')[1].strip()
            formatted_output["Received"] = stats[1].split('=')[1].strip()
            formatted_output["Lost"] = stats[2].split('=')[1].strip().split(' ')[0]
        if "Approximate" in line:
            times = line.split(',')
            formatted_output["Minimum"] = times[0].split('=')[1].strip()
            formatted_output["Maximum"] = times[1].split('=')[1].strip()
            formatted_output["Average"] = times[2].split('=')[1].strip()

    return formatted_output


# ...

# For demonstration, we'll use ping
# cmd = ['ping', '-c', '1', ip]
# try:
#     output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
#     formatted_output = format_ping_output(output.decode('utf-8'))
#     result = {
#         "status": "success",
#         "output": formatted_output
#     }
# except subprocess.CalledProcessError as e:
#     result = {
#         "status": "error",
#         "output": e.output.decode('utf-8'),  # Decode bytes to string,
#         "message": str(e)
#     }
# print(result)


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
            print("forwarding request")
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
        print("running request")

        #output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True)
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        #formatted_output = format_ping_output(output.decode('utf-8'))
        #    formatted_output  # Decode bytes to string
        result = {
            "status": "success",
            "output": output.decode('utf-8')
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


