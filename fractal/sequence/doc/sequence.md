
Here’s an updated implementation considering the modifications for **rack variables** (`rinputs`, `rack:sm16`, etc.) and **system-wide operations** (`sum`, `avg`, `any`, `all`, `none`). The changes focus on evaluating condit
ions across multiple racks and appropriately handling destinations.

Here are **additional examples** of triggers, sequences, and operations to showcase the flexibility and extensibility of your state machine design.

---

### **1. Advanced Trigger Examples**

#### **Multiple Conditions Using Compound Triggers**
```json
[
    {
        "src_ref": "bits",
        "src_reg": 8,
        "oper": "reset_run",
        "dest_reg": "cooling_sequence",
        "value": 0
    },
    {
        "oper": "and",
        "conditions": [
            {"src_ref": "rinput", "src_reg": 20, "oper": "gt", "value": 80},
            {"src_ref": "rinput", "src_reg": 25, "oper": "lt", "value": 50}
        ],
        "dest_reg": "cooling_sequence"
    },
    {
        "oper": "or",
        "conditions": [
            {"src_ref": "bits", "src_reg": 10, "value": 1},
            {"src_ref": "bits", "src_reg": 11, "value": 1}
        ],
        "dest_reg": "emergency_shutdown"
    }
]
```

- **Explanation:**
  - The **first trigger** resets `bits[8]` and starts the `cooling_sequence`.
  - The **second trigger** checks if rack input `rinput[20] > 80` **AND** `rinput[25] < 50`.
  - The **third trigger** activates the `emergency_shutdown` sequence if either `bits[10]` **OR** `bits[11]` are set.

---

### **2. Sequence Examples**

#### **Cooling System Sequence**
```json
{
    "sequence_name": "cooling_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 101, "dest_def": "rbits", "dest_reg": 1},  // Start fan 1
        {"oper": "wait_reset", "counter_reg": 0, "time": 100},              // Wait 100 cycles
        {"oper": "set", "ref_reg": 102, "dest_def": "rbits", "dest_reg": 2},  // Start fan 2
        {"oper": "wait_reset", "counter_reg": 0, "time": 100},              // Wait 100 cycles
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 1},   // Stop fan 1
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 2}    // Stop fan 2
    ]
}
```

- **Explanation:**
  - The sequence alternates between starting and stopping fans with a wait period.

#### **Emergency Shutdown Sequence**
```json
{
    "sequence_name": "emergency_shutdown",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 133, "dest_def": "rbits", "dest_reg": 5},  // Send shutdown signal
        {"oper": "wait_reset", "counter_reg": 1, "time": 300},              // Wait for confirmation
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 5}    // Clear shutdown signal
    ]
}
```

---

### **3. Extended Operations**

#### **New Operations**

1. **Arithmetic Operations**:
    - **add**: Add two registers and store the result.
    - **sub**: Subtract one register from another.
    - **mul**: Multiply two registers.
    - **div**: Divide two registers.

```json
{"oper": "add", "src_reg": 20, "ref_reg": 25, "dest_def": "rinput", "dest_reg": 30}
```

2. **Conditional Branching**:
    - Jump to another step based on a condition.

```json
{"oper": "jump_if", "condition": {"reg": 10, "oper": "gt", "value": 100}, "jump_to": 5}
```

3. **State Monitoring**:
    - Monitor specific inputs and act accordingly.

```json
{"oper": "monitor", "src_reg": 40, "condition": {"oper": "lt", "value": 50}, "action": "set_flag", "flag": 1}
```

---

### **4. Comprehensive Example: Charging Control**

#### **Trigger**
```json
{
    "src_ref": "bits",
    "src_reg": 12,
    "oper": "reset_run",
    "dest_reg": "charging_sequence",
    "value": 0
}
```

#### **Sequence**
```json
{
    "sequence_name": "charging_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 0x04, "dest_def": "rbits", "dest_reg": 1},   // Close negative relay
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x06, "dest_def": "rbits", "dest_reg": 2},  // Precharge
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x07, "dest_def": "rbits", "dest_reg": 3},  // Close positive relay
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x85, "dest_def": "rbits", "dest_reg": 4}   // Open precharge, enable
    ]
}
```

#### **Integration into State Machine**
```cpp
void state_machine_loop() {
    while (true) {
        process_triggers(triggers, sequences, bits, inputs);
        execute_sequences(sequences, counters, bits, registers);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
```

---

### **5. Debugging and Monitoring**

Add logging to track execution:
```cpp
void log_sequence_progress(const std::string& sequence_name, int step) {
    std::cout << "Sequence: " << sequence_name << " | Current Step: " << step << std::endl;
}

void log_operation(const json& command) {
    std::cout << "Executing Operation: " << command.dump() << std::endl;
}
```

Call these logs in the sequence execution loop:
```cpp
log_sequence_progress(name, current_step);
log_operation(command);
```

---

### **Next Steps**

1. **Extending the Framework:**
    - Add more operation types like **logical operations** (`and`, `or`, `not`) and **system-level commands**.
2. **Configuration Management:**
    - Use JSON files to load triggers and sequences at runtime.
3. **Performance Monitoring:**
    - Track execution time for each sequence and log critical performance metrics.
4. **Error Handling:**
    - Add error detection for invalid operations, missing registers, or unreachable steps.

Let me know if you'd like further examples or a deeper dive into specific features!


Here are examples of typical sequences, incorporating the concept of `rack_num` for handling operations either at a system level or per rack:

---

### Example 1: Relay Control Sequence
**Description**: Controls relays in a sequence for a specific rack.

```json
{
    "sequence_name": "relay_control",
    "rack_num": -1, 
    "steps": [
        {"oper": "set", "ref": "rbits:4", "value": 1, "description": "Close negative relay"},
        {"oper": "wait_reset", "ref": "counter:0", "value": 200, "description": "Wait 2 seconds"},
        {"oper": "set", "ref": "rbits:6", "value": 1, "description": "Close precharge relay"},
        {"oper": "wait_reset", "ref": "counter:0", "value": 200, "description": "Wait 2 seconds"},
        {"oper": "set", "ref": "rbits:7", "value": 1, "description": "Close positive relay"},
        {"oper": "wait_reset", "ref": "counter:0", "value": 200, "description": "Wait 2 seconds"},
        {"oper": "set", "ref": "rbits:133", "value": 1, "description": "Open precharge relay and enable"}
    ]
}
```

---

### Example 2: Rack Initialization Sequence
**Description**: Initializes rack parameters, such as setting initial charge limits.

```json
{
    "sequence_name": "rack_initialization",
    "rack_num": 3, 
    "steps": [
        {"oper": "set", "ref": "rhold:20", "value": 50, "description": "Set initial charge limit for rack 3"},
        {"oper": "set", "ref": "rhold:21", "value": 100, "description": "Set discharge limit for rack 3"},
        {"oper": "set", "ref": "rinput:40", "value": 1, "description": "Activate rack monitoring"}
    ]
}
```

