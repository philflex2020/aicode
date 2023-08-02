import sys
import json
from datetime import datetime

def get_timestamp_offset(first_timestamp, current_timestamp):
    first_ts_datetime = datetime.strptime(first_timestamp, "%Y-%m-%d %H:%M:%S.%f")
    current_ts_datetime = datetime.strptime(current_timestamp, "%Y-%m-%d %H:%M:%S.%f")
    time_delta = current_ts_datetime - first_ts_datetime
    return time_delta.total_seconds()

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
                try:
                    value = json.loads(value)
                except json.JSONDecodeError:
                    pass  # Leave the value as it is (string) if it's not a valid JSON object
            json_object[key] = value
            if key == "Timestamp":
                if not first_timestamp:
                    first_timestamp = value

                timestamp_offset = get_timestamp_offset(first_timestamp, value)
                json_objects[timestamp_offset] = json_object

    return json_objects

def main(input_file, output_file):
    json_objects = decode_input_file(input_file)

    with open(output_file, 'w') as f:
        json.dump(json_objects, f, indent=2)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script_name.py input_file output_file")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    main(input_file, output_file)


# import sys
# import json

# def decode_input_file(input_file):
#     with open(input_file, 'r') as f:
#         data = f.read()

#     entries = data.strip().split("\n\n")
#     json_objects = {}

#     for entry in entries:
#         json_object = {}
#         lines = entry.strip().split("\n")
#         for line in lines:
#             key, value = map(str.strip, line.split(":", 1))
#             if key == "Body":
#                 try:
#                     value = json.loads(value)
#                 except json.JSONDecodeError:
#                     pass  # Leave the value as it is (string) if it's not a valid JSON object
#             json_object[key] = value
#             if key == "Timestamp":
#                 timestamp = value
#                 json_objects[timestamp] = json_object

#     return json_objects

# def main(input_file, output_file):
#     json_objects = decode_input_file(input_file)

#     with open(output_file, 'w') as f:
#         json.dump(json_objects, f, indent=2)

# if __name__ == "__main__":
#     if len(sys.argv) != 3:
#         print("Usage: python script_name.py input_file output_file")
#         sys.exit(1)

#     input_file = sys.argv[1]
#     output_file = sys.argv[2]

#     main(input_file, output_file)

# def decode_input_file(input_file):
#     with open(input_file, 'r') as f:
#         data = f.read()

#     entries = data.strip().split("\n\n")
#     json_objects = []

#     for entry in entries:
#         json_object = {}
#         lines = entry.strip().split("\n")
#         for line in lines:
#             key, value = map(str.strip, line.split(":", 1))
#             if key == "Body":
#                 try:
#                     value = json.loads(value)
#                 except json.JSONDecodeError:
#                     pass  # Leave the value as it is (string) if it's not a valid JSON object
#             json_object[key] = value
#             if key == "Timestamp":
#                 json_objects.append(json_object)
#                 json_object = {}

#     return json_objects

# def main(input_file, output_file):
#     json_objects = decode_input_file(input_file)

#     with open(output_file, 'w') as f:
#         json.dump(json_objects, f, indent=2)

# if __name__ == "__main__":
#     if len(sys.argv) != 3:
#         print("Usage: python script_name.py input_file output_file")
#         sys.exit(1)

#     input_file = sys.argv[1]
#     output_file = sys.argv[2]

#     main(input_file, output_file)

# def decode_input_file(input_file):
#     with open(input_file, 'r') as f:
#         data = f.read()

#     entries = data.strip().split("\n\n")
#     json_objects = []

#     for entry in entries:
#         json_object = {}
#         lines = entry.strip().split("\n")
#         for line in lines:
#             key, value = map(str.strip, line.split(":", 1))
#             json_object[key] = value
#             if key == "Timestamp":
#                 json_objects.append(json_object)
#                 json_object = {}

#     return json_objects

# def main(input_file, output_file):
#     json_objects = decode_input_file(input_file)

#     with open(output_file, 'w') as f:
#         json.dump(json_objects, f, indent=2)

# if __name__ == "__main__":
#     if len(sys.argv) != 3:
#         print("Usage: python script_name.py input_file output_file")
#         sys.exit(1)

#     input_file = sys.argv[1]
#     output_file = sys.argv[2]
#     main(input_file, output_file)
