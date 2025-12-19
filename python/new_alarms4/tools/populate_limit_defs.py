import json

def extract_limits_def(input_file, output_file):
    with open(input_file, 'r') as f:
        data = json.load(f)

    alarms = data.get('alarms', [])

    # Collect all unique limits_def keyed by their name
    limits_def_map = {}
    for alarm in alarms:
        limits_def = alarm.get('limit_defs')
        if limits_def:
            name = limits_def.get('name')
            if name and name not in limits_def_map:
                limits_def_map[name] = limits_def

        # Remove 'limits_var' field if present
        if 'limits_var' in alarm:
            del alarm['limits_var']

        # Add 'limits_def' field with the name of the limits definition
        if limits_def and 'name' in limits_def:
            alarm['limits_def'] = limits_def['name']

        # Remove the full 'limit_defs' object from the alarm to avoid duplication
        if 'limit_defs' in alarm:
            del alarm['limit_defs']

    # Add the collected limits_def as a new top-level key
    data['limits_def'] = list(limits_def_map.values())

    with open(output_file, 'w') as f:
        json.dump(data, f, indent=2)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Extract limits_def from alarms and create a separate limits_def section')
    parser.add_argument('input_file', help='Input alarm_definitions.json file')
    parser.add_argument('output_file', help='Output JSON file with separate limits_def section')
    args = parser.parse_args()

    extract_limits_def(args.input_file, args.output_file)
    print(f'Processed {args.input_file} and wrote output to {args.output_file}')