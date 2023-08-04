import os
import re
import shutil

def create_directory(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

def extract_code_blocks(markdown_file, src_directory, include_directory):
    create_directory(src_directory)
    create_directory(include_directory)

    with open(markdown_file, 'r') as file:
        content = file.read()

    # Define the pattern for code block extraction
    pattern = r"\*\*(\w+\.h|\w+\.cpp)\*\*:\s*```cpp(.*?)```"
    matches = re.findall(pattern, content, re.DOTALL)

    for match in matches:
        filename, code_content = match
        filename = filename.strip()
        code_content = code_content.strip()

        # Save the code content to the appropriate directory
        if filename.endswith('.h'):
            directory = include_directory
        elif filename.endswith('.cpp'):
            directory = src_directory
        else:
            continue

        filepath = os.path.join(directory, filename)
        with open(filepath, 'w') as code_file:
            code_file.write(code_content)

if __name__ == "__main__":
    src_dir = "src"
    include_dir = "include"
    md_file = "code_blocks.md"

    extract_code_blocks(md_file, src_dir, include_dir)
