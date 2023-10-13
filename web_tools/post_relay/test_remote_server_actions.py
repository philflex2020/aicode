import requests

def test_run_command():
    url = "http://127.0.0.1:5000/run-command"
    # docker will direct this to the docker node attached to port 5000
    # python_server_dest must be running on that docker image and on the 172.17.0.2 image 
    params = {
        "route": "172.17.0.2",
        "ip": "172.17.0.3",
        "port": "8080",
        "action": "sample_action",
        "deviceId": "sample_device",
        "type": "sample_type",
        "offset": "sample_offset",
        "value": "sample_value",
        "rowId": "sample_rowId"
    }
    
    try:
        response = requests.get(url, params=params)
        response.raise_for_status()
        
        print("Server Response:", response.json())
    except requests.RequestException as error:
        print(f"Error testing server: {error}")
        print("Server Response:", response.json())

# Test the POST endpoint
def test_post_command():
    actions = [
        {"id": "1", "delay": 0, "ip": "127.0.0.1", "func": "somefunc", "args": "someargs"},
        {"id": "2", "delay": 0.2, "ip": "127.0.0.1", "func": "somefunc", "args": "someargs"},
        # ... Add more actions if needed
    ]

    headers = {
        'Content-Type': 'application/json'
    }

    response = requests.post(SERVER_URL, data=json.dumps({'actions': actions}), headers=headers)
    print("POST Response:", response.json())
    
if __name__ == "__main__":
    test_run_command()