---

### Example 3: System-Wide Alarm Reset
**Description**: Resets all alarms across all racks.

```json
{
    "sequence_name": "alarm_reset",
    "rack_num": -1,
    "steps": [
        {"oper": "set", "ref": "bits:10", "value": 0, "description": "Clear system-wide alarm bit"},
        {"oper": "set", "ref": "rbits:10", "value": 0, "description": "Clear rack-specific alarm bits for all racks"}
    ]
}
```

---

### Example 4: Energy Discharge Sequence
**Description**: Gradually increases discharge power for a specific rack.

```json
{
    "sequence_name": "discharge_sequence",
    "rack_num": 5,
    "steps": [
        {"oper": "set", "ref": "rhold:50", "value": 10, "description": "Start with low discharge power for rack 5"},
        {"oper": "wait_reset", "ref": "counter:0", "value": 100, "description": "Wait 1 second"},
        {"oper": "set", "ref": "rhold:50", "value": 20, "description": "Increase discharge power for rack 5"},
        {"oper": "wait_reset", "ref": "counter:0", "value": 100, "description": "Wait 1 second"},
        {"oper": "set", "ref": "rhold:50", "value": 30, "description": "Set maximum discharge power for rack 5"}
    ]
}
```

---

### Example 5: Conditional Activation Sequence
**Description**: Activates a feature if specific conditions are met.

```json
{
    "sequence_name": "conditional_activation",
    "rack_num": -1,
    "steps": [
        {
            "oper": "condition",
            "conditions": [
                {"src": "bits:3", "oper": "=", "value": 1},
                {"src": "rinput:10", "oper": ">", "value": 300}
            ],
            "description": "Activate only if bit 3 is set and input 10 is greater than 300"
        },
        {"oper": "set", "ref": "bits:15", "value": 1, "description": "Activate system feature"}
    ]
}
```

---

### Highlights:
1. **`rack_num`**:
   - Use `-1` for system-wide operations.
   - Specify rack numbers for per-rack sequences.
2. **Conditions**:
   - Enable complex triggers using logical operations like `and`/`or` on multiple conditions.
3. **Descriptions**:
   - Document steps for clarity in maintenance and debugging.

Let me know if you’d like further refinements or additional examples!

---

Here’s a reminder of the `start_sequence` function we discussed earlier. This function initializes and begins a named sequence. It typically interacts with a sequence map, looks up the sequence by name, and initializes the sequence execution.

### Code Reminder: `start_sequence`

```cpp
void start_sequence(const std::string& sequence_name, int rack_num = -1) {
    // Check if the sequence exists in the sequence map
    if (sequence_map.find(sequence_name) == sequence_map.end()) {
        std::cerr << "Error: Sequence '" << sequence_name << "' not found." << std::endl;
        return;
    }

    // Retrieve the sequence
    Sequence sequence = sequence_map[sequence_name];

    // Initialize the sequence
    sequence.current_step = 0;
    sequence.running = true;

    // Handle rack-specific logic if needed
    if (rack_num >= 0) {
        sequence.rack_num = rack_num; // Associate with a specific rack
    }

    std::cout << "Starting sequence: " << sequence_name
              << (rack_num >= 0 ? " for rack " + std::to_string(rack_num) : "") << std::endl;

    // Save the updated sequence back into the sequence map
    sequence_map[sequence_name] = sequence;
}
```

---

### Key Features:
1. **Sequence Map Lookup**:
   - Ensures the sequence exists in the `sequence_map`.
2. **Initialization**:
   - Sets the sequence's `current_step` to 0 and marks it as `running`.
3. **Rack Association**:
   - Optionally associates the sequence with a specific rack using `rack_num`.
4. **Error Handling**:
   - Outputs an error message if the sequence does not exist.

---

### Example Usage:

```cpp
// Define a sequence
Sequence relay_sequence = {
    "relay_sequence",
    0,
    false,
    {
        {"set", "bits:4", 1},
        {"wait_reset", "counter:0", 200},
        {"set", "bits:6", 1},
        {"wait_reset", "counter:0", 200}
    }
};

// Add to the sequence map
sequence_map["relay_sequence"] = relay_sequence;

// Start the sequence
start_sequence("relay_sequence");
start_sequence("relay_sequence", 3); // For rack 3
```

---

### Supporting Data Structures:

#### `Sequence` Definition:
```cpp
struct Step {
    std::string oper;      // Operation type (e.g., set, wait_reset)
    std::string ref;       // Reference variable (e.g., bits:4, counter:0)
    int value;             // Value to set or compare
};

struct Sequence {
    std::string name;                  // Sequence name
    int current_step;                  // Current step in the sequence
    bool running;                      // Is the sequence currently running
    std::vector<Step> steps;           // List of steps in the sequence
    int rack_num = -1;                 // Associated rack number (-1 for system-wide)
};
```

#### Sequence Map:
```cpp
std::unordered_map<std::string, Sequence> sequence_map;
```


Let me know if you’d like additional improvements or specific examples for `start_sequence` integration!

1. **Condition Evaluation for Rack Variables:**
   - If the `src` or `dest` is a rack variable (e.g., `rinputs`, `rack:sm16`), iterate over all racks (`rack_num` from `0` to `rack_max`).

2. **System Variable Aggregation:**
   - If the `dest` is a system variable, define operations like `sum`, `avg`, `any`, `all`, and `none` to aggregate results across racks.

3. **Execution Workflow Adjustments:**
   - Add logic to determine whether a variable is a rack variable or a system variable.
   - Use appropriate aggregation logic when evaluating conditions for system variables.

---

### **Modified Functions**

