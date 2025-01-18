Here's the C++ code equivalent of your JSON sequence definition. It dynamically constructs a `Sequence` object and its associated `Step` objects, initializing all fields based on the JSON-like definition:

### Converted C++ Code

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Step structure definition
struct Step {
    std::string oper;
    std::string src;
    std::string dest;
    int value = 0;
    int counter = 0;
    int time = 0;

    Step(const std::string& op, const std::string& s, const std::string& d, int v, int c, int t)
        : oper(op), src(s), dest(d), value(v), counter(c), time(t) {}
};

// Sequence structure definition
struct Sequence {
    std::string name;
    int step = 0;
    bool running = false;
    std::vector<Step> steps;

    Sequence(const std::string& seq_name, int start_step, const std::vector<Step>& seq_steps)
        : name(seq_name), step(start_step), steps(seq_steps) {}
};

// Sequence map to store multiple sequences
std::unordered_map<std::string, Sequence> sequence_map;

void setup_sequences() {
    // Define the "cooling_sequence"
    Sequence cooling_sequence(
        "cooling_sequence",
        0, // Starting step
        {
            Step("set", "", "rbits:1", 101, 0, 0),              // Start fan 1
            Step("wait_reset", "", "", 0, 1, 100),              // Wait 100 cycles using local counter 1
            Step("set", "", "rbits:2", 102, 0, 0),              // Start fan 2
            Step("wait_reset", "", "", 0, 1, 100),              // Wait 100 cycles using local counter 1
            Step("set", "bits:40", "rbits:1", 0, 0, 0),         // Stop fan 1
            Step("set", "", "rbits:2", 0, 0, 0)                 // Stop fan 2
        }
    );

    // Add the sequence to the map
    sequence_map["cooling_sequence"] = cooling_sequence;
}

void print_sequence(const Sequence& seq) {
    std::cout << "Sequence Name: " << seq.name << "\n";
    std::cout << "Starting Step: " << seq.step << "\n";
    for (const auto& step : seq.steps) {
        std::cout << "  Step: " << step.oper
                  << ", Src: " << (step.src.empty() ? "None" : step.src)
                  << ", Dest: " << (step.dest.empty() ? "None" : step.dest)
                  << ", Value: " << step.value
                  << ", Counter: " << step.counter
                  << ", Time: " << step.time
                  << "\n";
    }
}

int main() {
    // Initialize sequences
    setup_sequences();

    // Print the "cooling_sequence"
    if (sequence_map.find("cooling_sequence") != sequence_map.end()) {
        print_sequence(sequence_map["cooling_sequence"]);
    } else {
        std::cout << "Sequence 'cooling_sequence' not found.\n";
    }

    return 0;
}
```

---

### Explanation

1. **Step Structure**:
   - Each step contains:
     - `oper`: The operation to perform (`set`, `wait_reset`, etc.).
     - `src`: The source reference (optional).
     - `dest`: The destination reference.
     - `value`: A specific value (e.g., `101` for fan 1).
     - `counter`: The counter index used for `wait_reset`.
     - `time`: The delay for `wait_reset`.

2. **Sequence Structure**:
   - Represents the sequence of operations.
   - Includes:
     - `name`: Name of the sequence.
     - `step`: Starting step index.
     - `steps`: List of `Step` objects.

3. **Sequence Map**:
   - Stores multiple sequences using a string key for easy retrieval.

4. **`setup_sequences()`**:
   - Initializes the `cooling_sequence` and adds it to the `sequence_map`.

5. **`print_sequence()`**:
   - Utility function to display the sequence details for debugging or verification.

---

### Output

When you run the code, it will print the sequence details:

```
Sequence Name: cooling_sequence
Starting Step: 0
  Step: set, Src: None, Dest: rbits:1, Value: 101, Counter: 0, Time: 0
  Step: wait_reset, Src: None, Dest: None, Value: 0, Counter: 1, Time: 100
  Step: set, Src: None, Dest: rbits:2, Value: 102, Counter: 0, Time: 0
  Step: wait_reset, Src: None, Dest: None, Value: 0, Counter: 1, Time: 100
  Step: set, Src: bits:40, Dest: rbits:1, Value: 0, Counter: 0, Time: 0
  Step: set, Src: None, Dest: rbits:2, Value: 0, Counter: 0, Time: 0
