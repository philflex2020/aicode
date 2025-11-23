#!/bin/bash
# test_protections.sh - Quick test for protections endpoints

BASE_URL="http://127.0.0.1:8084"

echo "=========================================="
echo "Testing Protections Endpoints"
echo "=========================================="
echo ""

# Test 1: Get protection metadata
echo "### 1. Get protection metadata ###"
curl -s "${BASE_URL}/api/protection_meta" | jq .
echo ""

# Test 2: Get current protections
echo "### 2. Get current protections ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq .
echo ""

# Test 3: Get default protections
echo "### 3. Get default protections ###"
curl -s "${BASE_URL}/api/protections?source=default" | jq .
echo ""

# Test 4: Get just the protection keys
echo "### 4. List protection variable names ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq '.protections | keys'
echo ""

# Test 5: Get summary (names, values, states)
echo "### 5. Get protection summary ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq '.protections | to_entries | map({variable: .key, value: .value.current_value, state: .value.current_state, unit: .value.meta.unit})'
echo ""

# Test 6: Get one specific protection (cell_voltage)
echo "### 6. Get cell_voltage protection details ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq '.protections.cell_voltage'
echo ""

echo "=========================================="
echo "Tests complete!"
echo "=========================================="