#### **`get_val`: Fetch Value by Rack or System**
This function now supports both system-wide and rack-specific variables.
```cpp
int get_val(const std::string& ref, int rack_num = -1) {
    if (ref.find("rinput") == 0 || ref.find("rack:sm16") == 0) {
        if (rack_num == -1) throw std::invalid_argument("Rack number is required for rack variables.");
        int reg = parse_reg(ref);
        return read_modbus_input(rack_num, reg); // Fetch rack-specific input
    } else if (ref.find("bits") == 0) {
        int reg = parse_reg(ref);
        return read_bits(reg); // Fetch system-wide bit
    } else if (ref.find("sbmu") == 0) {
        return read_sbmu_shared_memory(parse_sbmu_ref(ref)); // Fetch shared memory
    } else if (ref.find("vals") == 0) {
        return global_vars[parse_name(ref)]; // Fetch global variable
    } else if (ref.find("lvals") == 0) {
        return local_vars[parse_name(ref)]; // Fetch local variable
    }
    throw std::invalid_argument("Invalid data reference: " + ref);
}

int set_val(const std::string& ref, uint32_t value, int rack_num = -1) {
    if (ref.find("rinput") == 0 || ref.find("rack:sm16") == 0) {
        if (rack_num == -1) throw std::invalid_argument("Rack number is required for rack variables.");
        int reg = parse_reg(ref);
        return write_rack_modbus_input(rack_num, reg, value); // Fetch rack-specific input
    } else if (ref.find("bits") == 0) {
        int reg = parse_reg(ref);
        return write_modbus_bits(reg, value); // Fetch system-wide bit
    } else if (ref.find("sbmu") == 0) {
        return write_sbmu_shared_memory(parse_sbmu_ref(ref), value); // Fetch shared memory
    } else if (ref.find("vals") == 0) {
        return global_vars[parse_name(ref)] = value; // Fetch global variable
    } else if (ref.find("lvals") == 0) {
        return local_vars[parse_name(ref)] = value; // Fetch local variable
    }
    throw std::invalid_argument("Invalid data reference: " + ref);
}

```


---

#### **`evaluate_trigger`: Evaluate Trigger for Rack or System**
Supports rack iteration and system-wide aggregation.
```cpp
bool evaluate_trigger(const json& trigger, int rack_num = -1) {
    if (trigger.contains("conditions")) {
        std::string oper = trigger["oper"];
        bool result = (oper == "and") ? true : false;

        for (const auto& condition : trigger["conditions"]) {
            bool eval = evaluate_trigger(condition, rack_num);
            if (oper == "and") result &= eval;
            if (oper == "or") result |= eval;
        }
        return result;
    }

    int src_val = get_val(trigger["src"], rack_num);
    int value = get_val(trigger["value"], rack_num);
    std::string oper = trigger["oper"];

    if (oper == "=") return src_val == value;
    if (oper == ">") return src_val > value;
    if (oper == "<") return src_val < value;
    if (oper == "reset_run") {
        if (src_val) {
            set_val(trigger["src"], 0, rack_num);
            return true;
        }
        return false;
    }
    throw std::invalid_argument("Invalid operator: " + oper);
}
```

---

#### **`process_triggers`: Process Triggers with Rack and System Logic**
Handles both rack-specific and system-wide destinations.
```cpp
void process_triggers(const json& triggers) {
    for (const auto& trigger : triggers) {
        if (trigger["src"].find("rinput") == 0 || trigger["src"].find("rack:sm16") == 0) {
            // Evaluate for each rack
            for (int rack_num = 0; rack_num < rack_max; ++rack_num) {
                if (evaluate_trigger(trigger, rack_num)) {
                    std::string sequence = trigger["run"];
                    start_sequence(sequence, rack_num); // Pass rack_num for rack-specific sequences
                }
            }
        } else {
            // Evaluate as system-wide trigger
            if (evaluate_trigger(trigger)) {
                std::string sequence = trigger["run"];
                start_sequence(sequence); // System-wide sequence
            }
        }
    }
}
```

---

#### **Aggregation Logic for System Variables**
Add support for system-wide aggregation modes (`sum`, `avg`, `any`, `all`, `none`).

```cpp
int aggregate_results(const std::string& mode, const std::vector<bool>& results) {
    if (mode == "any") return std::any_of(results.begin(), results.end(), [](bool val) { return val; });
    if (mode == "all") return std::all_of(results.begin(), results.end(), [](bool val) { return val; });
    if (mode == "none") return !std::any_of(results.begin(), results.end(), [](bool val) { return val; });
    if (mode == "sum") return std::count(results.begin(), results.end(), true);
    if (mode == "avg") return std::count(results.begin(), results.end(), true) / static_cast<int>(results.size());
    throw std::invalid_argument("Invalid aggregation mode: " + mode);
}
```

---

#### **System Destination Handling in `process_triggers`**
If a trigger's destination is a system variable, apply aggregation.
'

```cpp
void process_triggers(const json& triggers) {
    for (const auto& trigger : triggers) {
        if (trigger["src"].find("rinput") == 0 || trigger["src"].find("rack:sm16") == 0) {
            std::vector<bool> rack_results;

            for (int rack_num = 0; rack_num < rack_max; ++rack_num) {
                rack_results.push_back(evaluate_trigger(trigger, rack_num));
            }

            if (trigger.contains("dest_mode")) {
                std::string mode = trigger["dest_mode"];
                int aggregated_result = aggregate_results(mode, rack_results);
                set_val(trigger["dest"], aggregated_result); // Write aggregated result to destination
            }
        } else {
            if (evaluate_trigger(trigger)) {
                std::string sequence = trigger["run"];
                start_sequence(sequence); // System-wide sequence
            }
        }
    }
}
```

---

### **Example Triggers with Aggregation**

#### **Rack-Specific Triggers**
```json
{
    "src": "rinput:5","oper": ">","value": 400,"run": "rack_specific_sequence"
}
```

#### **System-Wide Trigger with Aggregation**
```json
{
    "src": "rinput:10","oper": ">","value": 500,"dest": "bits:15",
    "dest_mode": "any", // Aggregates results from all racks
    "run": "system_sequence"
}
```

#### **Compound Trigger**
```json
{
    "src_ref": "bits:3",
    "oper": "and",
    "conditions": [
        {"src": "rinput:3", "oper": ">", "value": 300},
        {"src": "bits:5", "oper": "=", "value": 1}
    ],
    "run": "compound_sequence"
}
```

---

This design ensures that triggers can handle rack-specific data, evaluate conditions across racks, and aggregate results for system-wide operations, providing a robust and scalable solution for complex systems. 



---

### **1. Data Definitions**

The design allows for referencing data from various sources:
- **Rack Modbus:** (`rbits`, `rinput`, `rhold`, etc.)
- **System Modbus:** (`bits`, `input`, etc.)
- **SBMU Shared Memory:** (`sbmu:sm16:320` or `sbmu:sm8:400`)
- **Global Variables:** (`vals:global_var`)
- **Local Variables:** (`lvals:local_var`)

All values can be accessed or set using unified `get_val` and `set_val` functions.

---

### **2. Unified Global and Local Definitions**
- **Global Definitions:** Key-value pairs for commonly used or shared variables.
- **Local Definitions:** Context-specific variables unique to a sequence or trigger.

```json
"global_defs": { "run_relay": "bits:5","high_input1": "rinput:10","global_var1": 456, "sbmu_ref1": "sbmu:sm16:320"},
"local_defs": { "run_local": "bits:5", "high_input2": "rinput:11","local_var1": 456, "sbmu_ref2": "sbmu:sm16:320"}
```

- These definitions standardize and simplify value access, enabling reuse and clear semantics.

---

