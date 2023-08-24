from wasmtime import Store, Module, Instance, Engine

# Read the wasm file
with open('demo.wasm', 'rb') as f:
    wasm_bytes = f.read()

# Create a Wasmtime engine
engine = Engine()

# Compile the WebAssembly module
module = Module(engine, wasm_bytes)

# Create a Wasmtime store
store = Store(engine)

# Create an instance of the module (with an empty list of imports)
instance = Instance(store, module, [])

# Retrieve the exports, providing the store as an argument
exports = instance.exports(store)

# Define Python functions to call the WebAssembly functions
def add(a, b):
    return exports[0](store, a, b)

def subtract(a, b):
    return exports[1](store, a, b)

def multiply(a, b):
    return exports[2](store, a, b)

# Call the functions and print the results
result_add = add(5, 37)
result_subtract = subtract(42, 5)
result_multiply = multiply(6, 7)

print(f"5 + 37 = {result_add}")        # Outputs 42
print(f"42 - 5 = {result_subtract}")   # Outputs 37
print(f"6 * 7 = {result_multiply}")    # Outputs 42
