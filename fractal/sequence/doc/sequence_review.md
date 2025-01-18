# web socket
Creating a simple web page with a text box, a submit button, and functionality to send a request to a WebSocket server while displaying the JSON response as a table is fairly straightforward. Here’s an overview and an implementation plan:

---

### **Steps to Implement**

1. **WebSocket Server Setup**
   - Ensure your WebSocket server is running and listening on a specific port.
   - It should accept JSON requests and send JSON responses.

2. **HTML Frontend**
   - Create a basic HTML page with a text box, submit button, and a placeholder for the table.

3. **JavaScript for WebSocket Communication**
   - Use JavaScript to open a WebSocket connection.
   - On the submit button click, send the request data to the WebSocket server.
   - Process the response to dynamically create and display a table.

4. **CSS for Styling (Optional)**
   - Add CSS to make the page visually appealing.

---

### **Code Example**

#### **HTML File (index.html)**
```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebSocket Test</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
        }
        #responseTable {
            border-collapse: collapse;
            width: 100%;
            margin-top: 20px;
        }
        #responseTable th, #responseTable td {
            border: 1px solid #ddd;
            padding: 8px;
        }
        #responseTable th {
            background-color: #f4f4f4;
        }
    </style>
</head>
<body>
    <h1>WebSocket Test</h1>
    <div>
        <input type="text" id="inputText" placeholder="Enter your query" />
        <button id="submitButton">Submit</button>
    </div>
    <table id="responseTable">
        <thead>
            <tr>
                <th>Key</th>
                <th>Value</th>
            </tr>
        </thead>
        <tbody></tbody>
    </table>

    <script>
        // Connect to the WebSocket server
        const ws = new WebSocket("ws://localhost:8080");

        ws.onopen = () => {
            console.log("Connected to WebSocket server");
        };

        ws.onmessage = (event) => {
            const response = JSON.parse(event.data);
            displayResponseAsTable(response);
        };

        ws.onerror = (error) => {
            console.error("WebSocket error:", error);
        };

        ws.onclose = () => {
            console.log("WebSocket connection closed");
        };

        // Handle button click
        document.getElementById("submitButton").addEventListener("click", () => {
            const query = document.getElementById("inputText").value;
            if (query && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ query }));
            } else {
                alert("WebSocket connection not open or query is empty");
            }
        });

        // Display JSON response as a table
        function displayResponseAsTable(response) {
            const tbody = document.querySelector("#responseTable tbody");
            tbody.innerHTML = ""; // Clear previous table rows

            for (const [key, value] of Object.entries(response)) {
                const row = document.createElement("tr");
                const keyCell = document.createElement("td");
                const valueCell = document.createElement("td");

                keyCell.textContent = key;
                valueCell.textContent = value;

                row.appendChild(keyCell);
                row.appendChild(valueCell);
                tbody.appendChild(row);
            }
        }
    </script>
</body>
</html>
```

---

### **WebSocket Server Example (Node.js)**

#### **server.js**
```javascript
const WebSocket = require("ws");

const server = new WebSocket.Server({ port: 8080 });

server.on("connection", (ws) => {
    console.log("Client connected");

    ws.on("message", (message) => {
        const data = JSON.parse(message);
        console.log("Received query:", data.query);

        // Mock response
        const response = {
            status: "success",
            query: data.query,
            result: "This is a mock result",
            timestamp: new Date().toISOString(),
        };

        ws.send(JSON.stringify(response));
    });

    ws.on("close", () => {
        console.log("Client disconnected");
    });
});

console.log("WebSocket server is running on ws://localhost:8080");
```

---

### **Steps to Test**

1. Run the WebSocket server:
   ```bash
   node server.js
   ```

2. Open the HTML file (`index.html`) in your web browser.

3. Enter a query in the text box and click "Submit."

4. Observe the response rendered as a table below the input box.

---

### **Features You Can Add**
- **Validation:** Validate user input and WebSocket connection state.
- **Styling:** Enhance the table and form with CSS.
- **Error Handling:** Display errors gracefully if the server responds with an error or if the WebSocket connection fails.

This project is simple yet powerful enough to showcase WebSocket communication and dynamic UI updates.



To make the first item in the response a clickable link that sends the corresponding key to the WebSocket server when clicked, you can modify the `displayResponseAsTable` function in your JavaScript code.

Here’s the updated example:

---

### **Updated JavaScript Function**

```javascript
// Display JSON response as a table with the first item as a link
function displayResponseAsTable(response) {
    const tbody = document.querySelector("#responseTable tbody");
    tbody.innerHTML = ""; // Clear previous table rows

    let isFirst = true;

    for (const [key, value] of Object.entries(response)) {
        const row = document.createElement("tr");
        const keyCell = document.createElement("td");
        const valueCell = document.createElement("td");

        if (isFirst) {
            // Create a clickable link for the first key
            const link = document.createElement("a");
            link.textContent = key;
            link.href = "#";
            link.addEventListener("click", (e) => {
                e.preventDefault();
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({ query: key }));
                    console.log(`Sent key: ${key} to WebSocket`);
                } else {
                    alert("WebSocket connection not open");
                }
            });
            keyCell.appendChild(link);
            isFirst = false;
        } else {
            keyCell.textContent = key;
        }

        valueCell.textContent = value;

        row.appendChild(keyCell);
        row.appendChild(valueCell);
        tbody.appendChild(row);
    }
}
```

