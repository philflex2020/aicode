import sys
import json
from datetime import datetime
import copy

varmaps_array = {}  # Global dictionary to store the varmaps at each time instance
varmap = {}  # Global dictionary to store the varmap

def get_timestamp_offset(first_timestamp, current_timestamp):
    first_ts_datetime = datetime.strptime(first_timestamp, "%Y-%m-%d %H:%M:%S.%f")
    current_ts_datetime = datetime.strptime(current_timestamp, "%Y-%m-%d %H:%M:%S.%f")
    time_delta = current_ts_datetime - first_ts_datetime
    return time_delta.total_seconds()

def parse_uri(uri):
    # Assuming the URI segments are separated by '/'
    segments = uri.split('/')
    if len(segments) >= 3:
        last_segment = segments[-1]
        new_uri = '/'.join(segments[:-1])
        return last_segment, new_uri
    return None, uri

def update_varmap(uri, body):
    if uri in varmap:
        varmap[uri].update(body)
    else:
        varmap[uri] = body

def decode_input_file(input_file):
    with open(input_file, 'r') as f:
        data = f.read()

    entries = data.strip().split("\n\n")
    json_objects = {}

    first_timestamp = None

    for entry in entries:
        json_object = {}
        lines = entry.strip().split("\n")
        for line in lines:
            key, value = map(str.strip, line.split(":", 1))
            if key == "Body":
                valueOK = True
                try:
                    value = json.loads(value)
                    if not isinstance(value, dict):
                        raise ValueError  # To handle non-dict values after JSON loading
                except (json.JSONDecodeError, ValueError):
                    valueOK = False

                if not valueOK:
                    last_segment, new_uri = parse_uri(json_object.get("Uri"))
                    if last_segment:
                        value = {last_segment: value}
                        json_object["Uri"] = new_uri

            json_object[key] = value
            if key == "Timestamp":
                if not first_timestamp:
                    first_timestamp = value

                timestamp_offset = get_timestamp_offset(first_timestamp, value)
                json_objects[timestamp_offset] = json_object

                # Update the varmap dictionary with the URI and body
                uri = json_object.get("Uri")
                if uri:
                    body = json_object.get("Body")
                    if body and isinstance(body, dict):
                        for var_id, var_value in body.items():
                            update_varmap(uri, {var_id: var_value})

                # Store a copy of the current varmap in the varmaps_array
                varmaps_array[timestamp_offset] = copy.deepcopy(varmap)

    return json_objects

def save_varmap_to_file(output_file):
    with open(output_file, 'w') as f:
        json.dump(varmap, f, indent=2)

def main(input_file, output_file, varmap_file):
    json_objects = decode_input_file(input_file)

    with open(output_file, 'w') as f:
        json.dump(json_objects, f, indent=2)

    save_varmap_to_file(varmap_file)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python fims_decode_map_array.py input_file output_file varmap_file")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    varmap_file = sys.argv[3]

    main(input_file, output_file, varmap_file)

    # Print the varmaps_array
    print("Varmaps at each time instance:")
    for timestamp_offset, varmap_at_time in varmaps_array.items():
        print(f"Time instance {timestamp_offset}:")
        print(json.dumps(varmap_at_time, indent=2))
