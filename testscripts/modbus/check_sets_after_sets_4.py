import json
import sys

def extract_last_segment(uri):
    return uri.split('/')[-1]

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

# Sort both set and pub data by timestamp
set_items = sorted(set_data.items(), key=lambda x: float(x[0]))
pub_items = sorted(pub_data.items(), key=lambda x: float(x[0]))

# Iterate through the sorted "set" items
for i, (set_time_str, set_value) in enumerate(set_items):
    if set_value['Method'] != 'set':
        continue

    set_time = float(set_time_str)
    set_uri = set_value['Uri']
    set_obj = set_value['Body']

    if not isinstance(set_obj, dict):
        key = extract_last_segment(set_value['Uri'])
        set_obj = {key: set_obj}
        print(f"Warning: 'set' Body at timestamp {set_time} was not a dictionary. changed to {set_obj}.")

    # Check the next one or two "pub" messages with matching URIs
    match_count = 0
    for pub_time_str, pub_value in pub_items:
        pub_time = float(pub_time_str)

        # Skip "pub" messages before the current "set" message
        if pub_time <= set_time:
            continue

        if pub_value['Method'] != 'set':
            continue

        # Check URI match
        if pub_value['Uri'] != set_uri:
            continue

        pub_obj = pub_value['Body']
        pub_uri = pub_value["Uri"]

        #print(f"Note: 'pub' Body at timestamp {pub_time} is {pub_uri} {pub_obj}.")
        if not isinstance(pub_obj, dict):
            key = extract_last_segment(pub_value['Uri'])
            pub_obj = {key: pub_obj}
            print(f"Warning: 'pub' Body at timestamp {pub_time} is not a dictionary. changed to  {pub_obj}.")
            #continue

        match_found = all(pub_obj.get(key) == value for key, value in set_obj.items())
        if match_found:
            print(f"PASS The objects at 'set' timestamp {set_time} with URI {set_uri} are reflected in the 'pub' message at timestamp {pub_time}")
        else:
            print(f"FAIL The objects at 'set' timestamp {set_time} with URI {set_uri} are not reflected in the 'pub' message at timestamp {pub_time}")

        match_count += 1

        # Stop checking after two matches
        if match_count == 2:
            break