---

### **Explanation**

1. **Clickable Link:**
   - The first key in the JSON response is wrapped in an `<a>` tag.
   - The `click` event listener sends the key as a query to the WebSocket server.

2. **Key Handling:**
   - When the link is clicked, it prevents the default navigation behavior using `e.preventDefault()`.
   - Sends the clicked key to the WebSocket server.

3. **Preserve Original Table Structure:**
   - Other keys and values are added to the table normally.

---

### **Modified `index.html` (for clarity)**
```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebSocket Test</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
        }
        #responseTable {
            border-collapse: collapse;
            width: 100%;
            margin-top: 20px;
        }
        #responseTable th, #responseTable td {
            border: 1px solid #ddd;
            padding: 8px;
        }
        #responseTable th {
            background-color: #f4f4f4;
        }
    </style>
</head>
<body>
    <h1>WebSocket Test</h1>
    <div>
        <input type="text" id="inputText" placeholder="Enter your query" />
        <button id="submitButton">Submit</button>
    </div>
    <table id="responseTable">
        <thead>
            <tr>
                <th>Key</th>
                <th>Value</th>
            </tr>
        </thead>
        <tbody></tbody>
    </table>

    <script>
        // Connect to the WebSocket server
        const ws = new WebSocket("ws://localhost:8080");

        ws.onopen = () => {
            console.log("Connected to WebSocket server");
        };

        ws.onmessage = (event) => {
            const response = JSON.parse(event.data);
            displayResponseAsTable(response);
        };

        ws.onerror = (error) => {
            console.error("WebSocket error:", error);
        };

        ws.onclose = () => {
            console.log("WebSocket connection closed");
        };

        // Handle button click
        document.getElementById("submitButton").addEventListener("click", () => {
            const query = document.getElementById("inputText").value;
            if (query && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ query }));
            } else {
                alert("WebSocket connection not open or query is empty");
            }
        });

        // Updated function to display response as a table
        function displayResponseAsTable(response) {
            const tbody = document.querySelector("#responseTable tbody");
            tbody.innerHTML = ""; // Clear previous table rows

            let isFirst = true;

            for (const [key, value] of Object.entries(response)) {
                const row = document.createElement("tr");
                const keyCell = document.createElement("td");
                const valueCell = document.createElement("td");

                if (isFirst) {
                    // Create a clickable link for the first key
                    const link = document.createElement("a");
                    link.textContent = key;
                    link.href = "#";
                    link.addEventListener("click", (e) => {
                        e.preventDefault();
                        if (ws.readyState === WebSocket.OPEN) {
                            ws.send(JSON.stringify({ query: key }));
                            console.log(`Sent key: ${key} to WebSocket`);
                        } else {
                            alert("WebSocket connection not open");
                        }
                    });
                    keyCell.appendChild(link);
                    isFirst = false;
                } else {
                    keyCell.textContent = key;
                }

                valueCell.textContent = value;

                row.appendChild(keyCell);
                row.appendChild(valueCell);
                tbody.appendChild(row);
            }
        }
    </script>
</body>
</html>
```

---

### **Behavior**
- The first item in the response becomes a clickable link.
- Clicking the link sends the key to the WebSocket server as a new query.
- Other keys and values are displayed as plain text in the table.

# Sequence Controller

p wilshire 01/14/2025


The system has a robust framework for accessing and managing variables within a system supporting multiple memory types, including Modbus registers, shared memory for racks, RTOS memory, and local/global variables.


---

### **Strengths:**

1. **Unified Variable Access:**
   - The use of `VarDef` and `get_var`/`set_var` functions provides a unified interface for accessing different types of variables (Modbus, shared memory, local, and global).

2. **Flexibility:**
   - Supports various memory types (`VAR_RACK_MEM`, `VAR_MODBUS_INPUT`, etc.).
   - Handles both global and local variables dynamically with indexing.

3. **Error Handling:**
   - Throws runtime exceptions for invalid operations, like missing rack numbers or unsupported variable types.
   - Includes mechanisms to handle undefined local/global variables by initializing them on-the-fly.

4. **Expandability:**
   - Easily extensible for new variable types or additional memory access mechanisms.

---

### **Suggestions for Improvement:**

1. **Error Messages:**
   - Improve the clarity of error messages to include context (e.g., the variable reference or type causing the issue).

2. **Functionality for `RTOS_MEM`:**
   - Add implementation for `RTOS_MEM` access or remove it if unused. A placeholder like `read_rtos_mem` or `write_rtos_mem` is present but commented out.

3. **Optimize Storage:**
   - Use `std::vector` or other dynamic structures instead of raw arrays (`uint16_t shm_rack_mem[MAX_RACK][4096]`), as they provide better bounds checking and are resizeable.

4. **Initialization of `global_var_vec` and `local_var_vec`:**
   - Ensure these vectors are pre-sized or dynamically resized when accessing new variables to prevent runtime errors.

