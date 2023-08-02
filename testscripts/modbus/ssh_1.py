import os
import time
import shutil
import paramiko
import docker

# Function to run the program on the remote host using SSH and timeout
def run_program_ssh(host, username, key_filename, json_file, program, log_file):
    ssh_client = paramiko.SSHClient()
    ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh_client.connect(hostname=host, username=username, key_filename=key_filename)

    # Copy json file to the tmp directory on the host
    sftp = ssh_client.open_sftp()
    sftp.put(json_file, '/tmp/client.json')
    sftp.close()

    # Run the program on the host for 10 seconds using timeout, collecting stdout and stderr into the log file
    command = f"(timeout 10 {program}) &> {log_file}"
    ssh_client.exec_command(command)
    ssh_client.close()

# Function to collect log files from remote host
def collect_logs(host, username, key_filename, log_dir):
    time.sleep(10)  # Wait for 10 seconds to ensure logs are generated

    ssh_client = paramiko.SSHClient()
    ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh_client.connect(hostname=host, username=username, key_filename=key_filename)

    # Collect log file from the remote host to the log directory on the host
    sftp = ssh_client.open_sftp()
    sftp.get('/tmp/stdout_stderr.log', os.path.join(log_dir, 'stdout_stderr.log'))
    sftp.close()
    ssh_client.close()

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
    run_program_ssh(client_container, "root", "path/to/ssh_key.pem", client_json, "xxx_client", os.path.join(client_log_dir, "stdout_stderr.log"))

    # Process server
    run_program_ssh(server_container, "root", "path/to/ssh_key.pem", server_json, "xxx_server", os.path.join(server_log_dir, "stdout_stderr.log"))

    # Wait for both processes to finish and collect log files
    collect_logs(client_container, "root", "path/to/ssh_key.pem", client_log_dir)
    collect_logs(server_container, "root", "path/to/ssh_key.pem", server_log_dir)

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
