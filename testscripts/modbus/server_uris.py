import os
import json
import sys

def create_dir_structure(data, parent_path=''):
    for entry in data:
        if isinstance(entry, dict):
            for key, value in entry.items():
                if key == 'uri':
                    dir_path = os.path.join(parent_path, value.strip('/').replace('/', '_'))
                    os.makedirs(dir_path, exist_ok=True)
                elif isinstance(value, list):
                    create_dir_structure(value, parent_path)
                elif isinstance(value, dict):
                    create_dir_structure([value], parent_path)

def list_all_uris(data):
    uris = set()

    def extract_uris(data):
        if isinstance(data, dict):
            if 'device_id' in data.keys():
                print ("    found device_id :",data['device_id'])
            
        for entry in data:
            if isinstance(entry, dict):
                if 'device_id' in entry.keys():
                    print ("  found device_id :",entry['device_id'])
                if 'uri' in entry.keys():
                    print ("    found uri :",entry['uri'])
                    if 'offset' in entry.keys():
                        print ("    found offset :",entry['offset'] )
                    if 'id' in entry.keys():
                        print ("    found id",entry['id'])

                for key, value in entry.items():
                    if key == 'device_id':
                        print("  y device_id :", value)
                    if key == 'uri':
                        print(value)
                        uris.add(value)
                    elif isinstance(value, list):
                        extract_uris(value)
                    elif isinstance(value, dict):
                        if 'device_id' in value.keys():
                            print (" xx found device_id :",value['device_id'])

                        extract_uris([value])

    extract_uris(data)
    return uris

def main():
    if len(sys.argv) != 3:
        print("Usage: python server_uris.py input_file output_file")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    with open(input_file, 'r') as f:
        data = json.load(f)
    xxx = data["registers"]
    if isinstance(xxx, list):
        print("its a list")
        #print(xxx[1])
        if isinstance(xxx[1], dict):
            print("    its a dict")
            for key, value in xxx[1].items():
                print(key)

        print("ok")

    for key in xxx:
        print(key)
        print(key["device_id"])
        print()

    #create_dir_structure(data)

    uris = list_all_uris(data["registers"])

    with open(output_file, 'w') as f:
        for uri in uris:
            f.write(uri + '\n')

    print("Directory structure created and URIs listed in:", output_file)

if __name__ == "__main__":
    main()
