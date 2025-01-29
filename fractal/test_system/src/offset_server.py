

import socket
import threading
import parse_definitions

# Your existing function to parse and validate the tables
def parse_and_validate(data_definitions):
    # Parse the data definitions using the imported module
    data_tables = parse_definitions.parse_definitions(data_definitions)
    parse_definitions.verify_offsets(data_definitions)

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
    try:
        parts = query.split(':')

        if parts[0] == "help":
            return "Available commands:\n - tables: List all tables\n - table:offset: Query by offset\n - table:name: Query by name\n - table *pattern*: Query by name pattern\n - table:x-y: Query by offset range\n"
        elif parts[0] == "tables":
            return "Known tables:\n" + "\n".join(data_tables.keys()) + "\n"
        if len(parts) < 2:
            return "Invalid query format. Use 'table:query' format.\n"

        table, identifier = parts[0], parts[1]
        #table_key = f"{table}:"
        parts = identifier.split(' ')
        table_key=f"{table}:{parts[0]}"

        #print(parts)
        if '-' in parts[1]:  # Range query
            start, end = map(int, parts[1].split('-'))
            results = []
            for item in data_tables.get(table_key, {}).get('items', []):
                if start <= item["offset"] <= end:
                    results.append(f"{item['name']} at offset {item['offset']}")
            if results:
                return "\n".join(results) + "\n"
            else:
                return "No items found in the specified range.\n"

        elif '*' in parts[1]:  # Pattern match query
            pattern = parts[1].replace('*', '')
            results = []
            for item in data_tables.get(table_key, {}).get('items', []):
                if pattern in item['name']:
                    results.append(f"{item['name']} at offset {item['offset']}")
            if results:
                return "\n".join(results) + "\n"
            else:
                return "No items found matching the pattern.\n"

        elif parts[1].isdigit():  # Offset query
            offset = int(parts[1])
            for item in data_tables.get(table_key, {}).get('items', []):
                if item["offset"] == offset:
                    return f"{item['name']} at offset {item['offset']}\n"
            return "Item not found at the specified offset.\n"
        
        else:  # Name query
            for item in data_tables.get(table_key, {}).get('items', []):
                if item['name'] == parts[1]:
                    return f"{item['name']} at offset {item['offset']}\n"
            return "Item not found with the specified name.\n"

    except Exception as e:
        return f"Error processing query: {str(e)}\n"

# # Function to process queries
# def handle_query(query, data_tables):
#     # Example query "table:name" or "table:offset"
#     try:
#         parts = query.split(':')
#         if parts[0] == "help":
#             return " There is no help\n"
#         elif parts[0] == "tables":
#             resp = " known tables\n"
#             for table in data_tables:
#                 resp += f"{table}\n"
#             return resp
#         table, identifier = parts[0], parts[1]
#         parts = identifier.split(' ')
#         tab=f"{table}:{parts[0]}"

#         if parts[1].isdigit():
#             for item in data_tables[tab]['items']:
#                 if item["offset"] == int(parts[1]):
#                     print(f" name {item['name']}\n")
#                     result = f"item['name']\n"                
#                     return result
#         else:
#             # Find by name
#             result = data_tables.get(tab, {}).get(identifier, None)
#         if result:
#             return f"{result}\n"
#         else:
#             return "Item not found\n"
#     except Exception as e:
#         return f"Error processing query: {str(e)}\n"


# Example usage:

# Main function to set up the server
def start_server(port, data_definition):
    host = 'localhost'
    port = port
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Set SO_REUSEADDR to 1 to reuse the socket
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # Optionally set SO_REUSEPORT to 1 to reuse the port (necessary for some platforms)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

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