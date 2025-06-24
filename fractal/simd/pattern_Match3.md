Here’s what you asked for:

---

### ✅ 1. Test Pattern JSON Generator

Save as `generate_patterns.cpp` (optional, but useful for testing):

```cpp
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>

int main() {
    constexpr int SAMPLE_SIZE = 1024;
    constexpr int NUM_PATTERNS = 2;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 100);

    nlohmann::json j;
    for (int p = 0; p < NUM_PATTERNS; ++p) {
        nlohmann::json pat = nlohmann::json::array();
        for (int i = 0; i < SAMPLE_SIZE; ++i) {
            pat.push_back(dist(gen));
        }
        j["patterns"].push_back(pat);
    }

    std::cout << j.dump(2) << std::endl;
    return 0;
}
```

Compile and use like this:

```bash
g++ -std=c++17 -o generate_patterns generate_patterns.cpp
./generate_patterns | nc localhost 9999
```

---

### ✅ 2. Makefile

Save as `Makefile` in your project root:

```makefile
CXX = g++
CXXFLAGS = -O3 -std=c++17 -pthread -march=native

INCLUDES = -I./
LIBS = 

SRC = pattern_match_simd.cpp
OUT = pattern_match

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(OUT) $(SRC) $(LIBS)

generate: generate_patterns.cpp
	$(CXX) -O2 -std=c++17 -o generate_patterns generate_patterns.cpp

clean:
	rm -f $(OUT) generate_patterns
```

---

Let me know if you want to bundle this into a deployable app, or add support for binary data input/output formats too!