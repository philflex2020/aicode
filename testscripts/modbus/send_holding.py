import json
import random
import subprocess

def send_command(command):
    subprocess.run(command, shell=True)

def generate_random_integer():
    return random.randint(1, 1000)

def main(config_file):
    with open(config_file, 'r') as f:
        data = json.load(f)

    for component in data["components"]:
        for register in component["registers"]:
            if register["type"] == "Holding":
                for mapping in register["map"]:
                    command = f"fims_send -m set -u {component['id']}/{component['registers'][0]['id']}/{mapping['id']} {generate_random_integer()}"
                    print("Running command:", command)
                    send_command(command)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script_name.py config_file")
        sys.exit(1)

    config_file = sys.argv[1]
    main(config_file)
    
