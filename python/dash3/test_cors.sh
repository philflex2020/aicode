#!/bin/bash

echo "=== Testing CORS Headers ==="
echo ""

echo "Testing Primary Server (8902):"
curl -i -X OPTIONS http://192.168.86.51:8902/api/variables \
  -H "Origin: http://192.168.86.51:8080" \
  -H "Access-Control-Request-Method: GET" \
  -H "Access-Control-Request-Headers: Content-Type" 2>&1 | grep -i "access-control"

echo ""
echo "Testing Secondary Server (8903):"
curl -i -X OPTIONS http://192.168.86.51:8903/api/variables \
  -H "Origin: http://192.168.86.51:8080" \
  -H "Access-Control-Request-Method: GET" \
  -H "Access-Control-Request-Headers: Content-Type" 2>&1 | grep -i "access-control"

echo ""
echo "=== Testing Actual GET Requests ==="
echo ""

echo "Primary Server:"
curl -i http://192.168.86.51:8902/api/variables 2>&1 | head -20

echo ""
echo "Secondary Server:"
curl -i http://192.168.86.51:8903/api/variables 2>&1 | head -20
