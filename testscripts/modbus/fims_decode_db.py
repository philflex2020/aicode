import sys
import json
from datetime import datetime
import copy
from influxdb import InfluxDBClient

# InfluxDB configuration
INFLUXDB_HOST = "localhost"  # Replace with your InfluxDB host
INFLUXDB_PORT = 8086
INFLUXDB_DATABASE = "fims_db"  # Replace with your desired database name
INFLUXDB_USERNAME = "admin"  # Replace with your InfluxDB username
INFLUXDB_PASSWORD = "admin"  # Replace with your InfluxDB password

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

    return json_objects

def save_varmap_to_influxdb(varmaps_array):
    client = InfluxDBClient(INFLUXDB_HOST, INFLUXDB_PORT, INFLUXDB_USERNAME, INFLUXDB_PASSWORD, INFLUXDB_DATABASE)

    for timestamp_offset, varmap_at_time in varmaps_array.items():
        json_body = [
            {
                "measurement": "fims_measurements",
                "tags": {
                    "timestamp_offset": timestamp_offset
                },
                "fields": varmap_at_time
            }
        ]
        client.write_points(json_body)

def main(input_file, output_file, varmap_file):
    json_objects = decode_input_file(input_file)

    save_varmap_to_file(varmap_file)
    save_varmap_to_influxdb(varmaps_array)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python fims_listen_db.py input_file output_file varmap_file")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    varmap_file = sys.argv[3]

    main(input_file, output_file, varmap_file)
