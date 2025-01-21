import os
import time
import psutil

# Configuration
FILE_PATH = "/path/to/your/file.log"  # Replace with your file path
SIZE_LIMIT_MB = 100  # Size limit in MB
CHECK_INTERVAL = 5  # Interval to check the file size in seconds
PROCESS_NAME = "your_process_name"  # Replace with the process name to monitor

def get_file_size_mb(file_path):
    """Get the file size in MB."""
    try:
        return os.path.getsize(file_path) / (1024 * 1024)  # Convert bytes to MB
    except FileNotFoundError:
        return 0  # File does not exist

def kill_process_by_name(process_name):
    """Kill a process by its name."""
    for proc in psutil.process_iter(['name', 'pid']):
        if proc.info['name'] == process_name:
            try:
                proc.terminate()  # Send SIGTERM
                print(f"Process {process_name} (PID: {proc.info['pid']}) terminated.")
                return True
            except psutil.NoSuchProcess:
                print(f"Process {process_name} not found.")
            except Exception as e:
                print(f"Failed to terminate process {process_name}: {e}")
    return False

def main():
    """Main function to monitor file size and kill the process if necessary."""
    while True:
        file_size_mb = get_file_size_mb(FILE_PATH)
        if file_size_mb > SIZE_LIMIT_MB:
            print(f"File {FILE_PATH} exceeded size limit ({file_size_mb:.2f} MB > {SIZE_LIMIT_MB} MB).")
            if kill_process_by_name(PROCESS_NAME):
                break  # Exit the loop after killing the process
        time.sleep(CHECK_INTERVAL)

if __name__ == "__main__":
    main()