5. **Modbus Access Abstraction:**
   - The `get_var_mb` and `set_var_mb` functions should perform error checking to ensure valid offsets and avoid undefined behavior.

6. **Enums for Readability:**
   - Use enumerations instead of integers for Modbus function codes (`MB_NONE`, `MB_BITS`, etc.) to enhance code readability.

7. **Thread Safety:**
   - If the system is multi-threaded, ensure thread safety for shared resources like `global_var_vec` and `local_var_vec`.

---

### **Documentation Suggestions:**

1. **Function Comments:**
   - Add brief comments above functions explaining their purpose and usage.

2. **Variable Type Enum (`VarType`):**
   - Include a description of each enum value (`VAR_MODBUS_INPUT`, `VAR_LOCAL_VAR`, etc.).

3. **Example Usage:**
   - Provide examples of how to use `decode_var_type`, `get_var`, and `set_var` for different variable types.

---

### **Potential Enhancements:**

1. **JSON Configuration for Global and Local Variables:**
   - Allow defining initial global and local variable values through JSON configuration.

2. **Performance Improvements:**
   - Optimize Modbus and shared memory access by caching frequently used data.

3. **Unit Tests:**
   - Add unit tests for each function to ensure reliability and catch regressions.

4. **Debug Logging:**
   - Add logging for variable access to debug and monitor operations.

5. **Aggregation Support:**
   - Extend `get_var` to support aggregation operations like `SUM`, `AVG`, etc., for rack-specific variables.

---

### Example Additions:

#### **Improved Error Message in `get_var`**
```cpp
default:
    throw std::runtime_error("Invalid variable type for reference: " + var_def.name);
```

#### **Example Usage**
```cpp
void example_usage() {
    // Define a variable
    auto var_def = decode_var_type("rinput:10");

    // Access variable value
    int value = get_var<int>(var_def, 0);

    // Set variable value
    set_var(var_def, value + 5, 0);

    std::cout << "Updated value: " << get_var<int>(var_def, 0) << std::endl;
}
```

This structure is well-suited for your project's requirements, and with the suggested improvements, it can become even more robust and maintainable.

This code implements a **state machine and sequence management system** for handling control logic using predefined sequences, triggers, and operations. 
Below is a **detailed review and documentation** for clarity and organization.

---

## **Code Overview**
This system is designed to:
1. Define and manage **sequences** of operations (`set`, `wait_reset`, `inc`, etc.).
2. Support **triggers** that evaluate conditions and execute sequences.
3. Handle operations across multiple racks using **shared memory** and **Modbus registers**.

---

## **Key Components**
### **1. Step Structure**
Represents a single operation in a sequence.

```cpp
struct Step {
    std::string oper;       // Operation type (e.g., "set", "jump_if", "wait_reset").
    std::string src;        // Source reference (e.g., "bits:4").
    std::string dest;       // Destination reference (e.g., "rbits:5").
    std::string var;        // Variable for arithmetic operations.
    std::string cond;       // Condition for "jump_if".
    std::string counter;    // Counter reference for "wait_reset".
    int value = 0;          // Value to set or compare.
    int jump_to = -1;       // Target step index for jump operations.
    int time = 0;           // Time in milliseconds for "wait_reset".
};
```

### **2. Sequence Structure**
Defines a sequence of steps and tracks its execution state.

```cpp
struct Sequence {
    std::string name;                            // Sequence name.
    std::map<int, int> current_steps;            // Current step per rack.
    std::map<int, bool> running_states;          // Running state per rack.
    std::map<int, std::map<int, int>> counters;  // Counters per rack and step.
    std::map<int, std::map<std::string, int>> sq_vals; // State variables per rack.
    std::vector<Step> steps;                     // List of steps in the sequence.
};
```

### **3. Triggers**
- **Triggers** evaluate conditions and start sequences.
- Conditions can be evaluated per rack or system-wide.
- Example JSON:
    ```json
    {
        "trigger_name": "cooling_trigger",
        "src_ref": "rinput:3",
        "oper": ">",
        "value": 300,
        "run": "cooling_sequence"
    }
    ```

### **4. Sequence Operations**
Implemented in `run_step`:
- **set**: Write a value to a destination.
- **inc**: Increment a value.
- **add**: Add a value from a variable.
- **wait_reset**: Wait until a counter reaches a threshold.
- **jump_if**: Jump to a specific step if a condition is met.

---

## **Sequence Example**
### Input JSON
```json
{
    "sequence_name": "cooling_sequence",
    "step": 0,
    "steps": [
        {"oper": "set", "value": 101, "dest": "rbits:1"},
        {"oper": "wait_reset", "counter": "wait1", "time": 100},
        {"oper": "set", "value": 102, "dest": "rbits:2"},
        {"oper": "wait_reset", "counter": "wait2", "time": 100},
        {"oper": "set", "value": 0, "dest": "rbits:1"}
    ]
}
```

### Converted to Sequence:
```cpp
Sequence cooling_sequence;
cooling_sequence.name = "cooling_sequence";
cooling_sequence.steps.push_back(Step("set", "", "rbits:1", 101));
cooling_sequence.steps.push_back(Step("wait_reset", "wait1", 100));
cooling_sequence.steps.push_back(Step("set", "", "rbits:2", 102));
cooling_sequence.steps.push_back(Step("wait_reset", "wait2", 100));
cooling_sequence.steps.push_back(Step("set", "", "rbits:1", 0));
```

