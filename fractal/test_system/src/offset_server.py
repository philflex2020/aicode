

import socket
import threading
import parse_definitions

# Your existing function to parse and validate the tables
def parse_and_validate(data_definitions):
    # Parse the data definitions using the imported module
    data_tables = parse_definitions.parse_definitions(data_definitions)

    # Your existing code to parse and check offsets
    return data_tables

# Function to handle client requests
def client_handler(connection, address, data_tables):
    try:
        print(f"Connected by {address}")
        while True:
            data = connection.recv(1024)
            if not data:
                break
            query = data.decode('utf-8').strip()
            response = handle_query(query, data_tables)
            connection.sendall(response.encode('utf-8'))
    finally:
        connection.close()

# Function to process queries
def handle_query(query, data_tables):
    # Example query "table:name" or "table:offset"
    try:
        parts = query.split(':')
        table, identifier = parts[0], parts[1]
        if identifier.isdigit():
            # Find by offset
            result = next((item for item in data_tables[table] if item['offset'] == int(identifier)), None)
        else:
            # Find by name
            result = data_tables.get(table, {}).get(identifier, None)
        if result:
            return f"{result}\n"
        else:
            return "Item not found\n"
    except Exception as e:
        return f"Error processing query: {str(e)}\n"


# def parse_definitions(data):
#     import re
#     tables = {}
#     current_table = None
#     current_offset = 0

#     lines = data.strip().split('\n')
#     for line in lines:
#         line = line.strip()
#         if not line:
#             continue
        
#         # Detect new table
#         if line.startswith('['):
#             match = re.match(r'\[(.+?):(.+?):(\d+)\]', line)
#             if match:
#                 system, reg_type, base_offset = match.groups()
#                 current_table = f"{system}:{reg_type}"
#                 current_offset = int(base_offset)
#                 tables[current_table] = {'base_offset': current_offset, 'items': [], 'calculated_size': 0}
#             continue
        
#         # Parse data items within a table
#         parts = line.split(':')
#         if len(parts) < 2:
#             continue

#         name = parts[0].strip()
#         offset = int(parts[1].strip())
#         size = int(parts[2].strip()) if len(parts) > 2 else 1
        
#         # Validate the offset sequence
#         expected_offset = tables[current_table]['base_offset'] + tables[current_table]['calculated_size']
#         if offset != expected_offset:
#             raise ValueError(f"Offset mismatch in {current_table} for {name}, expected {expected_offset}, found {offset}")
        
#         # Update the table entry
#         tables[current_table]['items'].append({'name': name, 'offset': offset, 'size': size})
#         tables[current_table]['calculated_size'] += size

#     return tables

# def verify_offsets(data_definition):
#     """
#     Verify that each data entry in the provided data definition has an offset that
#     follows sequentially from the previous entry's offset plus its size.
    
#     Args:
#     data_definition (str): Multiline string containing data definitions in the format:
#                            <name>:<offset>[:<size>], where <size> defaults to 1 if not specified.

#     Returns:
#     bool: True if all offsets are sequential and correctly calculated, False otherwise.
#     str: Message indicating success or the point of failure.
#     """
#     lines = data_definition.strip().split('\n')
#     last_offset = -1  # Initialize to -1 to handle the first offset check correctly.
    
#     for line in lines:
#         #print(line)
#         parts = line.split(':')
#         name = parts[0]
#         offset = int(parts[1])
#         size = int(parts[2]) if len(parts) > 2 else 1
        
#         if last_offset != -1 and offset != last_offset + 1:
#             print(f"Error in data definition: {name} at offset {offset} does not follow {last_offset + 1}")
#             gap = offset - (last_offset+1)
#             print(f" offset gap {gap}")
#             return False, f"Error in data definition: {name} at offset {offset} does not follow {last_offset + 1}"
        
#         last_offset = offset + size - 1  # Update last_offset to the last used position by this entry
#     print("All offsets are sequentially correct.")
#     return True, "All offsets are sequentially correct."

# Example usage:

# Main function to set up the server
def start_server(port, data_definition):
    host = 'localhost'
    port = port
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((host, port))
    server_socket.listen()

    data_tables = parse_and_validate(data_definition)

    print(f"Server started on {host}:{port}")
    try:
        while True:
            conn, addr = server_socket.accept()
            thread = threading.Thread(target=client_handler, args=(conn, addr, data_tables))
            thread.start()
    finally:
        server_socket.close()




# # Parse and print the table definitions and check offset correctness
# table_definitions = parse_definitions(data_definition)
# for table, info in table_definitions.items():
#     print(f"Table {table} starting at offset {info['base_offset']}")
#     total_size  = 0
#     for item in info['items']:
#         total_size += item['size']
#     #    print(f"  {item['name']} at offset {item['offset']} size {item['size']}")
#     print(f"total size for table {table} is {total_size}")
#verify_offsets(data_definition)
# Function to load data definitions from a file
def load_data_definitions(file_path):
    try:
        with open(file_path, 'r') as file:
            data_definition = file.read()
        return data_definition
    except IOError as e:
        print(f"Error reading file {file_path}: {e}")
        return ""

if __name__ == '__main__':
    file_path = 'src/data_definition.txt'
    data_definition = load_data_definitions(file_path)
    if data_definition:
        start_server(9999, data_definition)