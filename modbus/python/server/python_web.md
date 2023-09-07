Creating a web interface for your Modbus client can be done using a web framework like Flask.

Here's a basic example of how you could set up a Flask application that will allow you to get or set Modbus registers via a web interface.

1. Install Flask by running `pip install flask`.

2. Create a file for your application, e.g., `web_client.py`.

3. In `web_client.py`, import the necessary modules and set up your Flask application:

```python
from flask import Flask, render_template, request
from pymodbus.client.sync import ModbusTcpClient
import json

app = Flask(__name__)

# Load configuration
with open('config.json') as f:
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
    app.run(debug=True)
```

4. Create a template for your web interface, e.g., `templates/index.html`:

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Modbus Client</title>
</head>
<body>
    <h1>Modbus Client</h1>
    <form action="/get" method="post">
        <h2>Get Register</h2>
        <label for="type">Type:</label>
        <select id="type" name="type">
            <option value="coil">Coil</option>
            <option value="input_register">Input Register</option>
            <!-- ... -->
        </select><br>
        <label for="id">ID:</label>
        <input type="text" id="id" name="id"><br>
        <input type="submit" value="Get">
    </form>
    <form action="/set" method="post">
        <h2>Set Register</h2>
        <label for="type">Type:</label>
        <select id="type" name="type">
            <option value="coil">Coil</option>
            <option value="holding_register">Holding Register</option>
            <!-- ... -->
        </select><br>
        <label for="id">ID:</label>
        <input type="text" id="id" name="id"><br>
        <label for="value">Value:</label>
        <input type="text" id="value" name="value"><br>
        <input type="submit" value="Set">
    </form>
    {% if value %}
    <h2>Value: {{ value }}</h2>
    {% endif %}
</body>
</html>
```

5. Run your Flask application by running `python web_client.py`.

6. Open a web browser and navigate to `localhost:5000`.

You should now be able to get or set Modbus registers via the web interface. Adjust the application according to your requirements.