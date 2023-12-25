
import json
import socket
## This is an attempt to get a remote dict working
from AttrDict import *

class RemDict(dict):
    def __init__(self, *args, **kwargs):
        super(RemDict, self).__init__(*args, **kwargs)
        self.attributes = {}  # Separate dictionary for attributes
        self.socket_fd = None        
        #self.non_serializable_attribute = self.some_non_serializable_value()
        self.data = {2, 4, 6, 8, 10} 

    def some_non_serializable_value(self):
        # Return a value that is not directly serializable by json.dumps
        # For example, a function, file object, etc.
        return open("somefile.txt", "w")  # Example: file object

    def get_data(self, uri="/"):
        """
        Retrieve data from the remote dictionary by sending a 'get' request
        with the URI set to '/'.
        """
        if self.socket_fd == None:
            return None

        try:
            # Create a 'get' request with URI
            get_request = json.dumps({
                "method": "get",
                "uri": uri,
                "body": ""
            })
            # # Create a 'get' request with URI as '/'
            # mystr = f'{"method": "get", "uri": "{uri}", "body":"""}'
            # get_request = json.dumps(mystr)
            ret = self.socket_fd.sendall(get_request.encode('utf-8'))
            #print(f" get request ret {ret}")

            # Receive the response from the remote server
            response = self.socket_fd.recv(1024)
            return response.decode('utf-8')
        except Exception as e:
            print(f"Error in getting data from remdict: {e}")
            return None

    def send_request(self, uri, body):
        """
        Retrieve data from the remote dictionary by sending a 'get' request
        with the URI set to '/'.
        """
        #print(f" set request uri {uri} body [{body}]")
        if self.socket_fd == None:
            return None

        try:
            # Create a 'set' request with URI
            set_request = json.dumps({
                "method": "request",
                "uri": uri,
                "body": body
            })
            # # Create a 'get' request with URI as '/'
            # mystr = f'{"method": "get", "uri": "{uri}", "body":"""}'
            # get_request = json.dumps(mystr)
            ret = self.socket_fd.sendall(set_request.encode('utf-8'))
            print(f" set request ret {ret}")

            # Receive the response from the remote server
            response = self.socket_fd.recv(1024)
            return response.decode('utf-8')
        except Exception as e:
            print(f"Error in getting data from remdict: {e}")
            return None

    def set_data(self, uri, body):
        """
        Retrieve data from the remote dictionary by sending a 'get' request
        with the URI set to '/'.
        """
        #print(f" set request uri {uri} body [{body}]")
        if self.socket_fd == None:
            return None

        try:
            # Create a 'set' request with URI
            set_request = json.dumps({
                "method": "set",
                "uri": uri,
                "body": body
            })
            # # Create a 'get' request with URI as '/'
            # mystr = f'{"method": "get", "uri": "{uri}", "body":"""}'
            # get_request = json.dumps(mystr)
            ret = self.socket_fd.sendall(set_request.encode('utf-8'))
            print(f" set request ret {ret}")

            # Receive the response from the remote server
            response = self.socket_fd.recv(1024)
            return response.decode('utf-8')
        except Exception as e:
            print(f"Error in getting data from remdict: {e}")
            return None

    def show(self):
            print(f"ip : {self.ip} port {self.port} fd:{self.socket_fd}")

    def connect(self, ip, port):
        self.ip = ip
        self.port = port
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((self.ip, self.port))
            self.socket_fd = s
            print(f"ip : {self.ip} port {self.port} fd:{self.socket_fd}")
        except Exception as e:
            print(f"Error in connecting to remote: {e}")
            return None
        

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
        
    def to_rdict(self):
        # Convert to a regular dictionary for JSON serialization
        regular_dict = {k: v.to_rdict() if isinstance(v, RemDict) else v for k, v in self.items()}
        if self.attributes:
            regular_dict["@attributes"] = self.attributes
        return regular_dict

class RemDictEncoder(json.JSONEncoder):

    def _preprocess_rem(self, obj):
        if isinstance(obj, (RemDict)):
            remote_data = obj.get_data()
            try:
                parsed_data = json.loads(remote_data)
            except json.JSONDecodeError:
                parsed_data = None
            return parsed_data

        elif isinstance(obj, (dict, AttrDict)):
            return {self._preprocess_rem(k): self._preprocess_rem(v) for k,v in obj.items()}
        elif isinstance(obj, list):
            return [self._preprocess_rem(i) for i in obj]
        return obj

    def default(self, obj):
        if isinstance(obj, (RemDict, AttrDict)):
            return str(obj)
        return super().default(obj)

    def iterencode(self, obj, **kwargs):  # Modified to accept and ignore additional kwargs
        return super(RemDictEncoder, self).iterencode(self._preprocess_rem(obj), **kwargs)



        


