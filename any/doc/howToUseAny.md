Creating an equivalent of Go's `interface{}` in C++ is quite challenging because C++ is a statically typed language, whereas Go's `interface{}` is a way to work around static typing and achieve dynamic typing. However, you can use `std::variant` or `std::any` from C++17 to hold any type of data. `std::variant` is a type-safe union, while `std::any` can hold any type of value.

`std::map<std::string, std::any>` can be used to store arbitrary key-value pairs, where the key is a string, and the value can be of any type.

Here's an example of how you can use `std::map<std::string, std::any>` to store arbitrary data:

```cpp
#include <iostream>
#include <map>
#include <any>
#include <string>

int main() {
    std::map<std::string, std::any> m;

    m["int"] = 10;
    m["double"] = 20.5;
    m["string"] = std::string("hello");

    std::cout << std::any_cast<int>(m["int"]) << std::endl;
    std::cout << std::any_cast<double>(m["double"]) << std::endl;
    std::cout << std::any_cast<std::string>(m["string"]) << std::endl;

    return 0;
}
```

In this example, `m` is a `std::map<std::string, std::any>` that stores an `int`, a `double`, and a `std::string`. The values can be retrieved using `std::any_cast`.

As for parsing JSON with simdjson and storing the result in a `std::map<std::string, std::any>`, simdjson is a high-performance JSON parser, but it does not automatically convert JSON objects to `std::map<std::string, std::any>`. 
You would have to write code to traverse the JSON object and insert the values into the map.

Here's an example of how you can do it:

```cpp
#include <iostream>
#include <map>
#include <any>
#include <simdjson.h>

std::map<std::string, std::any> jsonToMap(simdjson::dom::object obj) {
    std::map<std::string, std::any> m;

    for (auto [key, value] : obj) {
        switch (value.type()) {
            case simdjson::dom::element_type::INT64:
                m[key] = int64_t(value);
                break;
            case simdjson::dom::element_type::UINT64:
                m[key] = uint64_t(value);
                break;
            case simdjson::dom::element_type::DOUBLE:
                m[key] = double(value);
                break;
            case simdjson::dom::element_type::STRING:
                m[key] = std::string(value);
                break;
            case simdjson::dom::element_type::ARRAY:
                // Handle array
                break;
            case simdjson::dom::element_type::OBJECT:
                m[key] = jsonToMap(value);
                break;
            default:
                // Handle other types
                break;
        }
    }

    return m;
}

int main() {
    simdjson::dom::parser parser;
    simdjson::dom::element elem = parser.parse(R"({"key1": "value1", "key2": 123, "key3": {"subkey1": "subvalue1"}})");
    simdjson::dom::object obj = elem.get_object();

    std::map<std::string, std::any> m = jsonToMap(obj);

    std::cout << std::any_cast<std::string>(m["key1"]) << std::endl;
    std::cout << std::any_cast<int>(m["key2"]) << std::endl;

    return 0;
}
```

In this example, the `jsonToMap` function converts a `simdjson::dom::object` to a `std::map<std::string, std::any>`. 
The `main` function parses a JSON string using simdjson, converts the resulting `simdjson::dom::object` to a `std::map<std::string, std::any>`,
 and then prints the values.
 
You can use `std::any::type()` function to get the `std::type_info` of the contained object and then compare it with `typeid` of the 
type you are interested in.

Here is an example:

```cpp
#include <iostream>
#include <any>
#include <typeinfo>

int main() {
    std::any a = 42;
    std::any b = std::string("hello");

    if (a.type() == typeid(int)) {
        std::cout << "a is an int" << std::endl;
    }

    if (b.type() == typeid(std::string)) {
        std::cout << "b is a string" << std::endl;
    }

    return 0;
}
```

In this example, `a.type()` returns the `std::type_info` for `int`, and `b.type()` returns the `std::type_info` for `std::string`. The `typeid` operator is used to get the `std::type_info` for the types being compared.

Note that you should include the header `<typeinfo>` to use `typeid`.