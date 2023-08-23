import json
import sys

if len(sys.argv) < 2:
    print("Please provide the file name as an argument.")
    sys.exit(1)

file_name = sys.argv[1]

# Read the JSON data from the file
try:
    with open(file_name, 'r') as file:
        data = json.load(file)
except FileNotFoundError:
    print(f"File {file_name} not found.")
    sys.exit(1)
except json.JSONDecodeError:
    print(f"Error decoding JSON in file {file_name}.")
    sys.exit(1)
# # Read the JSON data from the file
# with open('data.json', 'r') as file:
#     data = json.load(file)

# Collect all the objects from the "set" methods
set_objects = {}
for key, value in data.items():
    if value['Method'] == 'set':
        set_objects.update(value['Body'])
# Check that the objects are reflected in the "pub" messages
for key, value in data.items():
    if value['Method'] == 'pub':
        for obj_key, obj_value in set_objects.items():
            # Check if the key exists in the 'pub' Body and the value matches
            if obj_key not in value['Body'] or value['Body'][obj_key] != obj_value:
                print(f"The object {obj_key}: {obj_value} is not reflected in the 'pub' message at timestamp {value['Timestamp']}")
                continue
            print(f"The object {obj_key}: {obj_value} is reflected in the 'pub' message at timestamp {value['Timestamp']}")

# Check that the objects are reflected in the "pub" messages
# for key, value in data.items():
#     if value['Method'] == 'pub':
#         for obj_key, obj_value in set_objects.items():
#             # Check if the key exists in the 'pub' Body and the value matches
#             if obj_key not in value['Body'] or value['Body'][obj_key] != obj_value:
#                 print(f"The object {obj_key}: {obj_value} is not reflected in the 'pub' message")
#                 continue
#             print(f"The object {obj_key}: {obj_value} is reflected in the 'pub' message")
