from flask import Flask, request
import asyncio
import websockets
Pi_IP = '192.168.86.191'
app = Flask(__name__)

async def send_ws_command(command):
    uri = f"ws://{Pi_IP}:8765"
    async with websockets.connect(uri) as websocket:
        await websocket.send(command)
        response = await websocket.recv()
        return response

@app.route('/gpio', methods=['POST'])
def control_gpio():
    command = request.form['command']
    response = asyncio.run(send_ws_command(command))
    return response

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
