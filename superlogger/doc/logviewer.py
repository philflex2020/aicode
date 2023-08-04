import os
import glob
import datetime

# Read log ID formats
#def read_log_formats(project):
#    with open(f"/var/log/{project}/logids") as f:
#        log_formats = dict(line.strip().split('\t', 1) for line in f)
#    return log_formats
def read_log_formats(project):
    with open(f"/var/log/{project}/logids") as f:
        log_formats = {}
        for line in f:
            parts = line.strip().split(':', 1)
            if len(parts) == 2:
                log_formats[parts[0]] = parts[1]
    return log_formats
def parse_log_line(line, log_formats):
    parts = line.strip().split('\t')
    timestamp = datetime.datetime.strptime(parts[0], "%Y-%m-%dT%H:%M:%S")
    log_id = parts[1]
    vars = parts[2:]
    format = log_formats.get(log_id, "")
    formatted_message = format.format(*vars)
    return (timestamp, formatted_message)

# # Parse a log line into a tuple (timestamp, formatted message)
# def parse_log_line(line, log_formats):
#     parts = line.strip().split('\t')
#     timestamp = datetime.datetime.fromisoformat(parts[0])
#     log_id = parts[1]
#     vars = parts[2:]
#     format = log_formats.get(log_id, "")
#     formatted_message = format.format(*vars)
#     return (timestamp, formatted_message)

# Read all log entries from a file
def read_log_entries(filename, log_formats):
    with open(filename) as f:
        return [parse_log_line(line, log_formats) for line in f]

def main():
    project = "testProject"
    log_formats = read_log_formats(project)

    # Get all log files
    log_files = glob.glob(f"/var/log/{project}/*/*")

    # Read all log entries from all files
    log_entries = [entry for log_file in log_files for entry in read_log_entries(log_file, log_formats)]

    # Sort log entries by timestamp
    log_entries.sort()

    # Print log entries
    for timestamp, message in log_entries:
        print(f"{timestamp}: {message}")

if __name__ == "__main__":
    main()
