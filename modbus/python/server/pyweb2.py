from flask import Flask, render_template
import json

app = Flask(__name__)

@app.route('/')
def index():
    with open('client_config.json', 'r') as f:
        config = json.load(f)
    return render_template('index.html', config=config)

if __name__ == '__main__':
    app.run(host='0.0.0.0', debug=True)
