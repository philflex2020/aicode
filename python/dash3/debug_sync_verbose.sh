#!/bin/bash

# Kill any existing instances
pkill -f "db_variables_api.py"
pkill -f "db_sync_client.py"
sleep 2

# Clean up old databases
rm -f primary.db secondary.db

echo "=========================================="
echo "Variable Registry Sync Test (Verbose)"
echo "=========================================="

# Start primary server (port 8902)
echo "Starting primary server on port 8902..."
python3 db_variables_api.py --db primary.db --config data_def.json --metadata var_metadata.json --port 8902 > primary.log 2>&1 &
PRIMARY_PID=$!
echo "Primary PID: $PRIMARY_PID"
sleep 3

# Start secondary server (port 8903)
echo "Starting secondary server on port 8903..."
python3 db_variables_api.py --db secondary.db --port 8903 > secondary.log 2>&1 &
SECONDARY_PID=$!
echo "Secondary PID: $SECONDARY_PID"
sleep 3

# Check if servers are running
echo -e "\nChecking servers..."
if curl -s http://localhost:8902/api/health > /dev/null; then
    echo "✓ Primary server is running"
else
    echo "✗ Primary server failed to start"
    cat primary.log
    exit 1
fi

if curl -s http://localhost:8903/api/health > /dev/null; then
    echo "✓ Secondary server is running"
else
    echo "✗ Secondary server failed to start"
    cat secondary.log
    exit 1
fi

# Start sync client
echo -e "\nStarting sync client..."
python3 db_sync_client.py --primary http://localhost:8902 --secondary http://localhost:8903 > sync.log 2>&1 &
SYNC_PID=$!
echo "Sync PID: $SYNC_PID"
sleep 3

# Check if sync client is running
if ps -p $SYNC_PID > /dev/null; then
    echo "✓ Sync client is running"
    echo -e "\nSync client output:"
    head -20 sync.log
else
    echo "✗ Sync client failed to start"
    cat sync.log
    exit 1
fi

echo ""
echo "=========================================="
echo "✓ All services started!"
echo "=========================================="
echo "Primary server:   http://localhost:8902"
echo "Secondary server: http://localhost:8903"
echo "Monitor UI:       Open sync_monitor.html in browser"
echo ""
echo "Logs:"
echo "  Primary:   tail -f primary.log"
echo "  Secondary: tail -f secondary.log"
echo "  Sync:      tail -f sync.log"
echo ""
echo "Press Ctrl+C to stop all services"
echo "=========================================="

# Wait for Ctrl+C
trap "echo ''; echo 'Stopping services...'; kill $PRIMARY_PID $SECONDARY_PID $SYNC_PID 2>/dev/null; exit" INT
wait