### **3. Refined Trigger Design**
Triggers monitor conditions and execute sequences based on their evaluation. Each trigger supports:
- **Data Source (`src`):** Specifies the input to monitor.
- **Operator (`oper`):** Defines the operation to evaluate.
- **Value (`value`):** The comparison value or reference.
- **Action (`run`):** The sequence to execute if the condition is met.
- **Complex Conditions:** Allows logical operations (`and`, `or`) with nested triggers.

#### **Example Triggers**
```json
"triggers": [
    { "src": "bits:5","oper": "reset_run","run": "relay_sequence" },
    { "src": "high_input1", "oper": "gt", "value": "local_var1", "run": "high_input_sequence"},
    { "src": "high_input2", "oper": "lt","value": -500,"run": "low_input_sequence"},
    { "src": "sbmu_ref", "oper": "eq", "value": 0, "run": "low_input_sequence" },
    { "src": "bits:3",
        "oper": "and", "conditions": [
                        {"src": "bits:3", "oper": "=", "value": 1},
                         {"src": "bits:4", "oper": "=", "value": 1},
                           {"src": "rinput:4", "oper": ">", "value": 400}
                       ],
                          "run": "compound_sequence"
    }
    ]
```

---

### **4. Execution Workflow**
1. **Trigger Evaluation:**
   - Iterate through the `triggers` list.
   - Use `get_val` to fetch the value from the defined `src` and compare it to the `value` using `oper`.
   - For compound triggers, evaluate all conditions within `conditions` and combine results using `oper`.

2. **Run Sequences:**
   - If a trigger's condition is met, execute the sequence defined in `run`.
   '

---

### **5. Functions for Trigger Handling**

#### **`get_val`: Fetch Value**
```cpp
int get_val(const std::string& ref) {
    // Parse `ref` to identify the data source and address
    if (ref.find("rinput") == 0) {
        int reg = parse_reg(ref);
        return read_modbus_input(reg); // Fetch rack input
    } else if (ref.find("bits") == 0) {
        int reg = parse_reg(ref);
        return read_bits(reg); // Fetch system bit
    } else if (ref.find("sbmu") == 0) {
        return read_sbmu_shared_memory(parse_sbmu_ref(ref)); // Fetch shared memory
    } else if (ref.find("vals") == 0) {
        return global_vars[parse_name(ref)]; // Fetch global variable
    } else if (ref.find("lvals") == 0) {
        return local_vars[parse_name(ref)]; // Fetch local variable
    }
    throw std::invalid_argument("Invalid data reference: " + ref);
}
```

#### **`evaluate_trigger`: Evaluate a Trigger**
```cpp
bool evaluate_trigger(const json& trigger) {
    if (trigger.contains("conditions")) {
        std::string oper = trigger["oper"];
        bool result = (oper == "and") ? true : false;

        for (const auto& condition : trigger["conditions"]) {
            bool eval = evaluate_trigger(condition);
            if (oper == "and") result &= eval;
            if (oper == "or") result |= eval;
        }
        return result;
    }

    int src_val = get_val(trigger["src"]);
    int value = get_val(trigger["value"]);
    std::string oper = trigger["oper"];

    if (oper == "=") return src_val == value;
    if (oper == ">") return src_val > value;
    if (oper == "<") return src_val < value;
    if (oper == "reset_run") {
        if (src_val) {
            set_val(trigger["src"], 0);
            return true;
        }
        return false;
    }
    throw std::invalid_argument("Invalid operator: " + oper);
}
```

#### **`process_triggers`: Main Trigger Loop**
```cpp
void process_triggers(const json& triggers) {
    for (const auto& trigger : triggers) {
        if (evaluate_trigger(trigger)) {
            std::string sequence = trigger["run"];
            start_sequence(sequence); // Start the associated sequence
        }
    }
}
```

---

### **6. Example Runtime Flow**
1. **Define Triggers and Sequences:**
   - Load JSON configuration for triggers and sequences at system startup.

2. **Monitor and Process:**
   - Continuously evaluate triggers in the main loop.

3. **Run Actions:**
   - When a trigger is activated, its associated sequence executes in the state machine.

---

This layout enables a robust and flexible system for monitoring and reacting to conditions in a modular way, ideal for battery management systems or similar applications.




void MainWindow::update_contactor_sequence()
{
    //TODO when writing to DO don't overwrite fault and alarm lights

    switch(contactor_sequence_state)
    {
    case start_open:
        for(int i = 0; contactor_rack>>i > 0 ; i++)
        {
            if(((contactor_rack >> i) & 1) == 1)
            {
                //set rhold 99 3
                QString message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                                  "\",\"reg_type\":\"mb_hold\",\"offset\":99,\"num\":1,\"seq\":" +
                                  QString::number(sequence_num) + ",\"data\":[3]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 99);
                sequence_num++;
                //websocket->send(message);
                //set rhold 101 0
                message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                                  "\",\"reg_type\":\"mb_hold\",\"offset\":101,\"num\":1,\"seq\":" +
                                  QString::number(sequence_num) + ",\"data\":[0]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 101);
                sequence_num++;
                //websocket->send(message);
                //set rhold 99 0
                message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                                  "\",\"reg_type\":\"mb_hold\",\"offset\":99,\"num\":1,\"seq\":" +
                                  QString::number(sequence_num) + ",\"data\":[0]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 99);
                sequence_num++;
                //websocket->send(message);
            }
        }
        contactor_sequence_state = idle;
        break;
    case start_close:
        for(int i = 0; contactor_rack>>i > 0 ; i++)
        {
            if(((contactor_rack >> i) & 1) == 1)
            {
                //set rhold 99 3
                QString message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                                  "\",\"reg_type\":\"mb_hold\",\"offset\":99,\"num\":1,\"seq\":" +
                                  QString::number(sequence_num) + ",\"data\":[3]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 99);
                sequence_num++;
                //websocket->send(message);
            }
        }
        contactor_sequence_state = close_negative;
        contactor_timer->start(2000);  // 2 second timer
        break;
    case close_negative:
        for(int i = 0; contactor_rack>>i > 0 ; i++)
        {
            if(((contactor_rack >> i) & 1) == 1)
            {
                // set rhold 101 4
                QString message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                                  "\",\"reg_type\":\"mb_hold\",\"offset\":101,\"num\":1,\"seq\":" +
                                  QString::number(sequence_num) + ",\"data\":[4]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 101);
                sequence_num++;
                //websocket->send(message);
            }
        }
        contactor_sequence_state = precharge;
        break;
    case precharge:
        for(int i = 0; contactor_rack>>i > 0 ; i++)
        {
            if(((contactor_rack >> i) & 1) == 1)
            {
                // set rhold 101 6
                QString message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                                  "\",\"reg_type\":\"mb_hold\",\"offset\":101,\"num\":1,\"seq\":" +
                                  QString::number(sequence_num) + ",\"data\":[6]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 101);
                sequence_num++;
                //websocket->send(message);
            }
        }
        contactor_sequence_state = close_positive;
        break;
    case close_positive:
        for(int i = 0; contactor_rack>>i > 0 ; i++)
        {
            if(((contactor_rack >> i) & 1) == 1)
            {
                // set rhold 101 7
                QString message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                                  "\",\"reg_type\":\"mb_hold\",\"offset\":101,\"num\":1,\"seq\":" +
                                  QString::number(sequence_num) + ",\"data\":[7]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 101);
                sequence_num++;
                //websocket->send(message);
            }
        }
        contactor_sequence_state = open_precharge;
        break;
    case open_precharge:
        for(int i = 0; contactor_rack>>i > 0 ; i++)
        {
            if(((contactor_rack >> i) & 1) == 1)
            {
                //set rhold 101 133
                QString message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                                  "\",\"reg_type\":\"mb_hold\",\"offset\":101,\"num\":1,\"seq\":" +
                                  QString::number(sequence_num) + ",\"data\":[133]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 101);
                sequence_num++;
                //websocket->send(message);
                // set rhold 99 0
                message = "{\"action\":\"set\",\"sm_name\":\"rtos_" + QString::number(i) +
                          "\",\"reg_type\":\"mb_hold\",\"offset\":99,\"num\":1,\"seq\":" +
                          QString::number(sequence_num) + ",\"data\":[0]}";
                seq_table[sequence_num %HASH_SIZE].set_data(sequence_num, rack_detail, true, i, 99);
                sequence_num++;
                //websocket->send(message);
            }
        }
        contactor_sequence_state = idle;
        contactor_timer->stop();
        break;
    default:
        contactor_timer->stop();
        break;
    };

}



