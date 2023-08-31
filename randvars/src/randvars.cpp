#include <iostream>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <chrono>
#include <climits>


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
    int i = 0;
    while(i < 3000) {
        int index = rand() % 10000;
        if(variableAttributeMap.find(variables[index]) == variableAttributeMap.end()) {
            variableAttributeMap[variables[index]] = new Attribute();
            i++;
        }
    }

    long maxTime = 0;
    long minTime = LONG_MAX;
    long totalTime = 0;

    // Now to find the attribute objects, you can simply check the map.
    auto start = std::chrono::high_resolution_clock::now();
    int foundCount = 0;
    for(Variable* variable : variables) {
        auto startv = std::chrono::high_resolution_clock::now();

        auto it = variableAttributeMap.find(variable);
        if(it != variableAttributeMap.end()) {
            // Found an associated attribute object
            // do something with it->second
            foundCount++;
        }
        auto endv = std::chrono::high_resolution_clock::now();
        auto durationv = std::chrono::duration_cast<std::chrono::microseconds>(endv - startv).count();
        maxTime = std::max(maxTime, durationv);
        minTime = std::min(minTime, durationv);
        totalTime += durationv;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Total Variables: 10000" << std::endl;
    std::cout << "Variables with Attributes: 3000" << std::endl;
    std::cout << "Found Variables with Attributes: " << foundCount << std::endl;
    std::cout << "Total Time for Lookup (microseconds): " << duration << std::endl;
    std::cout << "Average Time per Lookup (microseconds): " << duration / 10000.0 << std::endl;
    std::cout << " Individual results" << std::endl;
    
    std::cout << "Max Time for Lookup (nanoseconds): " << maxTime << std::endl;
    std::cout << "Min Time for Lookup (nanoseconds): " << minTime << std::endl;
    std::cout << "Total Time for Lookup (nanoseconds): " << totalTime  << std::endl;
    std::cout << "Average Time per Lookup (nanoseconds): " << totalTime / 10000.0 << std::endl;

    // Cleanup
    for(Variable* variable : variables) {
        delete variable;
    }
    for(auto& pair : variableAttributeMap) {
        delete pair.second;
    }

    return 0;
}