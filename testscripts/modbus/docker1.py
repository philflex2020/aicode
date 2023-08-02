import os
import time
import shutil
import docker
import subprocess

# Function to run the program in the Docker container and collect stdout and stderr to log file
def run_program_docker(container_name, json_file, program, log_file):
    command = f"docker cp {json_file} {container_name}:/tmp/client.json"
    subprocess.run(command, shell=True, check=True)

    command = f"docker exec -d {container_name} timeout 10 {program} &> {log_file}"
    subprocess.run(command, shell=True, check=True)

# Function to collect log files from Docker container
def collect_logs_docker(container_name, log_dir):
    time.sleep(10)  # Wait for 10 seconds to ensure logs are generated

    command = f"docker cp {container_name}:/tmp/stdout_stderr.log {os.path.join(log_dir, 'stdout_stderr.log')}"
    subprocess.run(command, shell=True, check=True)

# Main function to process each directory
def process_directory(test_dir, client_json, server_json, client_container, server_container):
    # Extract client and server names
    client_name = os.path.basename(test_dir)
    server_name = f"{client_name}_server"

    # Create log directories for client and server
    client_log_dir = os.path.join(test_dir, f"{client_name}_logs")
    server_log_dir = os.path.join(test_dir, f"{server_name}_logs")
    os.makedirs(client_log_dir, exist_ok=True)
    os.makedirs(server_log_dir, exist_ok=True)

    # Process client
    run_program_docker(client_container, client_json, "xxx_client", os.path.join(client_log_dir, "stdout_stderr.log"))

    # Process server
    run_program_docker(server_container, server_json, "xxx_server", os.path.join(server_log_dir, "stdout_stderr.log"))

    # Wait for both processes to finish and collect log files
    collect_logs_docker(client_container, client_log_dir)
    collect_logs_docker(server_container, server_log_dir)

if __name__ == "__main__":
    # List of directories to process
    directories = ["path/to/dir1", "path/to/dir2", "path/to/dir3"]

    # Client and Server container names
    client_container_name = "client_container_name"
    server_container_name = "server_container_name"

    for dir_path in directories:
        client_json_path = os.path.join(dir_path, "client.json")
        server_json_path = os.path.join(dir_path, "server.json")

        if os.path.exists(client_json_path) and os.path.exists(server_json_path):
            process_directory(dir_path, client_json_path, server_json_path, client_container_name, server_container_name)