oid MainWindow::on_close_contactors_btn_clicked()
{
    // sequence sends 0x04 to close negative relay
    // wait 2 seconds then send 0x06 to precharge
    // wait 2 seconds then send 0x07 to close positive
    // wait 2 second to send 0x85 to open precharge and turn on enabled
    if(contactor_sequence_state == idle)
    {
        contactor_rack = (mode == rack_detail) ? 1<<(rack_select-1) : 0xFFFF;
        contactor_sequence_state = contactors_close ? start_close : start_open;
        update_contactor_sequence();
    }
}

void MainWindow::on_factory_reset_btn_clicked()
{
    ui->confirm_reset->setVisible(true);
}

void MainWindow::on_confirm_reset_accepted()
{
    QProcess process;
    process.execute("/usr/bin/factory-reset");
}


void MainWindow::on_confirm_reset_rejected()
{
    ui->confirm_reset->setVisible(false);
}


switch(rack_data[i].status)
    {
    case 1:
    case 2:
    case 9:
    case 13:
        ui->power_on_btn->setText("Power On");
        ui->power_on_btn->setEnabled(rack_data[i].rack_online);
        power_btn_on = true;
        break;
    case 3:
    case 8:
        ui->power_on_btn->setText("Power On");
        ui->power_on_btn->setEnabled(false);
        power_btn_on = true;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
    case 10:
    case 11:
        ui->power_on_btn->setText("Power Off");
        ui->power_on_btn->setEnabled(rack_data[i].rack_online);
        power_btn_on = false;
        break;
    case 12:
        ui->power_on_btn->setText("Power Off");
        ui->power_on_btn->setEnabled(false);
        power_btn_on = false;
        break;
    }

// something like this and trigger it when bits 5 is set true , reset bits 5 when we start running 
        {"src_ref":"bits","src_reg": 5, "oper": "reset_run", "dest_reg": "relay_sequence", "value": 0},   //when bits reg 5 is set , reset it and run "relay_sequence" from step 0 

{

    "sequence_name": "relay_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "counter_reg": 0, "value": 1},  // inc step
        {"oper": "set", "ref_reg": 4, "dest_def": "rbits", "dest_reg": 1}, // sets value ( for each rack) inc step
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},             // increments counter reg , resets it to 0 and inc step when it gets to 200 
        {"oper": "set", "ref_reg": 6, "dest_def": "rbits", "dest_reg": 2},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 7, "dest_def": "rbits", "dest_reg": 3},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 133, "dest_def": "rbits", "dest_reg": 4}
    ]
}



Here’s a refined design to handle your **state-triggered sequences**. This builds on the idea of **sequences** that:
1. Trigger when specific conditions are met (e.g., a bit is set).
2. Reset the trigger bit and execute the sequence.
3. Use a counter for timed steps (`wait_reset`) without blocking.

---

### **Key Features**
1. **Triggering a Sequence:**
   - Based on a specific condition (e.g., `bits[5]` is set).
   - Resets the trigger condition (`bits[5] = 0`) before starting.

2. **Sequence Execution:**
   - Runs one step at a time, progressing when conditions like `wait_reset` are met.

3. **Flexible Operations:**
   - Support for `set`, `wait_reset`, and other sequence operations.

4. **Integration into the State Machine:**
   - Each sequence is managed independently.
   - Sequences progress asynchronously.

---

### **Expanded JSON Format**

#### **Sequence Trigger Configuration**
The trigger defines:
- The **condition** (`src_ref`, `src_reg`) to monitor.
- The **action** (`reset_run`) to execute.
- The **target sequence** to execute when the condition is met.

```json
{
    "src_ref": "bits",
    "src_reg": 5,
    "oper": "reset_run",
    "dest_reg": "relay_sequence",
    "value": 0
}
```

#### **Sequence Definition**
The sequence defines:
- The **name** (`sequence_name`) for lookup.
- The **current step** (`step`) to track progress.
- The **steps** array to define operations.

```json
{
    "sequence_name": "relay_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "counter_reg": 0, "value": 1},
        {"oper": "set", "ref_reg": 4, "dest_def": "rbits", "dest_reg": 1},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 6, "dest_def": "rbits", "dest_reg": 2},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 7, "dest_def": "rbits", "dest_reg": 3},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 133, "dest_def": "rbits", "dest_reg": 4}
    ]
}
```

---

### **C++ Implementation**

#### **Trigger Processing**
The trigger resets the condition and starts the sequence:
```cpp
void process_triggers(std::unordered_map<std::string, json>& sequences, std::unordered_map<int, int>& bits) {
    for (const auto& trigger : triggers) {
        if (trigger["oper"] == "reset_run") {
            int src_reg = trigger["src_reg"];
            if (bits[src_reg]) {
                // Reset the bit
                bits[src_reg] = 0;

                // Start the sequence
                const std::string& sequence_name = trigger["dest_reg"];
                sequences[sequence_name]["step"] = 0; // Reset sequence step
            }
        }
    }
}
```

