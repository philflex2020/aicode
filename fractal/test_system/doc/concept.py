To implement this testing system using `wscat`, here’s how you can proceed:

### Plan
1. **Define Queries and Expected Results**:
   - Create a JSON file with queries and their expected results (including tolerances for comparison).

2. **Run Queries Using wscat**:
   - Send queries to the system via `wscat` and collect responses.

3. **Compare Results**:
   - Parse the responses and compare them with the expected results, considering the specified tolerances.

4. **Output Test Results**:
   - Display a summary indicating passed/failed tests and details for failures.

### Implementation
#### Sample Test File (`test_queries.json`)
```json
[
  {
    "query": {
      "action": "get",
      "seq": 126,
      "sm_name": "rtos_0",
      "reg_type": "sm16",
      "offset": "0xc800",
      "num": 64
    },
    "expected": {
      "seq": 126,
      "data": [65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535]
    },
    "tolerance": 5
  },
  {
    "query": {
      "action": "get",
      "seq": 127,
      "sm_name": "rtos_1",
      "reg_type": "sm16",
      "offset": "0xc900",
      "num": 32
    },
    "expected": {
      "seq": 127,
      "data": [0, 0, 0, 0, 0, 0, 0, 0]
    },
    "tolerance": 0
  }
]
```

#### Python Script to Test Queries
```python
import json
import subprocess
import sys

# Load test queries
def load_tests(filename):
    with open(filename, 'r') as f:
        return json.load(f)

# Send query using wscat
def send_query(query):
    cmd = ['wscat', '-c', 'ws://localhost:8080']  # Replace with your WebSocket URL
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate(json.dumps(query) + '\n')
    if stderr:
        print(f"Error: {stderr}")
    return stdout.strip()

# Compare results with tolerance
def compare_results(expected, actual, tolerance):
    if expected.get("seq") != actual.get("seq"):
        return False, "Sequence mismatch"

    expected_data = expected.get("data", [])
    actual_data = actual.get("data", [])

    if len(expected_data) != len(actual_data):
        return False, "Data length mismatch"

    for i, (exp, act) in enumerate(zip(expected_data, actual_data)):
        if abs(exp - act) > tolerance:
            return False, f"Value mismatch at index {i}: expected {exp}, got {act}"

    return True, "OK"

# Run tests
def run_tests(test_file):
    tests = load_tests(test_file)
    passed = 0
    failed = 0

    for i, test in enumerate(tests):
        query = test['query']
        expected = test['expected']
        tolerance = test['tolerance']

        print(f"Running Test {i+1}: {query}")
        response_raw = send_query(query)
        try:
            response = json.loads(response_raw)
        except json.JSONDecodeError:
            print(f"Invalid JSON response: {response_raw}")
            failed += 1
            continue

        result, message = compare_results(expected, response, tolerance)
        if result:
            print(f"Test {i+1} Passed: {message}")
            passed += 1
        else:
            print(f"Test {i+1} Failed: {message}")
            failed += 1

    print(f"\nSummary: {passed} Passed, {failed} Failed")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <test_file>")
        sys.exit(1)

    test_file = sys.argv[1]
    run_tests(test_file)
```

### Steps to Run
1. Save the test queries in a JSON file (e.g., `test_queries.json`).
2. Save the script to a file (e.g., `test_runner.py`).
3. Install `wscat` if not already installed:
   ```bash
   npm install -g wscat
   ```
4. Run the script with the test file:
   ```bash
   python3 test_runner.py test_queries.json
   ```

### Output Example
```
Running Test 1: {'action': 'get', 'seq': 126, 'sm_name': 'rtos_0', 'reg_type': 'sm16', 'offset': '0xc800', 'num': 64'}
Test 1 Passed: OK
Running Test 2: {'action': 'get', 'seq': 127, 'sm_name': 'rtos_1', 'reg_type': 'sm16', 'offset': '0xc900', 'num': 32'}
Test 2 Failed: Value mismatch at index 0: expected 0, got 5

Summary: 1 Passed, 1 Failed
```

This system allows you to send queries, collect responses, and verify them against expected results, considering tolerances. Let me know if you need further adjustments!