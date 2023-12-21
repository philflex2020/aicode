
import json

data_store = {}

def is_valid_json(test_string):
    try:
        json_object = json.loads(test_string)
        return True
    except json.JSONDecodeError:
        return False

def get_store(uri):
    keys = uri.strip("/").split("/")
    current_level = data_store
    if uri == '/':
        return current_level

    #for key in keys[:-1]:
    for key in keys:
        if key not in current_level:
            current_level[key] = {}
        current_level = current_level[key]
    return current_level

def get_last(uri):
    keys = uri.strip("/").split("/")
    return keys[-1]

def update_data_store(uri, body):
    keys = uri.strip("/").split("/")
    current_level = data_store

    for i, key in enumerate(keys):
        if i == len(keys) - 1:  # If it's the last key
            if is_valid_json(body):
                #print(" body is json")
                new_data = json.loads(body)
                if isinstance(new_data, dict) and isinstance(current_level.get(key, {}), dict):
                    print("Merge dictionaries")
                    current_level[key] = merge_dicts(current_level.get(key, {}), new_data)
                else:
                    # Replace the existing value
                    current_level[key] = new_data
            else:
                print("If body is not a JSON string, just assign the value")
                current_level[key] = body
        else:
            # Navigate or create new nested dictionary
            current_level = current_level.setdefault(key, {})

def merge_dicts(original, new_data):
    for key, value in new_data.items():
        if key in original and isinstance(original[key], dict) and isinstance(value, dict):
            original[key] = merge_dicts(original[key], value)
        else:
            original[key] = value
    return original