---

## **Modbus and Shared Memory**
### Dynamic Registers
 These are used for simulation but Better Modbus Plans are in the works.
- **Global Modbus Registers**: `mb_regs`
    ```cpp
    std::map<int, std::map<int, int>> mb_regs; // type -> reg -> value
    ```
- **Rack-Specific Modbus Registers**: `mb_rack_regs`
    ```cpp
    std::map<int, std::map<int, std::map<int, int>>> mb_rack_regs; // rack -> type -> reg -> value
    ```
- **Shared Memory**: `sm_regs` and `sm_rack_regs`
    ```cpp
    std::map<std::string, std::map<int, int>> sm_regs; // name -> offset -> value
    std::map<int, std::map<std::string, std::map<int, int>>> sm_rack_regs; // rack -> name -> offset -> value
    ```

---

## **Sequence Execution**
### 1. Start a Sequence
```cpp
void start_sequence(const std::string& sequence_name, int rack_num = -1);
```
- Initializes the sequence.
- Tracks the current step for each rack.

### 2. Execute a Step
```cpp
int run_step(Sequence &sequence, Step &step, const std::string &name, int &current_step, int rack_num);
```
- Executes one step based on its `oper`.

### 3. Run All Sequences
```cpp
void run_all_sequences(std::vector<std::string>& seq_list, int rack_num);
```
- Iterates through all sequences for the specified rack.

---

## **Improvements**
1. **Validation**: Add validation for sequence names, step configurations, and Modbus references.
2. **Error Handling**: Graceful handling for invalid or undefined operations.
3. **Optimization**: Reduce redundant operations in `run_step` and `evaluate_trigger`.
4. **Logging**: Extend logging for sequence and trigger evaluations.

---

Here are some useful **sequences** and **triggers** tailored for a Battery Management System (BMS). These examples include actions like managing contactors, calculating current and power, and handling safety features.

---

## **Sequences**

### **1. Close Contactors**
Used to engage the battery pack by closing positive and negative contactors.

#### JSON Representation
```json
{
    "sequence_name": "close_contactors",
    "step": 0,
    "steps": [
        {"oper": "set", "value": 1, "dest": "rbits:1", "desc": "Close negative contactor"},
        {"oper": "wait_reset", "counter": "wait1", "time": 200, "desc": "Wait 200ms"},
        {"oper": "set", "value": 1, "dest": "rbits:2", "desc": "Close precharge relay"},
        {"oper": "wait_reset", "counter": "wait2", "time": 200, "desc": "Wait 200ms"},
        {"oper": "set", "value": 1, "dest": "rbits:3", "desc": "Close positive contactor"},
        {"oper": "set", "value": 0, "dest": "rbits:2", "desc": "Open precharge relay"}
    ]
}
```

---

### **2. Open Contactors**
Used to safely disconnect the battery pack by opening contactors.

#### JSON Representation
```json
{
    "sequence_name": "open_contactors",
    "step": 0,
    "steps": [
        {"oper": "set", "value": 0, "dest": "rbits:3", "desc": "Open positive contactor"},
        {"oper": "wait_reset", "counter": "wait1", "time": 200, "desc": "Wait 200ms"},
        {"oper": "set", "value": 0, "dest": "rbits:1", "desc": "Open negative contactor"}
    ]
}
```

---

### **3. Calculate Current and Power**
Calculates total current and power from individual rack measurements.

Uses "entry" when the sequence is started
Uses "exit" when the sequence completes 

it runs for all the racks in service, we can use a trigger for that

```c code
if (rack_data[i].rack_online)
{
            num_racks_online++;
            online_mask |= 1<<i;
            rack_data[i].chargeable_energy =  (static_cast<float>(rack_data[i].single_charge_cap) / 10) * (1 - static_cast<float>(rack_data[i].soc) / 1000);
            rack_data[i].dischargeable_energy = (static_cast<float>(rack_data[i].single_discharge_cap) / 10) * (static_cast<float>(rack_data[i].soc) / 1000);
            rack_data[i].voltage_diff = rack_data[i].max_voltage - rack_data[i].min_voltage;
            rack_data[i].temp_diff = rack_data[i].max_temp - rack_data[i].min_temp;

            if((rack_data[i].rack_contactors & 0x25) == 0x25)
            {
                num_racks_closed++;
                total_racks_voltage += rack_data[i].rack_voltage;
            }

            min_charge_energy = (rack_data[i].chargeable_energy < min_charge_energy) ? rack_data[i].chargeable_energy : min_charge_energy;
            min_discharge_energy = (rack_data[i].dischargeable_energy < min_discharge_energy) ? rack_data[i].dischargeable_energy : min_discharge_energy;
            min_charge_power = (rack_data[i].chargeable_power < min_charge_power) ? rack_data[i].chargeable_power : min_charge_power;
            min_discharge_power = (rack_data[i].dischargeable_power < min_discharge_power) ? rack_data[i].dischargeable_power : min_discharge_power;
}
```
#### JSON Representation  

