from flask import Flask, render_template, request, redirect
from pymodbus.client.sync import ModbusTcpClient
import json

app = Flask(__name__)

@app.route('/')
def index():
    with open('client_config.json', 'r') as f:
        config = json.load(f)

    client = ModbusTcpClient(config['host'], port=config['port'])
    client.connect()

    registers = []
    for register in config['registers']:
        for reg in register['map']:
            if register['type'] == 'coil':
                response = client.read_coils(reg['offset'], count=reg['size'])
                values = None
                bits = response.bits
            elif register['type'] == 'input_register':
                response = client.read_input_registers(reg['offset'], count=reg['size'])
                values = [response.registers]
                bits = None
            registers.append({'type': register['type'], 'id': reg['id'], 'offset': reg['offset'], 'size': reg['size'], 'vals': values , 'bits' : bits})

    client.close()

    return render_template('index.html', registers=registers)

# @app.route('/')
# def index():
#     try:
#         with open('client_config.json', 'r') as f:
#             config = json.load(f)
#     except FileNotFoundError:
#         return "Configuration file not found", 500
#     except json.JSONDecodeError:
#         return "Configuration file format is not valid", 500

#     client = ModbusTcpClient(config['host'], port=config['port'])
#     if not client.connect():
#         return "Connection to Modbus server failed", 500

#     registers = []
#     for register in config['registers']:
#         for reg in register['map']:
#             if register['type'] == 'coil':
#                 response = client.read_coils(reg['offset'], count=reg['size'])
#                 print(response)
#                 print(response.bits)
#                 if not response.isError():
#                     bits = response.bits
#                     values = None
#                     #values = response.bits
#                 else:
#                     client.close()
#                     return f"Error reading coil at offset {reg['offset']}", 500
#             elif register['type'] == 'input_register':
#                 response = client.read_input_registers(reg['offset'], count=reg['size'])
#                 print(response)
#                 print(response.registers)
#                 if not response.isError():
#                     values = response.registers
#                     bits = None
#                 else:
#                     client.close()
#                     return f"Error reading input register at offset {reg['offset']}", 500
#             #registers.append({'type': register['type'], 'id': reg['id'], 'offset': reg['offset'], 'size': reg['size'], 'values': values if isinstance(values, list) else [values]})
#             registers.append({'type': register['type'], 'id': reg['id'], 'offset': reg['offset'], 'size': reg['size'], 'values': values, 'bits': bits})






    client.close()
    for register in registers:
        print(register['values'])

    return render_template('index.html', registers=registers)


@app.route('/set_register', methods=['POST'])
def set_register():
    with open('client_config.json', 'r') as f:
        config = json.load(f)

    type = request.form['type']
    offset = int(request.form['offset'])
    size = int(request.form['size'])
    print (request.form.getlist('value'))
    try:
        values = [int(val) for val in request.form.getlist('value')]
    except:
        pass
    try:
        bits = [bit for bit in request.form.getlist('value')]
    except:
        pass

    #    values = [request.form.getlist('value')]

    client = ModbusTcpClient(config['host'], port=config['port'])
    client.connect()

    if type == 'coil':
        print(bits)
        client.write_coils(offset, bits)
    elif type == 'input_register':
        client.write_registers(offset, values)

    client.close()

    return redirect('/')

if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=True)
