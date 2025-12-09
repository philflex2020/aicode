#!/bin/bash

# Get name extension from argument, default to timestamp if not provided
NAME_EXT="${1:-$(date +%s)}"

echo "Creating variable: test_manual_sync${NAME_EXT}"
echo "=========================================="

curl -X POST http://localhost:8902/api/variables \
  -H "Content-Type: application/json" \
  -d '{
    "name": "test_manual_sync'"${NAME_EXT}"'",
    "display_name": "Test Manual Sync '"${NAME_EXT}"'",
    "category": "oper",
    "locator": "system.global"
  }' | jq '.'

echo ""
echo "=========================================="
echo "Waiting 2 seconds for sync..."
sleep 2

echo ""
echo "Primary system version:"
curl -s http://localhost:8902/api/system/version | jq '.'

echo ""
echo "Secondary system version:"
curl -s http://localhost:8903/api/system/version | jq '.'

echo ""
echo "Checking if variable exists on secondary:"
curl -s http://localhost:8903/api/variables | jq '.[] | select(.name=="test_manual_sync'"${NAME_EXT}"'")'

echo ""
echo "=========================================="

