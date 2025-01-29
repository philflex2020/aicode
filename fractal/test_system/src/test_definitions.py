

import sys

# Print all directories in Python's search path
# print("Python Path:")
# for path in sys.path:
#     print(path)
#export PYTHONPATH=$PYTHONPATH:$(pwd)/src
import parse_definitions

def main():
    # Path to the data definition file
    data_definition_file = 'src/data_definition.txt'

    # Read the data definitions from a file
    try:
        with open(data_definition_file, 'r') as file:
            data_definitions = file.read()
        
        # Parse the data definitions using the imported module
        tables = parse_definitions.parse_definitions(data_definitions)
        
        # Print parsed table information
        for table, details in tables.items():
            print(f"Table: {table}, Base Offset: {details['base_offset']}")
            for item in details['items']:
                print(f"  Item: {item['name']} at offset {item['offset']} with size {item['size']}")
        for table, details in tables.items():
            print(f"Table: {table}, Base Offset: {details['base_offset']}")

        # # Verify the offsets and print the results
        # verification_results = parse_definitions.verify_offsets(data_definitions)
        # print("Offset verification results:")
        # for table, is_correct in verification_results.items():
        #     status = "Correct" if is_correct else "Incorrect"
        #     print(f"{table}: {status}")

    except FileNotFoundError:
        print(f"Error: The file {data_definition_file} was not found.")

if __name__ == '__main__':
    main()
