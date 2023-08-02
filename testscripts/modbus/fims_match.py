import sys
import json
from datetime import datetime, timedelta

def load_output_file(output_file):
    with open(output_file, 'r') as f:
        return json.load(f)

def compare_body_objects(output1, output2, time_threshold=5):
    failures = []
    for ts1, obj1 in output1.items():
        ts1 = float(ts1)
        for ts2, obj2 in output2.items():
            ts2 = float(ts2)
            if (ts2 > ts1) and abs(ts2 - ts1) <= time_threshold:
                print ("fail")
                print (ts1)
                print (ts2)
                if obj1["Body"] != obj2["Body"]:
                    failures.append((obj1,ts1, obj2,ts2))
    return failures

def write_report_to_log(failures, log_file):
    with open(log_file, 'w') as f:
        for obj1, ts1, obj2,ts2 in failures:
            f.write("Mismatched Body Objects:\n")
            f.write(f"Timestamp 1: {obj1['Timestamp']}")
            f.write(f" Ts 1: {ts1}\n")
            f.write(f"Body 1: {json.dumps(obj1['Body'], indent=2)}\n")
            f.write(f"Timestamp 2: {obj2['Timestamp']}")
            f.write(f" Ts 2: {ts2}\n")
            f.write(f"Body 2: {json.dumps(obj2['Body'], indent=2)}\n")
            f.write("\n")

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python script_name.py output_file1 output_file2 time_threshold log_file")
        sys.exit(1)

    output_file1 = sys.argv[1]
    output_file2 = sys.argv[2]
    time_threshold = float(sys.argv[3])
    log_file = sys.argv[4]

    output1 = load_output_file(output_file1)
    output2 = load_output_file(output_file2)

    failures = compare_body_objects(output1, output2, time_threshold)
    write_report_to_log(failures, log_file)
