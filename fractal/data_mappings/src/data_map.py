import sys
import os
import json
import math

from pathlib import Path
from collections import defaultdict

# Define record structure
class Record:
    def __init__(self, source, operation, dest, name, notes=""):
        self.source = source.strip()
        self.operation = operation.strip()
        self.dest = dest.strip()
        self.name = name.strip()
        self.notes = notes.strip()

    def normalize_source(self):
        """Normalize the source field."""
        modbus_type, _, rest = self.source.partition(':')
        if modbus_type == "rinput":
            self.source = f"rack:input:{rest}"
        elif modbus_type == "input":
            self.source = f"sbmu:input:{rest}"
        elif modbus_type == "rbits":
            self.source = f"rack:bits:{rest}"
        elif modbus_type == "bits":
            self.source = f"sbmu:bits:{rest}"
        elif modbus_type == "rhold":
            self.source = f"rack:hild:{rest}"
        elif modbus_type == "hold":
            self.source = f"sbmu:hold:{rest}"

    def normalize_dest(self):
        """Normalize the destination field."""
        modbus_type, _, rest = self.dest.partition(':')
        if modbus_type == "rinput":
            self.dest = f"rack:input:{rest}"
        elif modbus_type == "input":
            self.dest = f"sbmu:input:{rest}"
        elif modbus_type == "rbits":
            self.dest = f"rack:bits:{rest}"
        elif modbus_type == "bits":
            self.dest = f"sbmu:bits:{rest}"
        elif modbus_type == "rhold":
            self.dest = f"rack:hild:{rest}"
        elif modbus_type == "hold":
            self.dest = f"sbmu:hold:{rest}"


    def to_dict(self):
        return {
            "source": self.source,
            "operation": self.operation,
            "dest": self.dest,
            "name": self.name,
            "notes": self.notes
        }

# [header]
# File Name: data.txt
# Report name: FractalBms Data Mappings
# Author: Your Name
# Version: 1.0
# Creation Date: 2025-01-14
# Changelog:
# - 2025-01-14 Initial creation
# - 2025-01-14 Added Modbus support
# - 2025-01-20 Added JSON and Markdown generation

def generate_report(json_data, metadata):
    """Generate a Markdown report with a header page, index, and content."""
    # if changes is None:
    #     changes = ["Initial creation", "Added details to the report format", "Minor formatting fixes"]
    
    #report_name="Report", author="Your Name", revision="1.0", changes=None):

    # Header Page
    markdown = f"# {metadata['report name']}\n\n"
    markdown += f"**Revision:** {metadata['version']}\n\n"
    markdown += f"**Author:** {metadata['author']}\n\n"
    markdown += f"**Creation Date:** {metadata['creation date']}\n\n"
    markdown += "## Changelog:\n\n"
    # Changelog
    if "changelog" in metadata:
        for change in metadata["changelog"]:
            markdown += f"- {change}\n"
    markdown += "\n---\n\n"
    
    # Index Page
    markdown += "## Index\n\n"
    index_entries = []
    current_page = 1
    rows_per_page = 20  # Assume 20 rows fit on a single Markdown page
    row_count = 0

    for name in json_data.keys():
        if row_count >= rows_per_page:
            current_page += 1
            row_count = 0
        index_entries.append((name, current_page))
        row_count += 1
    
    for entry in index_entries:
        markdown += f"- {entry[0]} (Page {entry[1]})\n"
    markdown += "\n---\n\n"
    
    # Content Section
    markdown += "## Data Table\n\n"
    headers = ["**Name**", "**Source**", "**Dest**", "**Notes**"]
    separator = ["---"] * len(headers)
    separator[0] = ":---"  # Left-align the first column
    separator[1] = ":---"  # Left-align the second column
    separator[2] = ":---"  # Left-align the third column
    separator[3] = ":---"  # Left-align the fourth column

    markdown += "| " + " | ".join(headers) + " |\n"
    markdown += "| " + " | ".join(separator) + " |\n"
    row_count = 0
    current_page = 1

    for name, details in json_data.items():
        if row_count >= rows_per_page:
            markdown += f"\n--- Page {current_page} ---\n\n"
            markdown += "| " + " | ".join(headers) + " |\n"
            markdown += "| " + " | ".join(separator) + " |\n"
            current_page += 1
            row_count = 0
        row = [
            name,
            details["source"],
            details["dest"],
            details["notes"]
        ]
        markdown += "| " + " | ".join(row) + " |\n"
        row_count += 1

    return markdown

