#!/bin/bash

echo "=========================================="
echo "Sync Debug Check"
echo "=========================================="

echo -e "\n1. Checking Primary Server..."
curl -s http://localhost:8902/api/health | jq '.'

echo -e "\n2. Checking Secondary Server..."
curl -s http://localhost:8903/api/health | jq '.'

echo -e "\n3. Creating test variable on primary..."
curl -X POST http://localhost:8902/api/variables \
  -H "Content-Type: application/json" \
  -d '{
    "name": "debug_test_'$(date +%s)'",
    "display_name": "Debug Test",
    "category": "oper",
    "locator": "system.global"
  }' | jq '.'

echo -e "\n4. Waiting 3 seconds for sync..."
sleep 3

echo -e "\n5. Checking Primary variables..."
curl -s http://localhost:8902/api/variables | jq 'length'

echo -e "\n6. Checking Secondary variables..."
curl -s http://localhost:8903/api/variables | jq 'length'

echo -e "\n=========================================="

