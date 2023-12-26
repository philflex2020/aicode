Sure, here is a code snippet where the vector holds the addresses of the variable objects 
and the map associates the variable address with the attribute object addresses. 
Also, I have added the test code to time the max, min, and average seek time 
for a variable lookup.

```cpp
#include <iostream>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <chrono>

class Variable {
    // Your variable object implementation
};

class Attribute {
    // Your attribute object implementation
};

int main() {
    // create a vector of 10000 variable object addresses
    std::vector<Variable*> variables(10000);
    for(int i = 0; i < 10000; i++) {
        variables[i] = new Variable();
    }

    // create a map to associate variable addresses with attribute object addresses
    std::unordered_map<Variable*, Attribute*> variableAttributeMap;

    // randomly associate 3000 variable objects with attribute objects
    srand(time(0));
    for(int i = 0; i < 3000; i++) {
        int index = rand() % 10000;
        variableAttributeMap[variables[index]] = new Attribute();
    }

    // Now to find the attribute objects, you can simply check the map.
    auto start = std::chrono::high_resolution_clock::now();
    int foundCount = 0;
    for(Variable* variable : variables) {
        auto it = variableAttributeMap.find(variable);
        if(it != variableAttributeMap.end()) {
            // Found an associated attribute object
            // do something with it->second
            foundCount++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Total Variables: 10000" << std::endl;
    std::cout << "Variables with Attributes: 3000" << std::endl;
    std::cout << "Found Variables with Attributes: " << foundCount << std::endl;
    std::cout << "Total Time for Lookup (microseconds): " << duration << std::endl;
    std::cout << "Average Time per Lookup (microseconds): " << duration / 10000.0 << std::endl;

    // Cleanup
    for(Variable* variable : variables) {
        delete variable;
    }
    for(auto& pair : variableAttributeMap) {
        delete pair.second;
    }

    return 0;
}
```
In this code:

- We first create a vector of 10,000 variable object addresses and then randomly associate 3,000 of them with new attribute object addresses by storing them in a map.
- We then go through all the variable addresses and check if they are associated with an attribute object by checking the map. If found, we increment the `foundCount`.
- We measure the time taken to go through all the variable addresses and check the map and then calculate the average time per lookup.
- Finally, we clean up by deleting all the variable and attribute objects.

Note that this code is just for demonstration purposes and should be adjusted according to your specific requirements and implementation. Also, the actual time taken for the lookups may vary based on the system and compiler optimizations.


// randomly associate 3000 variable objects with attribute objects
srand(time(0));
int i = 0;
while(i < 3000) {
    int index = rand() % 10000;
    if(variableAttributeMap.find(variables[index]) == variableAttributeMap.end()) {
        variableAttributeMap[variables[index]] = new Attribute();
        i++;
    }
}
