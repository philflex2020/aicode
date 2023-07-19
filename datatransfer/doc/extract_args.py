import os
import sys

def extract_code(input_file, output_dir):
    os.makedirs(output_dir, exist_ok=True)

    current_file_code = []
    current_file_name = None

    with open(input_file, "r") as f:
        for line in f:
            if line.startswith("// File: "):
                if current_file_name:
                    write_file(output_dir, current_file_name, current_file_code)
                current_file_code.clear()
                #current_file_name = line[len("// File: "):].strip() + ".cpp"
                current_file_name = line[len("// File: "):].strip()
            elif line.startswith("// End File: "):
                if current_file_name:
                    write_file(output_dir, current_file_name, current_file_code)
                current_file_code.clear()
                current_file_name = None
            elif line.startswith("// Makefile"):
                current_file_name = "Makefile"
            else:
                current_file_code.append(line)

        if current_file_name:
            write_file(output_dir, current_file_name, current_file_code)

    print("Files extracted successfully to the '{}' directory.".format(output_dir))

def write_file(output_dir, file_name, file_code):
    file_path = os.path.join(output_dir, file_name)
    with open(file_path, "w") as f:
        f.writelines(file_code)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python extract_args.py <input_file> <output_directory>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_dir = sys.argv[2]

    extract_code(input_file, output_dir)