notes , need tpo add entry and exit
check cond rack_online relative jump +-  
add max min
sort out g_designation
use names for vars 
add cond to sets, add, etx


```json
{
    "sequence_name": "calculate_power",
    "step": 0,
    "entry": [
      {"oper": "set",  "dest": "g_total_current", "var":0,  "desc": "init global total_current"},
      {"oper": "set",  "dest": "g_total_volts", "var":0,    "desc": "init global total volts"}
      {"oper": "set",  "dest": "g_max_volts", "var":0,    "desc": "init global max volts"}
      {"oper": "set",  "dest": "g_min_volts", "var":66666,    "desc": "init global min volts"}
    ],
    "step": [
        {"oper": "jump_ifnot", "cond": "rack_online", "jump": 100,    "desc": "skip if rack is not online"},
        {"oper": "max", "src": "rinput:1", "dest": "g_max_voltage",     "desc": "Calculate max voltage"},
        {"oper": "min", "src": "rinput:1", "dest": "g_min_voltage",     "desc": "Calculate min voltage"},
        {"oper": "3sum_max", "src": "rinput:1", "var": "rhold:2", "dest":"bits:12",    "desc": "Set level 1,2,3 max_alarm"},
        {"oper": "3sum_min", "src": "rinput:1", "var": "rhold:5", "dest":"bits:15",    "desc": "Set level 1,2,3 min_alarm"},
        {"oper": "set", "cond":">","src": "rinput:1", "var": "rhold:3", "dest":"bits:13",    "desc": "Set level 2 max_alarm"},
        {"oper": "set", "cond":">","src": "rinput:1", "var": "rhold:4", "dest":"bits:13",    "desc": "Set level 3 max_alarm"},
        {"oper": "set", "cond":"<","src": "rinput:1", "var": "rhold:5", "dest":"bits:14",    "desc": "Set level 1 min_alarm"},
        {"oper": "set", "cond":"<","src": "rinput:1", "var": "rhold:6", "dest":"bits:15",    "desc": "Set level 2 min_alarm"},
        {"oper": "set", "cond":"<","src": "rinput:1", "var": "rhold:7", "dest":"bits:16",    "desc": "Set level 3 min_alarm"},
        {"oper": "max", "src": "rinput:2", "dest": "g_max_current",     "desc": "Calculate max current"},
        {"oper": "min", "src": "rinput:2", "dest": "g_min_current",     "desc": "Calculate min current"},
        {"oper": "vadd", "var": 1,          "dest": "g_num_racks",       "desc": "Calculate num racks"},
        {"oper": "add", "src": "rinput:1",    "dest": "g_total_volts", "desc": "Add  volts for all racks"},
        {"oper": "add", "src": "rinput:2",   "dest": "g_total_power", "desc": "Get total volts"}
    ],, 
    "exit": [
        {"oper": "div", "src": "g_total_volts", "var":"g_num_racks","dest": "g_avg_volts", "desc": "get avg volts"},
        {"oper": "mul", "src": "g_avg_volts", "var":"g_total_current","dest": "g_total_power", "desc": "get  total power"},
        {"oper": "set", "src": "g_total_power","dest": "input:20", "desc": "set  total power mb outout"}

    ],
}
```


Yes, I remember the `Step` object from your sequence implementation. Here’s a quick recap of its definition and purpose:

---

### **`Step` Object Definition**
The `Step` object represents a single operation or action within a sequence. It defines the operation type, references for input/output data, conditions, and other parameters.

```cpp
struct Step {
    std::string oper;        // Operation type (e.g., "set", "wait_reset", "jump_if")
    std::string src;         // Source reference (e.g., "bits:3")
    std::string dest;        // Destination reference (e.g., "rbits:5")
    std::string var;         // Auxiliary variable for operations like add/multiply
    std::string cond;        // Condition for conditional steps (e.g., ">", "==")
    std::string counter;     // Counter reference for "wait_reset"
    int idx = -1;            // Step index
    int value = 0;           // Value to set or compare
    int jump_to = -1;        // Target step index for "jump_if"
    int time = 0;            // Time in milliseconds for "wait_reset"

    // Default Constructor
    Step() = default;

    // Constructor for 'wait_reset' operation
    Step(std::string counter_reg, int time)
        : oper("wait_reset"), counter(counter_reg), time(time) {}

    // Constructor for fully-specified step
    Step(const std::string& oper, const std::string& dest, int value = 0, const std::string& cond = "",
         int time = 0, std::string counter = "", int jump_to = 0)
        : oper(oper), dest(dest), value(value), cond(cond), time(time), counter(counter), jump_to(jump_to) {}
};
```

---

### **Purpose**
Each `Step` object is part of a sequence and defines a specific action to perform. Examples include:
- **Set a value:** `Step("set", "rbits:5", 1)`
- **Wait for a condition:** `Step("wait_reset", "lvar:count1", 100)`
- **Conditional jump:** `Step("jump_if", "rinput:3", 10, ">", 0)`

---

