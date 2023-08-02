#!/bin/bash

# Function to copy the json file and run the program using ssh and timeout
function run_program() {
  local host=$1
  local json_file=$2
  local program=$3
  local log_file=$4

  # Copy json file to the tmp directory on the host
  scp "$json_file" someos@"$host":/tmp/client.json

  # Run the program on the host for 10 seconds using timeout, collecting stdout and stderr into the log file
  ssh someos@"$host" "(timeout 10 $program) &> $log_file" &
}

# Function to collect log files from remote hosts
function collect_logs() {
  local host=$1
  local log_dir=$2

  # Wait for 10 seconds to ensure logs are generated
  sleep 10

  # Collect log files from the remote host to the log directory on the host
  scp someos@"$host":/tmp/stdout_stderr.log "$log_dir"/
}

# Main function to process each directory
function process_directory() {
  local test_dir=$1

  # Extract client.json and server.json paths
  client_json=$(find "$test_dir" -name "client.json" -type f)
  server_json=$(find "$test_dir" -name "server.json" -type f)

  # Extract client and server names
  client_name=$(basename "$test_dir")
  server_name="$client_name"_server

  # Create log directories for client and server
  client_log_dir="$test_dir"/"$client_name"_logs
  server_log_dir="$test_dir"/"$server_name"_logs
  mkdir -p "$client_log_dir"
  mkdir -p "$server_log_dir"

  # Process client
  run_program "$client_name" "$client_json" "xxx_client" "$client_log_dir"/stdout_stderr.log

  # Process server
  run_program "$server_name" "$server_json" "xxx_server" "$server_log_dir"/stdout_stderr.log

  # Wait for all background tasks to finish
  wait

  # Collect log files from client and server
  collect_logs "$client_name" "$client_log_dir"
  collect_logs "$server_name" "$server_log_dir"
}

# List of directories to process
directories=("path/to/dir1" "path/to/dir2" "path/to/dir3")

# Process each directory
for dir in "${directories[@]}"; do
  process_directory "$dir"
done
