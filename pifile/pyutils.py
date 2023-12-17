
data_store = {}

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
        print(body)
        current_level[last_key] = body
    #current_level[keys[-1]] = body
