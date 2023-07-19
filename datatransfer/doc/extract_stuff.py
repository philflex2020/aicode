import os

def extract_code(input_file):
    # Create a directory to store the extracted files
    output_dir = "extracted_files"
    os.makedirs(output_dir, exist_ok=True)

    current_file_code = []
    current_file_name = None

    with open(input_file, "r") as f:
        for line in f:
            if line.startswith("// File: "):
                if current_file_name:
                    write_file(output_dir, current_file_name, current_file_code)
                current_file_code.clear()
                current_file_name = line[len("// File: "):].strip() + ".cpp"
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

    print("Files extracted successfully to the 'extracted_files' directory.")

def write_file(output_dir, file_name, file_code):
    file_path = os.path.join(output_dir, file_name)
    with open(file_path, "w") as f:
        f.writelines(file_code)

if __name__ == "__main__":
    input_file = "DataTransfer_2.md"  # Replace with the actual filename
    extract_code(input_file)