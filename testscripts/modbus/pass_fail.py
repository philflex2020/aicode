# check log file , it must contain all the pass strings an none of the fail strings

import sys
import os

def check_strings_in_file(strings_file, log_file):
    with open(strings_file, 'r') as f:
        strings = set(line.strip() for line in f)

    with open(log_file, 'r') as f:
        log_content = f.readlines()

    fail_strings = []
    for line_num, line in enumerate(log_content, 1):
        for s in strings:
            if s in line:
                fail_strings.append((s, line_num))

    return fail_strings

def main():
    if len(sys.argv) != 3:
        print("Usage: python script_name.py directory_name log_file_name")
        sys.exit(1)

    directory_name = sys.argv[1]
    log_file_name = sys.argv[2]

    pass_strings_file = os.path.join(directory_name, "pass_strings.txt")
    fail_strings_file = os.path.join(directory_name, "fail_strings.txt")

    if not os.path.exists(pass_strings_file):
        print("pass_strings.txt file not found in the specified directory.")
        sys.exit(1)

    if not os.path.exists(fail_strings_file):
        print("fail_strings.txt file not found in the specified directory.")
        sys.exit(1)

    fail_strings = check_strings_in_file(fail_strings_file, log_file_name)

    if fail_strings:
        with open(os.path.join(directory_name, "output_log.txt"), 'w') as f:
            for fail_string, line_num in fail_strings:
                f.write(f"Fail String: {fail_string}, Line Number: {line_num}\n")
        print("fail")
    else:
        with open(os.path.join(directory_name, "output_log.txt"), 'w') as f:
            f.write("pass")
        print("pass")

if __name__ == "__main__":
    main()
