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
    #for key in keys[:-1]:
    for key in keys:
        if key not in current_level:
            current_level[key] = {}
        current_level = current_level[key]
    return current_level

def get_last(uri):
    keys = uri.strip("/").split("/")
    return keys[-1]

# Update the data store based on URI
def old_update_data_store(uri, body):
    keys = uri.strip("/").split("/")
    current_level = data_store

    #for key in keys[:-1]:
    for key in keys:
        if key not in current_level:
            current_level[key] = {}
        current_level = current_level[key]

    last_key = keys[-1]
    if last_key in current_level and isinstance(current_level[last_key], dict):
        # If the last key exists and its value is a dictionary, merge the dictionaries
        print(f" merge last key {last_key}")
        print(body)
        for mkey in body:
            print(mkey)
            print(last_key)
            #current_level[last_key].update(body)
            current_level[last_key][mkey] = body[mkey]
    else:
        # Otherwise, just set the value to body
        print(f" set last key {last_key}")
        print(" body type ")
        print(type(body))
        #body may be a json object
        if False and is_valid_json(body):
            json_object = json.loads(body)
            for mkey in json_object:
                print(mkey)
                print(last_key)
                #current_level[last_key].update(body)
                current_level[last_key][mkey] = json_object[mkey]
        else:
            current_level[last_key] = body
    #current_level[keys[-1]] = body

# 1/ uri may already exist , if so navigate to the end of the uri.
# 2/ the body may be a string , number or a full json object
# Update the data store based on URI\

def new_update_data_store(uri, body):
    update_data_store(uri,body)

def update_data_store(uri, body):
    keys = uri.strip("/").split("/")
    current_level = data_store

    for i, key in enumerate(keys):
        if i == len(keys) - 1:  # If it's the last key
            if is_valid_json(body):
                print(" body is json")
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

# Example Usage
#data_store = {}
#new_update_data_store("/mysys/grid/contactors/contactor_1/state3", '{"2value": 3456, "nvalue": {"name": "myname", "val": 4567}}', data_store)
#print(data_store)

def xnew_update_data_store(uri, body):
    keys = uri.strip("/").split("/")
    current_level = data_store

    for i, key in enumerate(keys):
        if i == len(keys) - 1:  # If it's the last key
            if is_valid_json(body):
                new_data = json.loads(body)
                if isinstance(new_data, dict) and isinstance(current_level.get(key, {}), dict):
                    # Merge dictionaries
                    current_level[key] = {**current_level.get(key, {}), **new_data}
                else:
                    # Replace the existing value
                    current_level[key] = new_data
            else:
                # If body is not a JSON string, just assign the value
                current_level[key] = body
        else:
            # Navigate or create new nested dictionary
            current_level = current_level.setdefault(key, {})

def xnew_update_data_store(uri, body):
    keys = uri.strip("/").split("/")
    current_level = data_store
    last_level = None

    last_key = keys[-1]
    #for key in keys[:-1]:
    for key in keys:
        if key not in current_level:
            print(f" creating key {key}")
            current_level[key] = {}
        else:
            print(f" using key {key}")

        last_level = current_level
        current_level = current_level[key]

    if is_valid_json(body):
        print(f" body is json {body}")
        json_object = json.loads(body)
        if isinstance(json_object, dict):
            print(f" body is json dict {body}")
            for mkey in json_object:
                print(f" key  {mkey}")
                if not isinstance(current_level, dict):
                    current_level = {}

                current_level[mkey] = json_object[mkey]
        else:
            print(f" body is not json dict {body}")
            last_level[last_key] = body
    else:
        print(f" body is not json {body}")

    # return
    # if last_key in current_level and isinstance(current_level[last_key], dict):
    #     # If the last key exists and its value is a dictionary, merge the dictionaries
    #     print(f" merge last key {last_key}")
    #     print(body)
    #     for mkey in body:
    #         print(mkey)
    #         print(last_key)
    #         #current_level[last_key].update(body)
    #         current_level[last_key][mkey] = body[mkey]
    # else:
    #     # Otherwise, just set the value to body
    #     print(f" set last key {last_key}")
    #     print(" body type ")
    #     print(type(body))
    #     #body may be a json object
    #     if False and is_valid_json(body):
    #         json_object = json.loads(body)
    #         for mkey in json_object:
    #             print(mkey)
    #             print(last_key)
    #             #current_level[last_key].update(body)
    #             current_level[last_key][mkey] = json_object[mkey]
    #     else:
    #         current_level[last_key] = body
    # #current_level[keys[-1]] = body
