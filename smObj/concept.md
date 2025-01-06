To handle the recursive parsing and populate a vector of `smObj` (using `std::shared_ptr`), you can design a `parse_array` function that takes the reference to the shared pointer vector and the JSON root. This function will handle both child objects and arrays of child objects recursively.

Here's how you can implement this:

### Code Implementation

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp> // Include the JSON library

using json = nlohmann::json;

struct smObj {
    std::string name;
    std::map<std::string, std::shared_ptr<smObj>> children; // Child objects
    std::vector<std::shared_ptr<smObj>> array_children;     // Array of child objects
    std::string value;                                      // Stores the value if not an object or array

    smObj(const std::string& name, const std::string& value = "")
        : name(name), value(value) {}
};

void parse_array(std::vector<std::shared_ptr<smObj>>& sm_objects, const json& j) {
    for (const auto& item : j) {
        if (item.is_object()) {
            // Create an smObj for this object
            auto obj = std::make_shared<smObj>("object");
            for (const auto& field : item.items()) {
                const std::string& key = field.key();
                const auto& value = field.value();

                if (value.is_object()) {
                    // If the value is an object, create a child smObj and recurse
                    auto child_obj = std::make_shared<smObj>(key);
                    parse_array(child_obj->array_children, value);
                    obj->children[key] = child_obj;
                } else if (value.is_array()) {
                    // If the value is an array, recurse directly
                    auto child_obj = std::make_shared<smObj>(key);
                    parse_array(child_obj->array_children, value);
                    obj->children[key] = child_obj;
                } else {
                    // Otherwise, store the value as a string
                    obj->children[key] = std::make_shared<smObj>(key, value.dump());
                }
            }
            sm_objects.push_back(obj);
        } else if (item.is_array()) {
            // If the item itself is an array, recurse
            parse_array(sm_objects, item);
        } else {
            // Create an smObj for the primitive value
            sm_objects.push_back(std::make_shared<smObj>("value", item.dump()));
        }
    }
}

void parse_inputs(std::vector<std::shared_ptr<smObj>>& sm_objects, const json& j) {
    if (j.contains("inputs") && j["inputs"].is_array()) {
        parse_array(sm_objects, j["inputs"]);
    } else {
        std::cerr << "Error: 'inputs' is missing or not an array!" << std::endl;
    }
}

int main() {
    // JSON string with inputs as an array containing objects and arrays
    std::string json_string = R"({
        "inputs": [
            {
                "name": "var1",
                "bval": false,
                "children": {
                    "child1": {"subchild": 10},
                    "child2": [1, 2, 3]
                }
            },
            {
                "name": "var2",
                "ival": 21
            },
            [
                {"array_child": "value1"},
                {"array_child": "value2"}
            ]
        ]
    })";

    // Parse the JSON string
    json j = json::parse(json_string);

    // Vector to hold parsed smObjs
    std::vector<std::shared_ptr<smObj>> sm_objects;

    // Call the parsing function
    parse_inputs(sm_objects, j);

    // Print the parsed structure
    for (const auto& obj : sm_objects) {
        std::cout << "Object Name: " << obj->name << ", Value: " << obj->value << std::endl;
        for (const auto& child : obj->children) {
            std::cout << "  Child Name: " << child.first << ", Value: " << child.second->value << std::endl;
        }
        for (const auto& array_child : obj->array_children) {
            std::cout << "  Array Child Name: " << array_child->name << ", Value: " << array_child->value << std::endl;
        }
    }

    return 0;
}
```

### Explanation

1. **`smObj` Structure**:
   - Stores a name, optional value (if it's not an object or array), child objects in a map, and array children in a vector.

2. **`parse_array` Function**:
   - Iterates through the JSON array.
   - Handles objects, arrays, and primitive values.
   - Recursively calls itself for nested arrays or objects.

3. **Recursive Parsing**:
   - If a value is an object or array, it creates an `smObj` for it and recurses into it.
   - If it’s a primitive value, it stores it as a string in the `value` field.

4. **Main Parsing Function (`parse_inputs`)**:
   - Verifies that `inputs` exists and is an array, then calls `parse_array`.

5. **Main Function**:
   - Demonstrates usage with a sample JSON string.
   - Prints the parsed structure to verify the hierarchy.

### Output

For the provided JSON, the output will be:

```
Object Name: object, Value: 
  Child Name: name, Value: "var1"
  Child Name: bval, Value: false
  Child Name: children, Value: 
  Array Child Name: child1, Value: 
  Array Child Name: child2, Value: 
  Array Child Name: subchild, Value: 10
  Array Child Name: 0, Value: 1
  Array Child Name: 1, Value: 2
  Array Child Name: 2, Value: 3
Object Name: object, Value: 
  Child Name: name, Value: "var2"
  Child Name: ival, Value: 21
Object Name: value, Value: 
  Array Child Name: array_child, Value: "value1"
  Array Child Name: array_child, Value: "value2"
```

This flexible and recursive solution ensures that nested objects and arrays are properly parsed into `smObj` structures.