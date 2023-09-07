from flask import Flask, render_template, request
from pymodbus.client.sync import ModbusTcpClient
import json

app = Flask(__name__)

# Load configuration
with open('client_config.json') as f:
    config = json.load(f)

host = config['host']
port = config['port']

client = ModbusTcpClient(host, port)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/get', methods=['POST'])
def get_register():
    reg_type = request.form['type']
    reg_id = request.form['id']
    # find the register from config
    for reg_config in config['registers']:
        if reg_config['type'] == reg_type:
            for reg in reg_config['map']:
                if reg['id'] == reg_id:
                    offset = reg['offset']
                    break
    # read the register
    if reg_type == 'coil':
        response = client.read_coils(offset, 1)
        value = response.bits[0]
    elif reg_type == 'input_register':
        response = client.read_input_registers(offset, 1)
        value = response.registers[0]
    # ...
    return render_template('index.html', value=value)

@app.route('/set', methods=['POST'])
def set_register():
    reg_type = request.form['type']
    reg_id = request.form['id']
    value = int(request.form['value'])
    # find the register from config
    for reg_config in config['registers']:
        if reg_config['type'] == reg_type:
            for reg in reg_config['map']:
                if reg['id'] == reg_id:
                    offset = reg['offset']
                    break
    # write to the register
    if reg_type == 'coil':
        client.write_coil(offset, value)
    elif reg_type == 'holding_register':
        client.write_register(offset, value)
    # ...
    return render_template('index.html')

if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=True)