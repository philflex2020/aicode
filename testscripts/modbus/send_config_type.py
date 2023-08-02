import json
import random
import subprocess
import sys

def send_command(command):
    subprocess.run(command, shell=True)

def generate_random_integer():
    return random.randint(1, 1000)

def main(config_file, register_type, fims_cmd):
    with open(config_file, 'r') as f:
        data = json.load(f)

    for component in data["components"]:
        print("Running component:", component['id'])
        for register in component["registers"]:
            if register["type"] == register_type:
                for mapping in register["map"]:
                    command = f"fims_send -m {fims_cmd} -u /components/{component['id']}/{mapping['id']} {generate_random_integer()}"
                    print("Running map:", mapping['id'])
                    print (command)
                    #send_command(command)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python script_name.py config_file register_type fims_cmd")
        sys.exit(1)

    config_file = sys.argv[1]
    register_type = sys.argv[2]
    fims_cmd = sys.argv[3]

    main(config_file, register_type, fims_cmd)
    