### **Fields**
1. **`oper`**: Specifies the type of operation (e.g., "set", "add", "jump_if").
2. **`src`**: Source data reference for the operation.
3. **`dest`**: Target register or variable to modify.
4. **`var`**: Auxiliary variable for arithmetic or logical operations.
5. **`cond`**: Condition string for operations like "jump_if".
6. **`counter`**: Counter reference for timed operations (e.g., "wait_reset").
7. **`idx`**: The step's position in the sequence.
8. **`value`**: Numerical value for "set" or comparison.
9. **`jump_to`**: Index to jump to in conditional operations.
10. **`time`**: Duration for timed operations.

---

### Example Usage

#### **Set Operation**
```cpp
Step set_step("set", "rbits:1", 100);
```
Sets the value of `rbits:1` to 100.

#### **Wait Operation**
```cpp
Step wait_step("lvar:counter1", 200);
```
Waits until the `lvar:counter1` reaches 200.

#### **Conditional Jump**
```cpp
Step jump_step("jump_if", "rinput:10", 50, ">", 5);
```
Jumps to step 5 if `rinput:10 > 50`.

---

Let me know if you need further clarification or want to expand on this!
---


Here's how you can decode the JSON representation of a `Step` object into a `std::map<int, std::any>` and also include the `oper` field as a key for easier usage in a `switch` or `if-else` case statement.

---

### **Code Implementation**

```cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum StepKeys {
    KEY_OPER = 0,
    KEY_SRC,
    KEY_DEST,
    KEY_VAR,
    KEY_COND,
    KEY_COUNTER,
    KEY_IDX,
    KEY_VALUE,
    KEY_JUMP_TO,
    KEY_TIME
};

// Function to decode a Step JSON object into a map of <int, std::any>
std::map<int, std::any> decode_step(const json& step_json) {
    std::map<int, std::any> step_map;

    // Map JSON fields to their respective integer keys
    if (step_json.contains("oper")) {
        step_map[KEY_OPER] = step_json["oper"].get<std::string>();
        // Add a special mapping for the operation key as well
        if (step_json["oper"] == "set") step_map[-KEY_OPER] = KEY_OPER + 1;
        else if (step_json["oper"] == "wait_reset") step_map[-KEY_OPER] = 2;
        else if (step_json["oper"] == "jump_if") step_map[-KEY_OPER] = 3;
        // Add other operations as needed
    }
    if (step_json.contains("src")) step_map[KEY_SRC] = step_json["src"].get<std::string>();
    if (step_json.contains("dest")) step_map[KEY_DEST] = step_json["dest"].get<std::string>();
    if (step_json.contains("var")) step_map[KEY_VAR] = step_json["var"].get<std::string>();
    if (step_json.contains("cond")) step_map[KEY_COND] = step_json["cond"].get<std::string>();
    if (step_json.contains("counter")) step_map[KEY_COUNTER] = step_json["counter"].get<std::string>();
    if (step_json.contains("idx")) step_map[KEY_IDX] = step_json["idx"].get<int>();
    if (step_json.contains("value")) step_map[KEY_VALUE] = step_json["value"].get<int>();
    if (step_json.contains("jump_to")) step_map[KEY_JUMP_TO] = step_json["jump_to"].get<int>();
    if (step_json.contains("time")) step_map[KEY_TIME] = step_json["time"].get<int>();

    return step_map;
}

// Function to demonstrate decoding
void decode_and_run_step(const json& step_json) {
    auto step_map = decode_step(step_json);

    // Example: Accessing values
    auto oper = std::any_cast<std::string>(step_map[KEY_OPER]);
    std::cout << "Decoded Step Operation: " << oper << std::endl;

    // Example switch-case on the decoded oper key
    int oper_key = std::any_cast<int>(step_map[-KEY_OPER]);
    switch (oper_key) {
        case 1: // KEY_OPER + 1 == "set"
            std::cout << "Performing 'set' operation." << std::endl;
            // Further logic for "set"
            break;
        case 2: // "wait_reset"
            std::cout << "Performing 'wait_reset' operation." << std::endl;
            // Further logic for "wait_reset"
            break;
        case 3: // "jump_if"
            std::cout << "Performing 'jump_if' operation." << std::endl;
            // Further logic for "jump_if"
            break;
        default:
            std::cout << "Unknown operation." << std::endl;
            break;
    }
}

int main() {
    // Example JSON representation of a Step
    std::string step_json_str = R"({
        "oper": "set",
        "src": "rinput:3",
        "dest": "rbits:5",
        "value": 100,
        "jump_to": 3,
        "time": 200,
        "cond": ">",
        "counter": "count1"
    })";

    json step_json = json::parse(step_json_str);

    // Decode and demonstrate the Step
    decode_and_run_step(step_json);

    return 0;
}
```

---

Below is an implementation that decodes variable references and provides `get_var` and `set_var` functions for efficient access to the various memory and Modbus areas. This includes decoding for modbus, shared memory, and both global and local variables.

---

### **Implementation**

#### **Variable Decoder**

A helper function `decode_var_type` is provided to parse variable references (`var_type`) and map them to their appropriate handlers.