#### **State Machine Execution**
The state machine progresses sequences:
```cpp
void execute_sequences(std::unordered_map<std::string, json>& sequences, std::unordered_map<int, int>& counters, std::unordered_map<int, int>& bits) {
    for (auto& [name, sequence] : sequences) {
        int& current_step = sequence["step"];
        if (current_step >= sequence["steps"].size()) {
            continue; // Sequence complete
        }

        const json& command = sequence["steps"][current_step];
        if (command["oper"] == "set") {
            // Example: Set a value
            int ref_reg = command["ref_reg"];
            int dest_reg = command["dest_reg"];
            bits[dest_reg] = ref_reg;
            current_step++;
        } else if (command["oper"] == "wait_reset") {
            // Example: Wait for a counter
            int counter_reg = command["counter_reg"];
            int time = command["time"];
            counters[counter_reg]++;

            if (counters[counter_reg] >= time) {
                counters[counter_reg] = 0; // Reset counter
                current_step++;
            }
        }
    }
}
```

#### **Main Loop**
Combine triggers and sequences into a single cycle:
```cpp
void state_machine_loop() {
    while (true) {
        process_triggers(sequences, bits);  // Process triggers
        execute_sequences(sequences, counters, bits);  // Execute sequences
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Cycle interval
    }
}
```

---

### **Advantages**
1. **Non-Blocking Waits:**
   - Timed steps using counters avoid blocking the main loop.
   - Multiple sequences can run concurrently.

2. **Dynamic Triggering:**
   - Conditions can trigger sequences dynamically without predefined order.

3. **Flexible Design:**
   - JSON-defined sequences and triggers allow easy modifications and extensions.

---

### **Example Workflow**

1. **Define Trigger:**
   ```json
   {"src_ref":"bits","src_reg":5,"oper":"reset_run","dest_reg":"relay_sequence","value":0}
   ```

2. **Define Sequence:**
   ```json
   {
       "sequence_name": "relay_sequence",
       "step": 0,
       "steps": [
           {"oper": "set", "ref_reg": 4, "dest_def": "rbits", "dest_reg": 1},
           {"oper": "wait_reset", "counter_reg": 0, "time": 200},
           {"oper": "set", "ref_reg": 6, "dest_def": "rbits", "dest_reg": 2},
           {"oper": "wait_reset", "counter_reg": 0, "time": 200},
           {"oper": "set", "ref_reg": 7, "dest_def": "rbits", "dest_reg": 3},
           {"oper": "wait_reset", "counter_reg": 0, "time": 200},
           {"oper": "set", "ref_reg": 133, "dest_def": "rbits", "dest_reg": 4}
       ]
   }
   ```

3. **System Execution:**
   - Triggers monitor conditions and start sequences.
   - State machine executes sequences step-by-step.

---

### **Next Steps**
- Add support for additional operations (`add`, `sub`, `gt`, etc.).
- Introduce logging for debugging and monitoring.
- Expand the trigger system to support compound conditions (e.g., multiple bit checks).

Would you like further elaboration or implementation examples?

Let’s expand on the **trigger system**, **state machine**, and **operations**, providing more robust functionality and examples for your sequence-driven state machine.

---

### **1. Expanded Trigger System**

The trigger system is extended to support **compound conditions** and various trigger types:
- **Single Condition:** Triggers based on a single bit or register.
- **Compound Condition:** Multiple conditions combined using logical operators (AND/OR).

####proposed JSON Format for Triggers**
```json

data can come from rack modbus (rbits, etc ), system modbus(bits), sbmu shared memory 16 0r 8 bit (sbmu:sm16:320)  global data (vals:myglobal_val) sequence data (lvals:local_var1) or be directly defined 
we have rack and sbmu modbus , raxk , rtos and sbmu memory , local vars and global vars
we can use a get/set_val  function with the data definition as an argument also get_set32 to get unit32 vals
{ 
    "global_defs":{"run_relay":"bits:5","high_input1":"rinput:10", "global_var1":456, "sbmu_ref1":"sbmu:sm16:320"},
    "local_defs":{"run_local":"bits:5","high_input2":"rinput:11", "local_var1":456, "sbmu_ref2":"sbmu:sm16:320"},
    "triggers":[
    { "src": "bits:5",   "oper": "reset_run",         "run": "relay_sequence"},
    { "src": "high_input","oper": "gt","value": "local_var1", "run": "high_input_sequence"},
    { "src": "high_input","oper": "lt","value": -500, "run": "low_input_sequence"},
    { "src": "sbmu_ref","oper": "eq","value": 0, "run": "low_input_sequence"},
    { "src_ref": "bits:3", "oper": "and",
        "conditions": [
            {"src": "bits:3", "oper" "=", "value": 1},
            {"src": "bits:4", "oper","=", "value": 1}
            {"src": "rinput:4", "oper",">", "value": 400}
        ],
        "run": "compound_sequence"
    }
    ]
}
```

#### **Trigger Processing Implementation**
```cpp
void process_triggers(
    const std::vector<json>& triggers,
    std::unordered_map<std::string, json>& sequences,
    std::unordered_map<int, int>& bits,
    std::unordered_map<int, int>& inputs) {
    
    for (const auto& trigger : triggers) {
        std::string oper = trigger["oper"];
        
        if (oper == "reset_run") {
            int src_reg = trigger["src_reg"];
            if (bits[src_reg]) {
                bits[src_reg] = 0; // Reset the bit
                const std::string& sequence_name = trigger["dest_reg"];
                sequences[sequence_name]["step"] = 0; // Reset sequence step
            }
        } else if (oper == "gt") {
            int src_reg = trigger["src_reg"];
            int ref_value = trigger["ref_value"];
            if (inputs[src_reg] > ref_value) {
                const std::string& sequence_name = trigger["dest_reg"];
                sequences[sequence_name]["step"] = 0; // Reset sequence step
            }
        } else if (oper == "and") {
            bool all_conditions_met = true;
            for (const auto& condition : trigger["conditions"]) {
                int src_reg = condition["src_reg"];
                int value = condition["value"];
                if (bits[src_reg] != value) {
                    all_conditions_met = false;
                    break;
                }
            }
            if (all_conditions_met) {
                const std::string& sequence_name = trigger["dest_reg"];
                sequences[sequence_name]["step"] = 0; // Reset sequence step
            }
        }
    }
}
```

---

### **2. State Machine Enhancements**

#### **JSON Sequence Definition**
Add more operations to the sequence, such as:
- **Increment/Decrement Registers**
- **Conditional Jumps**

```json
{
    "sequence_name": "relay_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 4, "dest_def": "rbits", "dest_reg": 1},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "inc", "reg": 10, "value": 5},
        {"oper": "jump_if", "condition": {"reg": 10, "oper": "lt", "value": 50}, "jump_to": 1},
        {"oper": "set", "ref_reg": 7, "dest_def": "rbits", "dest_reg": 3},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 133, "dest_def": "rbits", "dest_reg": 4}
    ]
}
```

