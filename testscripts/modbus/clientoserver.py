Of course! We can utilize the `argparse` module from Python's standard library to accomplish this. This allows the user to specify input and output filenames from the command line.

Here's the updated script:

```python
import json
import argparse

def client_to_server(client_config):
    server_config = {
        "system": {
            "name": client_config["connection"]["name"],
            "protocol": client_config["connection"]["device protocol"],
            "id": client_config["connection"]["name"],
            "ip_address": "0.0.0.0",
            "port": client_config["connection"]["port"],
            "server_delay": 510  # Default value as per the provided sample
        },
        "registers": {
            "discrete_inputs": [],
            "input_registers": []
        }
    }

    for component in client_config["components"]:
        for register in component["registers"]:
            if register["type"] == "Discrete Inputs":
                for item in register["map"]:
                    discrete_input = {
                        "name": item["name"],
                        "id": item["id"],
                        "offset": item["offset"],
                        "uri": f"/components/{component['id']}"
                    }
                    server_config["registers"]["discrete_inputs"].append(discrete_input)
            elif register["type"] == "Input Registers":
                for item in register["map"]:
                    input_register = {
                        "name": item["name"],
                        "id": item["id"],
                        "offset": item["offset"],
                        "uri": f"/components/{component['id']}"
                    }
                    server_config["registers"]["input_registers"].append(input_register)

    return server_config

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert client configuration to server configuration.")
    parser.add_argument("client_file", help="Path to the client configuration file.")
    parser.add_argument("server_file", help="Path to save the converted server configuration file.")

    args = parser.parse_args()

    with open(args.client_file, "r") as f:
        client_config = json.load(f)

    server_config = client_to_server(client_config)

    with open(args.server_file, "w") as out_f:
        json.dump(server_config, out_f, indent=4)

    print(f"Server configuration has been saved to {args.server_file}")
```

To run the script:

1. Save it to a file, say `converter.py`.
2. Use the following command:
```
python converter.py path_to_client_config.json path_to_output_server_config.json
```
Replace `path_to_client_config.json` with the path to your client configuration file and `path_to_output_server_config.json` with the path where you want the server configuration to be saved.