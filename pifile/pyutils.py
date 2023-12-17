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
def update_data_store(uri, body):
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
