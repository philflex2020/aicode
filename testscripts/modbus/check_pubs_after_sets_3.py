import json
import sys

if len(sys.argv) < 3:
    print("Please provide the file names for 'set' and 'pub' as arguments.")
    sys.exit(1)

set_file_name = sys.argv[1]
pub_file_name = sys.argv[2]

# Read the JSON data for 'set' from the first file
try:
    with open(set_file_name, 'r') as file:
        set_data = json.load(file)
except FileNotFoundError:
    print(f"File {set_file_name} not found.")
    sys.exit(1)
except json.JSONDecodeError:
    print(f"Error decoding JSON in file {set_file_name}.")
    sys.exit(1)

# Read the JSON data for 'pub' from the second file
try:
    with open(pub_file_name, 'r') as file:
        pub_data = json.load(file)
except FileNotFoundError:
    print(f"File {pub_file_name} not found.")
    sys.exit(1)
except json.JSONDecodeError:
    print(f"Error decoding JSON in file {pub_file_name}.")
    sys.exit(1)

# Collect all the objects and URIs from the "set" methods
set_objects = {float(key): (value['Body'], value['Uri']) for key, value in set_data.items() if value['Method'] == 'set'}

# Sort pub data by timestamp
pub_items = sorted(pub_data.items(), key=lambda x: float(x[0]))

# Check that the objects are reflected in the "pub" messages one or two measurements after the "set" timestamp
for set_time, (set_obj, set_uri) in set_objects.items():
    # Keep track of the number of matched "pub" messages
    match_count = 0

    for pub_time_str, pub_value in pub_items:
        pub_time = float(pub_time_str)

        # If the pub time is less than the set time, continue
        if pub_time <= set_time:
            continue

        # Check if the URIs match
        if pub_value['Uri'] != set_uri:
            continue

        # If there's a match in the next one or two measurements
        if match_count < 2:
            match_found = all(item in pub_value['Body'].items() for item in set_obj.items())

            if match_found:
                print(f"The objects at 'set' timestamp {set_time} with URI {set_uri} are reflected in the 'pub' message at timestamp {pub_time}")
            else:
                print(f"The objects at 'set' timestamp {set_time} with URI {set_uri} are not reflected in the 'pub' message at timestamp {pub_time}")

        match_count += 1

        # Break after checking the next two measurements
        if match_count == 2:
            break
