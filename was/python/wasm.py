from wasmtime import Store, Module, Instance, Engine

# Define the WebAssembly bytes (replace this with your actual Wasm module)
wasm_bytes = b'\x00\x61\x73\x6D\x01\x00\x00\x00\x01\x07\x01\x60\x02\x7F\x7F\x01\x7F\x03\x02\x01\x00\x07\x07\x01\x03\x61\x64\x64\x00\x00\x0A\x09\x01\x07\x00\x20\x00\x20\x01\x6A\x0B'

# Create a Wasmtime engine and store
engine = Engine()
store = Store(engine)

# Compile the WebAssembly module
module = Module(engine, wasm_bytes)

# Create an instance of the module (with an empty list of imports)
instance = Instance(store, module, [])

# Retrieve the exports, providing the store as an argument
exports = instance.exports(store)

# The `add` function should be the first exported function, so we access it with an index
add_func = exports[0]

# Define the Python function to call the WebAssembly function
def add(a, b):
    return add_func(store, a, b)

# Call the function with arguments 5 and 37
result = add(5, 37)

print(result)  # Outputs 42
