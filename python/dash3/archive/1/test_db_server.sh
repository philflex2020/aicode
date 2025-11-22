#!/bin/bash
# This is a `curl` test script for your `db_server.py`. This script will:

# 1.  **List existing profiles.**
# 2.  **Get the active profile.**
# 3.  **Create a new profile.**
# 4.  **Select the new profile as active.**
# 5.  **Update variables for the active profile.**
# 6.  **Retrieve variables for the active profile.**
# 7.  **Clone a profile.**
# 8.  **List all profiles again to see the changes.**

# Save this as `test_db_server.sh` and make it executable (`chmod +x test_db_server.sh`).

# ```bash

# Configuration
SERVER_HOST="localhost"
SERVER_PORT="8084"
BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"

echo "--- Testing db_server.py endpoints ---"
echo "Base URL: ${BASE_URL}"
echo ""

# 1. List existing profiles
echo "### 1. Listing all profiles ###"
curl -s -X GET "${BASE_URL}/api/profiles" | jq .
echo ""

# 2. Get the active profile
echo "### 2. Getting active profile ###"
curl -s -X GET "${BASE_URL}/api/active_profile" | jq .
echo ""

# 3. Create a new profile
echo "### 3. Creating a new profile: 'test-hatteras-0.3.35' ###"
curl -s -X POST "${BASE_URL}/api/profiles" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test-hatteras-0.3.35",
           "build": "hatteras",
           "version": "0.3.35",
           "target": "dev-install"
         }' | jq .
echo ""

# 4. Select the new profile as active
echo "### 4. Selecting 'test-hatteras-0.3.35' as active profile ###"
curl -s -X POST "${BASE_URL}/api/profiles/select" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test-hatteras-0.3.35"
         }' | jq .
echo ""

# Verify active profile changed
echo "### 4b. Verifying active profile is now 'test-hatteras-0.3.35' ###"
curl -s -X GET "${BASE_URL}/api/active_profile" | jq .
echo ""

# 5. Update variables for the active profile
echo "### 5. Updating variables for active profile ('test-hatteras-0.3.35') ###"
curl -s -X POST "${BASE_URL}/vars" \
     -H "Content-Type: application/json" \
     -d '{
           "gain": "100",
           "phase": "90",
           "link_state": "UP"
         }' | jq .
echo ""

# 6. Retrieve variables for the active profile
echo "### 6. Retrieving variables for active profile ('test-hatteras-0.3.35') ###"
curl -s -X GET "${BASE_URL}/vars?names=gain,phase,link_state,errors" | jq .
echo ""

# 7. Clone a profile
echo "### 7. Cloning 'test-hatteras-0.3.35' to 'test-warwick-0.3.36' ###"
curl -s -X POST "${BASE_URL}/api/profiles/clone" \
     -H "Content-Type: application/json" \
     -d '{
           "source_name": "test-hatteras-0.3.35",
           "new_name": "test-warwick-0.3.36",
           "new_version": "0.3.36",
           "new_target": "prod-install"
         }' | jq .
echo ""

# 8. List all profiles again to see the new cloned profile
echo "### 8. Listing all profiles again (should include cloned profile) ###"
curl -s -X GET "${BASE_URL}/api/profiles" | jq .
echo ""

# 9. Select the original default profile again
echo "### 9. Selecting 'default' as active profile again ###"
curl -s -X POST "${BASE_URL}/api/profiles/select" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "default"
         }' | jq .
echo ""


curl -s "http://127.0.0.1:8085/series" | jq .
curl -s "http://127.0.0.1:8085/series?category=network" | jq .
curl -s "http://127.0.0.1:8085/series?category=network,system" | jq .
curl -s "http://127.0.0.1:8085/series?names=mbps,cpu&window=20" | jq .
curl -s "http://127.0.0.1:8085/series?names=mbps,cpu&window=20"
curl -s "http://127.0.0.1:8085/series?category=network&names=mbps,cpu&window=20" | jq .
curl -s "http://127.0.0.1:8085/series?category=network&names=mbps,cpu&window=20"

echo "--- Test script finished ---"
# ```

# **Before running:**

# 1.  **Ensure `db_server.py` is running:**
#     ```bash
#     python3 db_server.py
#     ```
#     (It should be listening on `http://localhost:8081` by default).
# 2.  **Install `jq`:** This script uses `jq` to pretty-print the JSON output from `curl`. If you don't have it, install it:
#     -   On Debian/Ubuntu: `sudo apt-get install jq`
#     -   On macOS: `brew install jq`
#     -   On Windows (with Git Bash/WSL): `choco install jq` or `sudo apt-get install jq` in WSL.
# 3.  **Make the script executable:**
#     ```bash
#     chmod +x test_db_server.sh
#     ```

# **To run the test:**

# ```bash
# ./test_db_server.sh
# ```

# You should see a sequence of `curl` commands and their JSON responses, demonstrating the creation, selection, variable updates, and cloning of profiles. 
# The `rbms.db` file will be created/updated in the same directory where `db_server.py` is run.
