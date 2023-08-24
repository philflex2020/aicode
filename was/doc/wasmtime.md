Sure! Let's create a more extensive demo of Wasmtime by implementing a WebAssembly module that contains a few functions, and we'll interact with those functions from Python using Wasmtime.

First, let's write a simple WebAssembly text format (WAT) file with some exported functions, and then we'll use the Wasmtime Python API to load and call those functions.

1. **Writing WebAssembly Text File (WAT)**

Create a file called `demo.wat` with the following content:

```wat
(module
  (func $add (param i32 i32) (result i32) (i32.add (local.get 0) (local.get 1)))
  (func $subtract (param i32 i32) (result i32) (i32.sub (local.get 0) (local.get 1)))
  (func $multiply (param i32 i32) (result i32) (i32.mul (local.get 0) (local.get 1)))
  (export "add" (func $add))
  (export "subtract" (func $subtract))
  (export "multiply" (func $multiply))
)
```

This WebAssembly module exports three functions: `add`, `subtract`, and `multiply`. You can compile this file to the binary format using a tool like `wat2wasm`:

```bash
wat2wasm demo.wat
```

It will produce a `demo.wasm` file.

2. **Using Wasmtime from Python**

Next, we'll use the Wasmtime Python API to load the compiled `demo.wasm` and call the exported functions.

```python
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
```

Here, we define three Python functions (`add`, `subtract`, `multiply`) that wrap the corresponding WebAssembly functions, and we print the results of calling them.

This example demonstrates how you can create and interact with a WebAssembly module using Wasmtime, defining and calling multiple functions and bridging between the WebAssembly and Python worlds.