# # Example Usage
# json_data = {
#     "total_undervoltage": {
#         "source": "rbits:201",
#         "destination": "bits:1",
#         "notes": "Sum from rbms reg aggregates across online racks"
#     },
#     "total_overvoltage": {
#         "source": "rbits:204",
#         "destination": "bits:4",
#         "notes": ""
#     },
#     "total_overcurrent": {
#         "source": "rbits:207",
#         "destination": "bits:7",
#         "notes": ""
#     }
# }

# report = generate_report(json_data, report_name="BMS Summary Report", author="John Doe", revision="2.1")
# print(report)


# Generate Markdown table
def json_to_markdown(json_data):
    """Convert JSON data to a Markdown table with highlighted header and left-aligned columns."""
    # Define headers and highlight them
    headers = ["**Name**", "**Source**", "**Dest**", "**Notes**"]
    
    # Prepare the separator row for left alignment
    separator = ["---"] * len(headers)
    separator[0] = ":---"  # Left-align the first column (Name)
    separator[1] = ":---"  # Left-align the second column (Source)
    separator[2] = ":---"  # Left-align the third column (Destination)
    separator[3] = ":---"  # Left-align the fourth column (Notes)
    
    # Start building the table
    markdown = "| " + " | ".join(headers) + " |\n"
    markdown += "| " + " | ".join(separator) + " |\n"

    # Add rows from JSON data
    for name, details in json_data.items():
        row = [
            name,
            details["source"],
            details["dest"],
            details["notes"]
        ]
        markdown += "| " + " | ".join(row) + " |\n"
    
    return markdown

def json_to_markdown2(data):
    headers = ["Name", "Source", "Dest", "Notes"]
    table = ["| " + " | ".join(headers) + " |", "| " + " | ".join(["-" * len(header) for header in headers]) + " |"]
    
    for name, details in data.items():
        row = [
            name,
            details.get("source", ""),
            details.get("dest", ""),
            details.get("notes", "")
        ]
        table.append("| " + " | ".join(row) + " |")
    
    return "\n".join(table)
    

def record_to_json(records):
    """Convert a list of Record objects into a JSON structure."""
    json_data = {}
    for record in records:
        json_data[record.name] = {
            "source": record.source,
            "oper": record.operation,
            "dest": record.dest,
            "notes": record.notes
        }
    return json_data


def parse_file(input_file):
    """Parse the input file into a list of records."""
    records = []
    changelog = []
    metadata = {}
    in_header =  False
    in_data = False
    need_keys =  True

    with open(input_file, 'r') as f:
        lines = f.readlines()

        for line in lines:

            if line.startswith("[header]"):
                in_header = True
                in_data = False
                continue
            elif line.startswith("[data]"):
                in_data = True
                in_header = False
                continue
            if in_header and line:
                # print(headers)
                if line.lower().startswith("changelog:"):
                    changelog = []  # Reset the changelog
                    continue
                elif changelog is not None and line.startswith("-"):
                    line = line.strip("- ").strip()
                    print(line)
                    changelog.append(line)
                elif ":" in line:
                    # Parse header metadata as key-value pairs
                    key, _, value = line.partition(":")
                    metadata[key.strip().lower()] = value.strip()
            elif in_data and "|" in line:
                if not line.strip() or '|' not in line:
                    continue
                if need_keys:
                    need_keys = False
                    headers = [header.strip().lower() for header in lines[0].split('|')]
                    headers = headers[1:]
                    continue

                fields = [field.strip() for field in line.split('|')]
                fields = fields[1:]
                print(fields)
                #return records
                record = Record(
                    source=fields[0],
                    operation=fields[1],
                    dest=fields[2],
                    name=fields[3],
                    notes=fields[4] if len(fields) > 4 else ""
                )
                #print(record)
                #return records

                records.append(record)
    metadata['changelog'] = changelog
    return metadata, records

