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

# Function to run the check_logs script and get the test result
def run_check_logs(log_dir):
    command = f"python check_logs.py {os.path.join(log_dir, 'stdout_stderr.log')}"
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    return result.stdout.strip()

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

    # Run check_logs script and get the test results
    client_result = run_check_logs(client_log_dir)
    server_result = run_check_logs(server_log_dir)

    return client_name, client_result, server_name, server_result

def generate_md_table(test_results):
    md_content = "| Test | Client Result | Server Result |\n"
    md_content += "|------|---------------|---------------|\n"
    for client_name, client_result, server_name, server_result in test_results:
        md_content += f"| {client_name} | {client_result} | {server_result} |\n"
    return md_content

if __name__ == "__main__":
    # List of directories to process
    directories = ["path/to/dir1", "path/to/dir2", "path/to/dir3"]

    # Client and Server container names
    client_container_name = "client_container_name"
    server_container_name = "server_container_name"

    test_results = []
    for dir_path in directories:
        client_json_path = os.path.join(dir_path, "client.json")
        server_json_path = os.path.join(dir_path, "server.json")

        if os.path.exists(client_json_path) and os.path.exists(server_json_path):
            result = process_directory(dir_path, client_json_path, server_json_path, client_container_name, server_container_name)
            test_results.append(result)

    # Generate MD table content
    md_table = generate_md_table(test_results)

    # Check if all tests passed
    all_tests_passed = all(result[1] == "pass" and result[3] == "pass" for result in test_results)

    # Generate summary
    summary = "All tests passed!" if all_tests_passed else "Some tests failed."

    # Write the MD content to a file
    with open("test_results.md", "w") as md_file:
        md_file.write(md_table)

    print(summary)
