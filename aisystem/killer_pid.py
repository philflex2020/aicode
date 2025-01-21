import os
import sys
import signal
import time

def monitor_file_size(file_path, pid, max_size_mb, check_interval=1):
    """
    Monitor a file size and kill a process if the file exceeds the maximum size.

    :param file_path: Path to the file to monitor
    :param pid: Process ID to terminate if file size exceeds max_size_mb
    :param max_size_mb: Maximum allowed size in MB
    :param check_interval: Interval (in seconds) to check file size
    """
    try:
        max_size_bytes = max_size_mb * 1024 * 1024

        while True:
            if os.path.exists(file_path):
                file_size = os.path.getsize(file_path)
                if file_size > max_size_bytes:
                    print(f"File {file_path} exceeded {max_size_mb} MB. Killing process {pid}.")
                    os.kill(pid, signal.SIGTERM)  # Send termination signal
                    break
            else:
                print(f"File {file_path} does not exist. Exiting.")
                break

            time.sleep(check_interval)

    except ProcessLookupError:
        print(f"Process with PID {pid} does not exist.")
    except PermissionError:
        print(f"Permission denied while accessing {file_path} or killing process {pid}.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python killer_pid.py <file_path> <pid> <max_size_mb>")
        sys.exit(1)

    file_path = sys.argv[1]
    try:
        pid = int(sys.argv[2])
        max_size_mb = float(sys.argv[3])
    except ValueError:
        print("PID and max_size_mb must be numbers.")
        sys.exit(1)

    monitor_file_size(file_path, pid, max_size_mb)