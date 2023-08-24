from wasmer import engine, Store, Module, Instance

# Let's define the store, that holds the engine, that holds the compiler.
store = Store(engine.JIT)  # Feel free to use engine.Native to have AOT.

# Let's write the WebAssembly module as bytes for the sake of the example.
wasm_bytes = (
    b"\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x07\x01\x60\x02\x7f\x7f\x01\x7f\x03\x02\x01\x00\x07\x07\x01\x03\x61\x64\x64\x00\x00\x0a\x09\x01\x07\x00\x20\x00\x20\x01\x6a\x0b"
)

# Let's compile the WebAssembly module.
module = Module(store, wasm_bytes)

# Let's instantiate the WebAssembly module.
instance = Instance(module)

# Let's call the `add` exported function with 5 and 37 as arguments.
result = instance.exports.add(5, 37)

print(result)  # 42!