#### **State Machine Execution**
Include new operations like `jump_if` and `inc`:
```cpp
void execute_sequences(
    std::unordered_map<std::string, json>& sequences,
    std::unordered_map<int, int>& counters,
    std::unordered_map<int, int>& bits,
    std::unordered_map<int, int>& registers) {
    
    for (auto& [name, sequence] : sequences) {
        int& current_step = sequence["step"];
        if (current_step >= sequence["steps"].size()) {
            continue; // Sequence complete
        }

        const json& command = sequence["steps"][current_step];
        std::string oper = command["oper"];

        if (oper == "set") {
            int ref_reg = command["ref_reg"];
            int dest_reg = command["dest_reg"];
            bits[dest_reg] = ref_reg;
            current_step++;
        } else if (oper == "wait_reset") {
            int counter_reg = command["counter_reg"];
            int time = command["time"];
            counters[counter_reg]++;
            if (counters[counter_reg] >= time) {
                counters[counter_reg] = 0;
                current_step++;
            }
        } else if (oper == "inc") {
            int reg = command["reg"];
            int value = command["value"];
            registers[reg] += value;
            current_step++;
        } else if (oper == "jump_if") {
            int reg = command["condition"]["reg"];
            std::string cond_oper = command["condition"]["oper"];
            int value = command["condition"]["value"];
            int jump_to = command["jump_to"];

            bool condition_met = false;
            if (cond_oper == "lt") {
                condition_met = registers[reg] < value;
            } else if (cond_oper == "gt") {
                condition_met = registers[reg] > value;
            } else if (cond_oper == "eq") {
                condition_met = registers[reg] == value;
            }

            if (condition_met) {
                current_step = jump_to;
            } else {
                current_step++;
            }
        }
    }
}
```

---

### **3. Comprehensive Example**

#### **Trigger Configuration**
```json
[
    {"src_ref": "bits", "src_reg": 5, "oper": "reset_run", "dest_reg": "relay_sequence", "value": 0},
    {"src_ref": "rinput", "src_reg": 10, "oper": "gt", "ref_value": 100, "dest_reg": "high_input_sequence"}
]
```

#### **Sequence Definition**
```json
{
    "sequence_name": "relay_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 4, "dest_def": "rbits", "dest_reg": 1},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 6, "dest_def": "rbits", "dest_reg": 2},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 7, "dest_def": "rbits", "dest_reg": 3},
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},
        {"oper": "set", "ref_reg": 133, "dest_def": "rbits", "dest_reg": 4}
    ]
}
```

#### **Main Loop**
```cpp
void state_machine_loop() {
    while (true) {
        process_triggers(triggers, sequences, bits, inputs);  // Process triggers
        execute_sequences(sequences, counters, bits, registers);  // Execute sequences
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Cycle interval
    }
}
```

---

### **Advantages of the Enhanced Design**
1. **Dynamic Triggers:** Ability to define conditions and start sequences dynamically.
2. **Flexible Operations:** Extended set of operations, including jumps and increments.
3. **Asynchronous Execution:** Non-blocking execution of sequences.
4. **Readable Configurations:** JSON-defined sequences and triggers for easy maintenance and updates.

Would you like further examples or assistance integrating this into your system?
Here are **additional examples** of triggers, sequences, and operations to showcase the flexibility and extensibility of your state machine design.

---

### **1. Advanced Trigger Examples**

#### **Multiple Conditions Using Compound Triggers**
```json
[
    {
        "src_ref": "bits",
        "src_reg": 8,
        "oper": "reset_run",
        "dest_reg": "cooling_sequence",
        "value": 0
    },
    {
        "oper": "and",
        "conditions": [
            {"src_ref": "rinput", "src_reg": 20, "oper": "gt", "value": 80},
            {"src_ref": "rinput", "src_reg": 25, "oper": "lt", "value": 50}
        ],
        "dest_reg": "cooling_sequence"
    },
    {
        "oper": "or",
        "conditions": [
            {"src_ref": "bits", "src_reg": 10, "value": 1},
            {"src_ref": "bits", "src_reg": 11, "value": 1}
        ],
        "dest_reg": "emergency_shutdown"
    }
]
```

- **Explanation:**
  - The **first trigger** resets `bits[8]` and starts the `cooling_sequence`.
  - The **second trigger** checks if rack input `rinput[20] > 80` **AND** `rinput[25] < 50`.
  - The **third trigger** activates the `emergency_shutdown` sequence if either `bits[10]` **OR** `bits[11]` are set.

---

### **2. Sequence Examples**

#### **Cooling System Sequence**
```json
{
    "sequence_name": "cooling_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 101, "dest_def": "rbits:1"},  // Start fan 1
        {"oper": "wait_reset", "counter_reg": 0, "time": 100},              // Wait 100 cycles
        {"oper": "set", "ref_reg": 102, "dest_def": "rbits", "dest_reg": 2},  // Start fan 2
        {"oper": "wait_reset", "counter_reg": 0, "time": 100},              // Wait 100 cycles
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 1},   // Stop fan 1
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 2}    // Stop fan 2
    ]
}
```

- **Explanation:**
  - The sequence alternates between starting and stopping fans with a wait period.

#### **Emergency Shutdown Sequence**
```json
{
    "sequence_name": "emergency_shutdown",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 133, "dest_def": "rbits", "dest_reg": 5},  // Send shutdown signal
        {"oper": "wait_reset", "counter_reg": 1, "time": 300},              // Wait for confirmation
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 5}    // Clear shutdown signal
    ]
}
```

---

### **3. Extended Operations**

#### **New Operations**

1. **Arithmetic Operations**:
    - **add**: Add two registers and store the result.
    - **sub**: Subtract one register from another.
    - **mul**: Multiply two registers.
    - **div**: Divide two registers.

```json
{"oper": "add", "src_reg": 20, "ref_reg": 25, "dest_def": "rinput", "dest_reg": 30}
```

2. **Conditional Branching**:
    - Jump to another step based on a condition.

```json
{"oper": "jump_if", "condition": {"reg": 10, "oper": "gt", "value": 100}, "jump_to": 5}
```

3. **State Monitoring**:
    - Monitor specific inputs and act accordingly.

```json
{"oper": "monitor", "src_reg": 40, "condition": {"oper": "lt", "value": 50}, "action": "set_flag", "flag": 1}
```

---

### **4. Comprehensive Example: Charging Control**

#### **Trigger**
```json
{
    "src_ref": "bits",
    "src_reg": 12,
    "oper": "reset_run",
    "dest_reg": "charging_sequence",
    "value": 0
}
```