#['rinput:139', 'sum2', 'input:18', 'accumulated_charge_capacity', '', '']
# rinput:138 needs to become rack:input:138 size 2 
# input:18:4 needs to become sbms:input:18 size 4
# I want to print all the rack points sorted  by offset with a header in a table 
# Rack Modbus Data
# bits
#    |offset| size| name|
# hold
#    |offset| size| name|
# inputs
#    |offset| size| name|
# Sbmu Modbus Data
# bits
#    |offset| size| name|
# hold
#    |offset| size| name|
# inputs
#    |offset| size| name|
def generate_modbus_json(records, output_dir):
    """Generate a Json file of Modbus registers."""
    modbus_data = defaultdict(lambda: defaultdict(list))

    # Normalize and categorize records
    for record in records:
        record.normalize_source()
        record.normalize_dest()

        # Determine type and offset
        modbus_source, _, offset_size = record.source.partition(':')
        modbus_type, _, offset_size = offset_size.partition(':')
        offset, _, size = offset_size.partition(':')
        size = size if size else "1"

        print(record.name)
        # Store the record
        modbus_data[modbus_type][int(offset)].append({
            "size": size,
            "name": record.name,
            "notes": record.notes,
        })

    # Sort offsets and prepare Markdown output
    output_file = Path(output_dir) / "modbus_registers.json"
    with open(output_file, 'w') as f:
        f.write("[\n")  # Add a newline after each JSON object
        f_step = True
        for modbus_type in modbus_data.items()
            if not f_step:
                f.write(",\n")  # Add a newline after each JSON object
            f_step = False
            json.dump(modbus_type, f)
        f.write("\n]\n")  # Add a newline after each JSON object
    print(f"Modbus Mappings sent to  {output_file}")

    #     f.write("# Modbus Registers\n\n")
        
    #     for modbus_type, entries in modbus_data.items():
    #         if modbus_type == "bits":
    #             btype = "Discrete Inputs"
    #         elif modbus_type == "hold":
    #             btype = "Holding Registers"
    #         elif modbus_type == "input":
    #             btype = "Input Registers"
    #         elif modbus_type == "coil":
    #             btype = "Coils"
    #         f.write(f"## Modbus  {btype} \n\n")
    #         f.write("| Offset | Size | Name | Notes |\n")
    #         f.write("|--------|------|------|-------|\n")
            
    #         for offset in sorted(entries.keys()):
    #             for entry in entries[offset]:
    #                 f.write(f"| {offset} | {entry['size']} | {entry['name']} | {entry['notes']} |\n")
    #         f.write("\n")
    
    # print(f"Modbus registers report saved to {output_file}")


def generate_modbus_registers(records, output_dir):
    """Generate a Markdown file of Modbus registers."""
    modbus_data = defaultdict(lambda: defaultdict(list))

    # Normalize and categorize records
    for record in records:
        record.normalize_source()
        record.normalize_dest()

        # Determine type and offset
        modbus_source, _, offset_size = record.source.partition(':')
        modbus_type, _, offset_size = offset_size.partition(':')
        offset, _, size = offset_size.partition(':')
        size = size if size else "1"

        print(record.name)
        # Store the record
        modbus_data[modbus_type][int(offset)].append({
            "size": size,
            "name": record.name,
            "notes": record.notes,
        })

    # Sort offsets and prepare Markdown output
    output_file = Path(output_dir) / "modbus_registers.md"
    with open(output_file, 'w') as f:
        f.write("# Modbus Registers\n\n")
        
        for modbus_type, entries in modbus_data.items():
            if modbus_type == "bits":
                btype = "Discrete Inputs"
            elif modbus_type == "hold":
                btype = "Holding Registers"
            elif modbus_type == "input":
                btype = "Input Registers"
            elif modbus_type == "coil":
                btype = "Coils"
            f.write(f"## Modbus  {btype} \n\n")
            f.write("| Offset | Size | Name | Notes |\n")
            f.write("|--------|------|------|-------|\n")
            
            for offset in sorted(entries.keys()):
                for entry in entries[offset]:
                    f.write(f"| {offset} | {entry['size']} | {entry['name']} | {entry['notes']} |\n")
            f.write("\n")
    
    print(f"Modbus registers report saved to {output_file}")


def generate_names_with_definitions(records, metadata, output_dir):
    """Generate a Markdown file of names with Modbus definitions."""

    jrec = record_to_json(records)
    jfile = json_to_markdown(jrec)
    jfile = generate_report(jrec, metadata)

    output_file = Path(output_dir) / "names_with_definitions.md"
    with open(output_file, 'w') as f:
        f.write(jfile)
    # with open(output_file, 'w') as f:
    #     f.write("# Names with Modbus Definitions\n\n")
    #     for record in records:
    #         f.write(
    #             f"- **{record.name}**: {{\"source\": \"{record.source}\", \"destination\": \"{record.dest}\"}}\n"
    #         )
    print(f"Names with definitions report saved to {output_file}")


