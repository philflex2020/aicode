from flask import Flask, request, jsonify
import subprocess


app = Flask(__name__)

@app.route('/run-command', methods=['GET'])
def run_command():
    # Extract command details
    ip = request.args.get('ip')
    port = request.args.get('port')
    action = request.args.get('action')
    deviceId = request.args.get('deviceId')
    type = request.args.get('type')
    offset = request.args.get('offset')
    value = request.args.get('value')
    rowId = request.args.get('rowId')

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