```

This design can handle dynamic sequences with a structure similar to your JSON example.

#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <functional>

// Simulated external functions for setting values and delays
void set_value(const std::string& dest, int value) {
    std::cout << "Setting " << dest << " to " << value << std::endl;
}

void wait_for_time(int time_ms) {
    std::cout << "Waiting for " << time_ms << " ms..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
}

// Step structure definition
struct Step {
    std::string oper;      // Operation type ("set", "wait_reset", etc.)
    std::string src;       // Source reference (if applicable)
    std::string dest;      // Destination reference
    int value = 0;         // Value to set (for "set")
    int counter = 0;       // Counter index (for "wait_reset")
    int time = 0;          // Time to wait (for "wait_reset")

    Step(const std::string& op, const std::string& s, const std::string& d, int v, int c, int t)
        : oper(op), src(s), dest(d), value(v), counter(c), time(t) {}
};

// Sequence function implementation
void cooling_sequence() {
    std::vector<Step> steps = {
        Step("set", "", "rbits:1", 101, 0, 0),             // Start fan 1
        Step("wait_reset", "", "", 0, 1, 100),            // Wait 100 cycles using local counter 1
        Step("set", "", "rbits:2", 102, 0, 0),            // Start fan 2
        Step("wait_reset", "", "", 0, 1, 100),            // Wait 100 cycles using local counter 1
        Step("set", "bits:40", "rbits:1", 0, 0, 0),       // Stop fan 1
        Step("set", "", "rbits:2", 0, 0, 0)              // Stop fan 2
    };

    for (const auto& step : steps) {
        if (step.oper == "set") {
            set_value(step.dest, step.value);
        } else if (step.oper == "wait_reset") {
            wait_for_time(step.time);
        } else {
            std::cout << "Unknown operation: " << step.oper << std::endl;
        }
    }
}

int main() {
    // Run the cooling sequence
    cooling_sequence();
    return 0;
}


Here is a structured document defining the **names**, **registers**, and **shared memory (SM) locations** based on the `cooling_sequence` function example. This includes a mapping for Modbus registers and their corresponding SM offsets.

---

# **Cooling Sequence Register and SM Allocation Document**

### **1. Overview**
The cooling sequence defines operations on Modbus registers and shared memory. The `r` prefix indicates rack variables, which are repeated for each rack (up to 20). The SM memory offsets are designed for both system-wide variables (`sbmu`) and rack-specific variables (`rtos`).

---

### **2. Modbus Register Mapping and Shared Memory Allocation**

| **Name**       | **Register Type** | **Register Number** | **Shared Memory Location**           | **Description**            |
|-----------------|-------------------|---------------------|---------------------------------------|----------------------------|
| `rbits:1`      | `rbits`           | `1`                 | `rtos:sm16:1000[#0xFFFF]`            | Fan 1 control bit          |
| `rbits:2`      | `rbits`           | `2`                 | `rtos:sm16:1002[#0xFFFF]`            | Fan 2 control bit          |
| `bits:40`      | `bits`            | `40`                | `sbmu:sm16:320`                      | System-wide bit (Fan stop) |
| `lcl:wait1`    | Local Counter     | -                   | Local to sequence logic (runtime)    | Counter for delays         |

---

### **3. Details of Each Step**

#### **Step 1**
- **Operation:** `set`
- **Destination:** `rbits:1`
- **Value:** `101`
- **Action:** Start fan 1.
- **SM Access:** Writes `101` to `rtos:sm16:1000[#0xFFFF]` for all racks.

#### **Step 2**
- **Operation:** `wait_reset`
- **Counter:** `lcl:wait1`
- **Time:** `100 ms`
- **Action:** Pause sequence for 100 cycles.

#### **Step 3**
- **Operation:** `set`
- **Destination:** `rbits:2`
- **Value:** `102`
- **Action:** Start fan 2.
- **SM Access:** Writes `102` to `rtos:sm16:1002[#0xFFFF]` for all racks.

#### **Step 4**
- **Operation:** `wait_reset`
- **Counter:** `lcl:wait1`
- **Time:** `100 ms`
- **Action:** Pause sequence for 100 cycles.

#### **Step 5**
- **Operation:** `set`
- **Source:** `bits:40`
- **Destination:** `rbits:1`
- **Value:** `0`
- **Action:** Stop fan 1.
- **SM Access:** Clears `rtos:sm16:1000[#0xFFFF]` for all racks.

#### **Step 6**
- **Operation:** `set`
- **Destination:** `rbits:2`
- **Value:** `0`
- **Action:** Stop fan 2.
- **SM Access:** Clears `rtos:sm16:1002[#0xFFFF]` for all racks.

---

### **4. Aggregate View of SM Allocations**

#### **System-Wide (Global) SM Locations**
- **`sbmu:sm16:320`:** System-wide bit for stopping fans.

#### **Rack-Specific SM Locations**
| **Rack Number** | **Fan 1 Control**        | **Fan 2 Control**        |
|------------------|--------------------------|--------------------------|
| Rack 0          | `rtos:sm16:1000[0x1]`    | `rtos:sm16:1002[0x1]`    |
| Rack 1          | `rtos:sm16:1000[0x2]`    | `rtos:sm16:1002[0x2]`    |
| Rack 2          | `rtos:sm16:1000[0x4]`    | `rtos:sm16:1002[0x4]`    |
| ...             | ...                      | ...                      |
| Rack 19         | `rtos:sm16:1000[0x80000]`| `rtos:sm16:1002[0x80000]`|

---

### **5. Notes**
1. **Local Variables:**
   - Local variables such as counters (`lcl:wait1`) exist only during sequence execution.
2. **Rack Masking:**
   - The `[#0xFFFF]` mask enables individual rack selection.
3. **Future Expansion:**
   - Allocate unused SM ranges for additional fan control or system operations.

---

This document provides a clear mapping of Modbus registers and shared memory offsets for the `cooling_sequence`. Let me know if you need additional refinement or expanded examples!