```cpp
#include <unordered_map>
#include <string>
#include <iostream>
#include <stdexcept>
#include <vector>

enum VarType {
    VAR_MODBUS_INPUT,
    VAR_MODBUS_HOLD,
    VAR_MODBUS_BITS,
    VAR_MODBUS_COIL,
    VAR_RACK_MEM,
    VAR_RTS_MEM,
    VAR_GLOBAL_VAR,
    VAR_LOCAL_VAR,
    VAR_UNKNOWN
};

struct VarDef {
    VarType type;
    int offset;
    std::string name; // Only used for named variables like global_var or local_var
};

// Global and local variable storage
std::unordered_map<std::string, int> global_var_index;
std::vector<int> global_vars;
std::unordered_map<std::string, int> local_vars;

// Shared memory
std::vector<std::unordered_map<int, uint8_t>> shm_rack_mem(20); // 20 racks

// Decode the variable reference string into a `VarDef`
VarDef decode_var_type(const std::string& var_ref, int rack_number = -1) {
    VarDef def;
    if (var_ref.find("rinput:") == 0) {
        def.type = VAR_MODBUS_INPUT;
        def.offset = std::stoi(var_ref.substr(7));
    } else if (var_ref.find("input:") == 0) {
        def.type = VAR_MODBUS_INPUT;
        def.offset = std::stoi(var_ref.substr(6));
    } else if (var_ref.find("rhold:") == 0) {
        def.type = VAR_MODBUS_HOLD;
        def.offset = std::stoi(var_ref.substr(6));
    } else if (var_ref.find("bits:") == 0) {
        def.type = VAR_MODBUS_BITS;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("rack:") == 0) {
        def.type = VAR_RACK_MEM;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("rtos:") == 0) {
        def.type = VAR_RTS_MEM;
        def.offset = std::stoi(var_ref.substr(5));
    } else if (var_ref.find("g_") == 0) {
        def.type = VAR_GLOBAL_VAR;
        def.name = var_ref;
        if (global_var_index.find(var_ref) == global_var_index.end()) {
            throw std::runtime_error("Global variable " + var_ref + " not found.");
        }
        def.offset = global_var_index[var_ref];
    } else {
        def.type = VAR_LOCAL_VAR;
        def.name = var_ref;
    }
    return def;
}
```

#### **Get and Set Functions**

`get_var` and `set_var` functions retrieve or set values for the decoded `VarDef`.

```cpp
template <typename T>
T get_var(const VarDef& var_def, int rack_number = -1) {
    switch (var_def.type) {
        case VAR_MODBUS_INPUT:
        case VAR_MODBUS_HOLD:
        case VAR_MODBUS_BITS:
        case VAR_MODBUS_COIL:
            // Modbus access (requires Modbus function code and offset)
            return static_cast<T>(read_any_mb(var_def.type, var_def.offset, rack_number));
        case VAR_RACK_MEM:
            if (rack_number == -1) {
                throw std::runtime_error("Rack number is required for rack memory access.");
            }
            return *reinterpret_cast<T*>(&shm_rack_mem[rack_number][var_def.offset]);
        case VAR_RTS_MEM:
            if (rack_number == -1) {
                throw std::runtime_error("Rack number is required for RTOS memory access.");
            }
            // Example: Implement RTOS shared memory access here
            // return read_rtos_mem<T>(rack_number, var_def.offset);
            break;
        case VAR_GLOBAL_VAR:
            return static_cast<T>(global_vars[var_def.offset]);
        case VAR_LOCAL_VAR:
            if (local_vars.find(var_def.name) == local_vars.end()) {
                throw std::runtime_error("Local variable " + var_def.name + " not found.");
            }
            return static_cast<T>(local_vars[var_def.name]);
        default:
            throw std::runtime_error("Invalid variable type.");
    }
}

template <typename T>
void set_var(const VarDef& var_def, T value, int rack_number = -1) {
    switch (var_def.type) {
        case VAR_MODBUS_INPUT:
        case VAR_MODBUS_HOLD:
        case VAR_MODBUS_BITS:
        case VAR_MODBUS_COIL:
            write_any_mb(var_def.type, var_def.offset, value, rack_number);
            break;
        case VAR_RACK_MEM:
            if (rack_number == -1) {
                throw std::runtime_error("Rack number is required for rack memory access.");
            }
            *reinterpret_cast<T*>(&shm_rack_mem[rack_number][var_def.offset]) = value;
            break;
        case VAR_RTS_MEM:
            if (rack_number == -1) {
                throw std::runtime_error("Rack number is required for RTOS memory access.");
            }
            // Example: Implement RTOS shared memory write here
            // write_rtos_mem<T>(rack_number, var_def.offset, value);
            break;
        case VAR_GLOBAL_VAR:
            global_vars[var_def.offset] = static_cast<int>(value);
            break;
        case VAR_LOCAL_VAR:
            local_vars[var_def.name] = static_cast<int>(value);
            break;
        default:
            throw std::runtime_error("Invalid variable type.");
    }
}
```

#### **Example Usage**

