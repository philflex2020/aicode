#!/bin/bash

# test_sync.sh - Test the sync system




## 5. **Run the Demo**

# # Make script executable
# chmod +x test_sync.sh

# # Run the test
# ./test_sync.sh

# # In another terminal, open the monitor
# open sync_monitor.html  # macOS
# # or
# xdg-open sync_monitor.html  # Linux
# # or just open it in your browser
# ```

# ---

# ## What You'll See:

# 1. **Primary Server** (port 8902) - loaded with your data_def.json
# 2. **Secondary Server** (port 8903) - starts empty
# 3. **Sync Client** - automatically syncs all variables from primary to secondary
# 4. **Web Monitor** - shows real-time activity:
#    - Create a variable on primary → see it appear on secondary instantly
#    - Color-coded events (green=created, yellow=updated, red=deleted)
#    - Live statistics for both servers
#    - Activity feed with timestamps

#Try creating variables through the web UI and watch them sync in real-time! 🚀
echo "=========================================="
echo "Variable Registry Sync Test"
echo "=========================================="

# Start primary server (port 8902)
echo "Starting primary server on port 8902..."
python3 db_variables_api.py --db primary.db --config data_def.json --metadata var_metadata.json --port 8902 &
PRIMARY_PID=$!
sleep 3

# Start secondary server (port 8903)
echo "Starting secondary server on port 8903..."
python3 db_variables_api.py --db secondary.db --port 8903 &
SECONDARY_PID=$!
sleep 3

# Start sync client
echo "Starting sync client..."
python3 db_sync_client.py --primary http://localhost:8902 --secondary http://localhost:8903 &
SYNC_PID=$!
sleep 2

echo ""
echo "=========================================="
echo "✓ All services started!"
echo "=========================================="
echo "Primary server:   http://localhost:8902"
echo "Secondary server: http://localhost:8903"
echo "Monitor UI:       Open sync_monitor.html in browser"
echo ""
echo "Press Ctrl+C to stop all services"
echo "=========================================="

# Wait for Ctrl+C
trap "echo ''; echo 'Stopping services...'; kill $PRIMARY_PID $SECONDARY_PID $SYNC_PID 2>/dev/null; exit" INT
wait