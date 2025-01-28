from flask import Flask, jsonify, json, render_template

app = Flask(__name__)

@app.route('/')
def home():
    return render_template('home.html')

@app.route('/data')
def serve_data():
    with open('./data2.json', 'r') as file:
        data = json.load(file)
    return jsonify(data)

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0')