def custom_dumps(obj,  indent=None):
    if isinstance(obj, RemDict) or isinstance(obj, AttrDict):
        return json.dumps(obj.to_dict(), indent=indent)
    return json.dumps(obj, indent=indent)

def custom_adumps(obj, indent=None):
    if isinstance(obj, RemDict):
        return json.dumps(obj.to_rdict(), indent=indent)
    elif isinstance(obj, AttrDict):
        return json.dumps(obj.to_adict(), indent=indent)
    return json.dumps(obj, indent=indent)

def custom_rdumps(obj, indent=None):
    if isinstance(obj, RemDict):
        return json.dumps(obj.to_rdict(), cls=RemDictEncoder, indent=indent)
    elif isinstance(obj, AttrDict):
        return json.dumps(obj.to_adict(), cls=RemDictEncoder, indent=indent)
    return json.dumps(obj, cls=RemDictEncoder, indent=indent)


def set(uri, body, data_store):
    # Split the URI into parts
    parts = uri.strip('/').split('/')
    
    # Navigate through the data_store using the URI
    current_level = data_store
    for i, part in enumerate(parts):
        #print(f" set request part {part}")

        if isinstance(current_level, RemDict):

            # If current level is a RemDict, send the remaining URI and body to it
            remaining_uri = '/'.join(parts[i:])
            print(f" remote set request uri {remaining_uri}")
            return current_level.set_data(remaining_uri, body)

        if i == len(parts) - 1:
            # If we've reached the end of the URI, set the body
            # If it's a dictionary, merge it, otherwise just set it
            if isinstance(body, dict) and isinstance(current_level.get(part, {}), dict):
                current_level[part] = merge_dicts(current_level.get(part, {}), body)
            else:
                current_level[part] = body
        else:
            current_level = current_level.setdefault(part, {})
            
    return data_store  # Optionally return the modified data_store or some status

def merge_dicts(d1, d2):
    for k, v in d2.items():
        if (k in d1 and isinstance(d1[k], dict)
                and isinstance(v, dict)):
            merge_dicts(d1[k], v)
        else:
            d1[k] = v
    return d1



myrequest = {
    "method":"request",
    "uri":"/my/request",
    "body":{
                "name" :"reqname",
                "every": 0.1, 
                "uris": 
                    [ 
                    {"/stuff/obj21/val1":"value1"},
                    {"/stuff/obj22/val2":"value2"},
                    {"/stuff/obj23/val134":"value3"}
                    ]
            }
        }




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
    data_store["mystuff"]["obj1"]["remobj2"] = RemDict()
    #data_store["mystuff"]["obj1"]["remobj2"]["value"] = [ 1,2,3 ]

    data_store["mystuff"]["obj1"]["remobj2"].set_attribute("remval", 1456)

    # Accessing attributes
    thing1_name = data_store["mystuff"]["obj1"]["obj2"]["thing1"].get_attribute("name")
    print(f"Thing1 Name: {thing1_name}")


    data_store["mystuff"]["obj1"]["remobj2"].connect("127.0.0.1",345)

    print(f"Try remobj2 show ")
    data_store["mystuff"]["obj1"]["remobj2"].show()

    print(f"Try RemDictEncoder ")
    serialized_data = json.dumps(data_store, cls=RemDictEncoder, indent=4)
    print(serialized_data)

    print(f"Try custom_adumps")
    serialized_data = custom_adumps(data_store, indent=4)
    print(serialized_data)

    print(f"Try custom_rdumps")
    serialized_data = custom_rdumps(data_store, indent=4)
    print(serialized_data)


    # data_store is your main data structure
    print(f"Try local set  on /mystuff/obj1/obj2")
    set('/mystuff/obj1/obj2', {'new_key': 'new_value'}, data_store)
    serialized_data = json.dumps(data_store, cls=RemDictEncoder, indent=4)
    print(serialized_data)

    print(f"Try remote set  on /mystuff/obj1/obj2")
    set('/mystuff/obj1/remobj2/stuff', {'new_rem_key2': 'new_rem_value2'}, data_store)
    serialized_data = json.dumps(data_store, cls=RemDictEncoder, indent=4)
    print(serialized_data)

    data_store["mystuff"]["obj1"]["remobj2"].send_request("/my/request", myrequest)
