import json
import subprocess
import sys

# Load test queries
def load_tests(filename):
    with open(filename, 'r') as f:
        return json.load(f)

# Send query using wscat
def send_query(query):
    print(f"  Running  Query : {query}")
    cmd = ['wscat', '-c', 'ws://192.168.86.209:9001']  # Replace with your WebSocket URL
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate(json.dumps(query) + '\n')
    if stderr:
        print(f"Error: {stderr}")
    print(f"  Sent Query : {query}")
    print(f"              Got Resp: {stdout}")
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