#['rinput:139', 'sum:2', 'input:18', 'accumulated_charge_capacity', '', '']
# this will parse the src and dest regs as before 
# the func will be parsed as a function name and a possible arg after the : 
# but the output will be header  "System Functions"
# then for each function
# |function name| desc  
#    then a table of inputs , outputs and names
def generate_functions(records, output_dir):
    """Generate a Markdown file of functions with source, destination, and name data."""
    # Organize records by function
    function_data = defaultdict(list)

    for record in records:
        # Normalize source and destination fields
        record.normalize_source()
        record.normalize_dest()

        # Extract function name and arguments
        func_name, _, func_arg = record.operation.partition(':')

        # Add data to the function map
        function_data[func_name].append({
            "source": record.source,
            "dest": record.dest,
            "name": record.name,
            "arg": func_arg or "N/A",
        })

    # Output Markdown file
    output_file = Path(output_dir) / "functions.md"
    with open(output_file, 'w') as f:
        f.write("# System Functions\n\n")
        for func, entries in function_data.items():
            f.write(f"## {func.capitalize()}\n\n")
            f.write("### Description\n")
            f.write(f"- Function `{func}` processes {len(entries)} entries.\n\n")

            # Combined Table
            f.write("### Entries\n")
            f.write("| Source           | Dest      | Name                 | Argument |\n")
            f.write("|------------------|------------------|----------------------|----------|\n")
            for entry in entries:
                f.write(f"| {entry['source']} | {entry['dest']} | {entry['name']} | {entry['arg']} |\n")

            f.write("\n")
    print(f"Functions report saved to {output_file}")


def generate_functions1(records, output_dir):
    """Generate a Markdown file of functions with source and destination data."""
    function_data = defaultdict(lambda: {"sources": [], "dest": []})

    for record in records:
        func_name, _, _ = record.operation.partition(':')
        function_data[func_name]["sources"].append(record.source)
        function_data[func_name]["dest"].append(record.dest)

    output_file = Path(output_dir) / "functions.md"
    with open(output_file, 'w') as f:
        f.write("# Functions\n\n")
        for func, data in function_data.items():
            f.write(f"## {func}\n")
            f.write(json.dumps(data, indent=4) + "\n")
    print(f"Functions report saved to {output_file}")

def generate_sequence_steps(records, output_dir):
    """Generate a JSON file of sequence steps."""
    steps = []
    for record in records:
        func_name, _, arg = record.operation.partition(':')
        step = {
            "oper": func_name,
            "arg": arg or "N/A",
            "src": record.source,
            "dest": record.dest,
            "notes": record.notes,
            "variable": record.name,
        }
        steps.append(step)

    output_file = Path(output_dir) / "sequence_steps.json"
    with open(output_file, 'w') as f:
        f.write("[\n")  # Add a newline after each JSON object
        f_step = True
        for step in steps:
            if not f_step:
                f.write(",\n")  # Add a newline after each JSON object
            f_step = False
            json.dump(step, f)
        f.write("\n]\n")  # Add a newline after each JSON object
    print(f"Sequence steps saved to {output_file}")

def main():
    if len(sys.argv) < 3:
        print("Usage: python data_map.py <input_file> <report_type> [<output_dir>]")
        sys.exit(1)

    input_file = sys.argv[1] if len(sys.argv) > 1 else "./data/data_map.txt"
    report_type = sys.argv[2] if len(sys.argv) > 2 else "mj"
    output_dir = sys.argv[3] if len(sys.argv) > 3 else "./output"

    if not Path(input_file).is_file():
        print(f"Error: Input file {input_file} does not exist.")
        sys.exit(1)

    os.makedirs(output_dir, exist_ok=True)
    metadata, records = parse_file(input_file)


    # Print metadata
    print("File Metadata:")
    for key, value in metadata.items():
        print(f"  {key.capitalize()}: {value}")

    if report_type == "modbus_json":
        generate_modbus_json(records, output_dir)
    elif report_type == "mj":
        generate_modbus_json(records, output_dir)
    elif report_type == "modbus_registers":
        generate_modbus_registers(records, output_dir)
    elif report_type == "mb":
        generate_modbus_registers(records, output_dir)
    elif report_type == "names_with_definitions":
        generate_names_with_definitions(records, metadata, output_dir)
    elif report_type == "nd":
        generate_names_with_definitions(records, metadata, output_dir)
    elif report_type == "functions":
        generate_functions(records, output_dir)
    elif report_type == "f":
        generate_functions(records, output_dir)
    elif report_type == "sequence_steps":
        generate_sequence_steps(records, output_dir)
    elif report_type == "ss":
        generate_sequence_steps(records, output_dir)
    else:
        print(f"Unknown report type: {report_type}")
        sys.exit(1)

if __name__ == "__main__":
    main()