```cpp
void example_usage() {
    // Initialize global variables
    global_var_index["g_var1"] = 0;
    global_var_index["g_var2"] = 1;
    global_vars = {100, 200};

    // Local variables
    local_vars["local1"] = 50;

    // Decode variable
    VarDef v1 = decode_var_type("rinput:10", 2);    // Rack Modbus input
    VarDef v2 = decode_var_type("rack:50", 1);      // Rack shared memory
    VarDef v3 = decode_var_type("g_var1");          // Global variable

    // Get and set variable values
    int val1 = get_var<int>(v1, 2);                 // Rack Modbus input
    set_var<int>(v2, 123, 1);                       // Write to rack memory
    int g_val = get_var<int>(v3);                   // Access global variable
    set_var<int>(v3, 456);                          // Modify global variable
}
```

---

### **Explanation**

1. **Efficient Access**:
   - Modbus, shared memory, global, and local variables are accessed based on the `VarDef` type, ensuring a single interface for all variable types.
   
2. **Extensibility**:
   - New variable types can be added by extending `VarType` and `decode_var_type`.

3. **Error Handling**:
   - Throws exceptions when variable references are invalid or required parameters (e.g., `rack_number`) are missing.

4. **Example Use Case**:
   - Easily integrates into `Step` operations like `src`, `dest`, and `var` for decoding and runtime execution.

### **Explanation**

1. **Step Key Mapping**:
   - Use an `enum` (`StepKeys`) to map human-readable keys to integers for `std::map`.

2. **Decode Function**:
   - The `decode_step` function takes a JSON object and populates a `std::map<int, std::any>`, where:
     - Integer keys correspond to the fields in `Step`.
     - Values are stored as `std::any` to handle mixed types (e.g., `int`, `std::string`).

3. **Special `oper` Mapping**:
   - A reverse mapping (`-KEY_OPER`) allows operations to be directly mapped to unique integers for efficient handling.

4. **Run Example**:
   - Demonstrates decoding the `Step` and using a switch statement based on the operation type.

---

### **Output Example**
For the given JSON:
```json
{
    "oper": "set",
    "src": "rinput:3",
    "dest": "rbits:5",
    "value": 100,
    "jump_to": 3,
    "time": 200,
    "cond": ">",
    "counter": "count1"
}
```

Output:
```
Decoded Step Operation: set
Performing 'set' operation.
```

---

Let me know if you'd like to extend this further!

### **4. Emergency Shutdown**
Safely disconnects the battery pack and signals an alarm.

#### JSON Representation
```json
{
    "sequence_name": "emergency_shutdown",
    "step": 0,
    "steps": [
        {"oper": "set", "value": 1, "dest": "rbits:5", "desc": "Set alarm signal"},
        {"oper": "wait_reset", "counter": "lcl:wait1", "time": 300, "desc": "Wait for confirmation"},
        {"oper": "set", "value": 0, "dest": "rbits:1", "desc": "Open negative contactor"},
        {"oper": "set", "value": 0, "dest": "rbits:3", "desc": "Open positive contactor"}
    ]
}
```

---

## **Triggers**

### **1. High Voltage Trigger**
Activates when the voltage of any rack exceeds a threshold, triggering an emergency shutdown.

#### JSON Representation
```json
{
    "trigger_name": "high_voltage",
    "src": "rinput:5",
    "oper": ">",
    "value": 450,
    "run": "emergency_shutdown"
}
```

---

### **2. Low State of Charge Trigger**
Activates when the state of charge (SOC) drops below a certain level.

#### JSON Representation
```json
{
    "trigger_name": "low_soc",
    "src": "rinput:15",
    "oper": "<",
    "value": 20,
    "run": "open_contactors"
}
```

---

### **3. Temperature Differential Trigger**
Activates when the temperature difference between cells in a rack exceeds a limit.

#### JSON Representation
```json
{
    "trigger_name": "high_temp_diff",
    "src_ref": "rinput",
    "src_reg": 30,
    "oper": ">",
    "value": 10,
    "run": "open_contactors"
}
```

---

### **4. Aggregate Overcurrent Trigger**
Runs the shutdown sequence if the total current across racks exceeds a safety threshold.

#### JSON Representation
```json
{
    "trigger_name": "overcurrent_shutdown",
    "aggregate": "any",
    "src": "rinput:12",
    "oper": ">",
    "value": 200,
    "run": "emergency_shutdown"
}
```

---

### **5. Cooling Trigger**
Activates the cooling system when temperature exceeds a threshold.

#### JSON Representation
```json
{
    "trigger_name": "high_temperature",
    "src": "rinput:40",
    "oper": ">",
    "value": 70,
    "run": "cooling_sequence"
}
```

---

## **Combination Example**
### JSON for Combined Sequences and Triggers
```json
{
    "sequences": [
        {
            "sequence_name": "close_contactors",
            "step": 0,
            "steps": [
                {"oper": "set", "value": 1, "dest": "rbits:1"},
                {"oper": "wait_reset", "counter": "lcl:wait1", "time": 200},
                {"oper": "set", "value": 1, "dest": "rbits:2"},
                {"oper": "wait_reset", "counter": "lcl:wait2", "time": 200},
                {"oper": "set", "value": 1, "dest": "rbits:3"},
                {"oper": "set", "value": 0, "dest": "rbits:2"}
            ]
        }
    ],
    "triggers": [
        {
            "trigger_name": "high_voltage",
            "src": "rinput:5",
            "oper": ">",
            "value": 450,
            "run": "emergency_shutdown"
        }
    ]
}
```