#### **Sequence**
```json
{
    "sequence_name": "charging_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 0x04, "dest_def": "rbits", "dest_reg": 1},   // Close negative relay
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x06, "dest_def": "rbits", "dest_reg": 2},  // Precharge
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x07, "dest_def": "rbits", "dest_reg": 3},  // Close positive relay
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x85, "dest_def": "rbits", "dest_reg": 4}   // Open precharge, enable
    ]
}
```

#### **Integration into State Machine**
```cpp
void state_machine_loop() {
    while (true) {
        process_triggers(triggers, sequences, bits, inputs);
        execute_sequences(sequences, counters, bits, registers);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
```

---

### **5. Debugging and Monitoring**

Add logging to track execution:
```cpp
void log_sequence_progress(const std::string& sequence_name, int step) {
    std::cout << "Sequence: " << sequence_name << " | Current Step: " << step << std::endl;
}

void log_operation(const json& command) {
    std::cout << "Executing Operation: " << command.dump() << std::endl;
}
```

Call these logs in the sequence execution loop:
```cpp
log_sequence_progress(name, current_step);
log_operation(command);
```

---

### **Next Steps**

1. **Extending the Framework:**
    - Add more operation types like **logical operations** (`and`, `or`, `not`) and **system-level commands**.
2. **Configuration Management:**
    - Use JSON files to load triggers and sequences at runtime.
3. **Performance Monitoring:**
    - Track execution time for each sequence and log critical performance metrics.
4. **Error Handling:**
    - Add error detection for invalid operations, missing registers, or unreachable steps.

Here are **additional examples** of triggers, sequences, and operations to showcase the flexibility and extensibility of your state machine design.

---

### **1. Advanced Trigger Examples**

#### **Multiple Conditions Using Compound Triggers**
```json
[
    {
        "src_ref": "bits",
        "src_reg": 8,
        "oper": "reset_run",
        "dest_reg": "cooling_sequence",
        "value": 0
    },
    {
        "oper": "and",
        "conditions": [
            {"src_ref": "rinput", "src_reg": 20, "oper": "gt", "value": 80},
            {"src_ref": "rinput", "src_reg": 25, "oper": "lt", "value": 50}
        ],
        "dest_reg": "cooling_sequence"
    },
    {
        "oper": "or",
        "conditions": [
            {"src_ref": "bits", "src_reg": 10, "value": 1},
            {"src_ref": "bits", "src_reg": 11, "value": 1}
        ],
        "dest_reg": "emergency_shutdown"
    }
]
```

- **Explanation:**
  - The **first trigger** resets `bits[8]` and starts the `cooling_sequence`.
  - The **second trigger** checks if rack input `rinput[20] > 80` **AND** `rinput[25] < 50`.
  - The **third trigger** activates the `emergency_shutdown` sequence if either `bits[10]` **OR** `bits[11]` are set.

---

### **2. Sequence Examples**

#### **Cooling System Sequence**
```json
{
    "sequence_name": "cooling_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 101, "dest_def": "rbits", "dest_reg": 1},  // Start fan 1
        {"oper": "wait_reset", "counter_reg": 0, "time": 100},              // Wait 100 cycles
        {"oper": "set", "ref_reg": 102, "dest_def": "rbits", "dest_reg": 2},  // Start fan 2
        {"oper": "wait_reset", "counter_reg": 0, "time": 100},              // Wait 100 cycles
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 1},   // Stop fan 1
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 2}    // Stop fan 2
    ]
}
```

- **Explanation:**
  - The sequence alternates between starting and stopping fans with a wait period.

#### **Emergency Shutdown Sequence**
```json
{
    "sequence_name": "emergency_shutdown",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 133, "dest_def": "rbits", "dest_reg": 5},  // Send shutdown signal
        {"oper": "wait_reset", "counter_reg": 1, "time": 300},              // Wait for confirmation
        {"oper": "set", "ref_reg": 0, "dest_def": "rbits", "dest_reg": 5}    // Clear shutdown signal
    ]
}
```

---

### **3. Extended Operations**

#### **New Operations**

1. **Arithmetic Operations**:
    - **add**: Add two registers and store the result.
    - **sub**: Subtract one register from another.
    - **mul**: Multiply two registers.
    - **div**: Divide two registers.

```json
{"oper": "add", "src_reg": 20, "ref_reg": 25, "dest_def": "rinput", "dest_reg": 30}
```

2. **Conditional Branching**:
    - Jump to another step based on a condition.

```json
{"oper": "jump_if", "condition": {"reg": 10, "oper": "gt", "value": 100}, "jump_to": 5}
```

3. **State Monitoring**:
    - Monitor specific inputs and act accordingly.

```json
{"oper": "monitor", "src_reg": 40, "condition": {"oper": "lt", "value": 50}, "action": "set_flag", "flag": 1}
```

---

### **4. Comprehensive Example: Charging Control**

#### **Trigger**
```json
{
    "src_ref": "bits",
    "src_reg": 12,
    "oper": "reset_run",
    "dest_reg": "charging_sequence",
    "value": 0
}
```

#### **Sequence**
```json
{
    "sequence_name": "charging_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "ref_reg": 0x04, "dest_def": "rbits", "dest_reg": 1},   // Close negative relay
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x06, "dest_def": "rbits", "dest_reg": 2},  // Precharge
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x07, "dest_def": "rbits", "dest_reg": 3},  // Close positive relay
        {"oper": "wait_reset", "counter_reg": 0, "time": 200},                 // Wait 2 seconds
        {"oper": "set", "ref_reg": 0x85, "dest_def": "rbits", "dest_reg": 4}   // Open precharge, enable
    ]
}
```

#### **Integration into State Machine**
```cpp
void state_machine_loop() {
    while (true) {
        process_triggers(triggers, sequences, bits, inputs);
        execute_sequences(sequences, counters, bits, registers);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
```

---

### **5. Debugging and Monitoring**

Add logging to track execution:
```cpp
void log_sequence_progress(const std::string& sequence_name, int step) {
    std::cout << "Sequence: " << sequence_name << " | Current Step: " << step << std::endl;
}

void log_operation(const json& command) {
    std::cout << "Executing Operation: " << command.dump() << std::endl;
}
```

Call these logs in the sequence execution loop:
```cpp
log_sequence_progress(name, current_step);
log_operation(command);
```

---

### **Next Steps**

1. **Extending the Framework:**
    - Add more operation types like **logical operations** (`and`, `or`, `not`) and **system-level commands**.
2. **Configuration Management:**
    - Use JSON files to load triggers and sequences at runtime.
3. **Performance Monitoring:**
    - Track execution time for each sequence and log critical performance metrics.
4. **Error Handling:**
    - Add error detection for invalid operations, missing registers, or unreachable steps.

Let me know if you'd like further examples or a deeper dive into specific features!
