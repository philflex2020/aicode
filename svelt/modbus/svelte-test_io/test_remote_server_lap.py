import requests

def test_run_command():
    url = "http://127.0.0.1:5000/run-command"
    params = {
        "route": "192.168.86.76",
        "ip": "192.168.86.76",
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

if __name__ == "__main__":
    test_run_command()