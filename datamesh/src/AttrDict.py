

class AttrDict(dict):
    def __init__(self, *args, **kwargs):
        super(AttrDict, self).__init__(*args, **kwargs)
        self.attributes = {}  # Separate dictionary for attributes

    def set_attribute(self, key, value):
        self.attributes[key] = value

    def get_attribute(self, key, default=None):
        return self.attributes.get(key, default)

    def to_dict(self):
        # Convert to a regular dictionary for JSON serialization
        regular_dict = dict(self)
        if self.attributes:
            regular_dict["@attributes"] = self.attributes
        return regular_dict
        
    def to_adict(self):
        # Convert to a regular dictionary for JSON serialization
        regular_dict = {k: v.to_adict() if isinstance(v, AttrDict) else v for k, v in self.items()}
        if self.attributes:
            regular_dict["@attributes"] = self.attributes
        return regular_dict


def custom_dumps(obj,  indent=None):
    if isinstance(obj, AttrDict):
        return json.dumps(obj.to_dict(), indent=indent)
    return json.dumps(obj, indent=indent)

def custom_adumps(obj, indent=None):
    if isinstance(obj, AttrDict):
        return json.dumps(obj.to_adict(), indent=indent)
    return json.dumps(obj, indent=indent)

if __name__ == "__main__":

    data_store = AttrDict({
        "mystuff": AttrDict({
            "obj1": AttrDict({
                "obj2": AttrDict({
                    "thing1": AttrDict()
                })
            })
        })
    })
    # Setting attributes
    data_store["mystuff"]["obj1"]["obj2"]["thing1"].set_attribute("name", "this is thing 1")
    data_store["mystuff"]["obj1"]["obj2"]["thing1"].set_attribute("value", 1234)
    data_store["mystuff"]["obj1"]["obj2"].set_attribute("obj2val", 456)
    #data_store["mystuff"]["obj1"]["obj2"]["thing1"]="this is thing1"

    # Accessing attributes
    thing1_name = data_store["mystuff"]["obj1"]["obj2"]["thing1"].get_attribute("name")
    print(f"Thing1 Name: {thing1_name}")

    # Serializing including attributes
    import json

    serialized_data = custom_dumps(data_store, indent=4)
    print(serialized_data)
    serialized_data = custom_adumps(data_store, indent=4)
    print(serialized_data)
