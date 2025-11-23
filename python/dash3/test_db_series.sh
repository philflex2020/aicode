#!/bin/bash
# test_db_server.sh
# Comprehensive test script for db_server.py endpoints

# Configuration
SERVER_HOST="localhost"
SERVER_PORT="8084"
BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"

echo "=========================================="
echo "Testing db_server.py endpoints"
echo "Base URL: ${BASE_URL}"
echo "=========================================="
echo ""

# ==========================================
# Profile Management Tests
# ==========================================
echo "### 1. Listing all profiles ###"
curl -s -X GET "${BASE_URL}/api/profiles" | jq .
echo ""

echo "### 2. Getting active profile ###"
curl -s -X GET "${BASE_URL}/api/active_profile" | jq .
echo ""

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

echo "### 4. Selecting 'test-hatteras-0.3.35' as active profile ###"
curl -s -X POST "${BASE_URL}/api/profiles/select" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test-hatteras-0.3.35"
         }' | jq .
echo ""

echo "### 4b. Verifying active profile is now 'test-hatteras-0.3.35' ###"
curl -s -X GET "${BASE_URL}/api/active_profile" | jq .
echo ""

# ==========================================
# Variable Management Tests
# ==========================================
echo "### 5. Updating variables for active profile ('test-hatteras-0.3.35') ###"
curl -s -X POST "${BASE_URL}/vars" \
     -H "Content-Type: application/json" \
     -d '{
           "gain": "100",
           "phase": "90",
           "link_state": "UP"
         }' | jq .
echo ""

echo "### 6. Retrieving variables for active profile ('test-hatteras-0.3.35') ###"
curl -s -X GET "${BASE_URL}/vars?names=gain,phase,link_state,errors" | jq .
echo ""

# ==========================================
# Profile Cloning Tests
# ==========================================
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

echo "### 8. Listing all profiles again (should include cloned profile) ###"
curl -s -X GET "${BASE_URL}/api/profiles" | jq .
echo ""

echo "### 9. Selecting 'default' as active profile again ###"
curl -s -X POST "${BASE_URL}/api/profiles/select" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "default"
         }' | jq .
echo ""

# ==========================================
# Telemetry Series Discovery Tests
# ==========================================
echo "=========================================="
echo "Telemetry Series Tests"
echo "=========================================="
echo ""

echo "### 10. Discovery: List all available telemetry series ###"
curl -s "${BASE_URL}/series" | jq .
echo ""

echo "### 11. Discovery: Filter by category 'network' ###"
curl -s "${BASE_URL}/series?category=network" | jq .
echo ""

echo "### 12. Discovery: Filter by multiple categories 'network,system' ###"
curl -s "${BASE_URL}/series?category=network,system" | jq .
echo ""

echo "### 13. Discovery: Filter by category 'power' (cell data) ###"
curl -s "${BASE_URL}/series?category=power" | jq .
echo ""

echo "### 14. Discovery: Filter by category 'thermal' ###"
curl -s "${BASE_URL}/series?category=thermal" | jq .
echo ""

# ==========================================
# Telemetry Data Retrieval Tests
# ==========================================
echo "### 15. Data: Get scalar time-series (mbps, cpu) ###"
curl -s "${BASE_URL}/series?names=mbps,cpu&window=20" | jq .
echo ""

echo "### 16. Data: Get scalar time-series with category filter ###"
curl -s "${BASE_URL}/series?category=network&names=mbps,rtt_ms&window=10" | jq .
echo ""

# ==========================================
# Cell/Module Array Data Tests (NEW)
# ==========================================
echo "=========================================="
echo "Cell/Module Array Data Tests"
echo "=========================================="
echo ""

echo "### 17. Data: Get cell_volts array (480 values) ###"
curl -s "${BASE_URL}/series?names=cell_volts&window=10" | jq '.series.cell_volts[0][1] | length'
echo "Expected: 480"
echo ""

echo "### 18. Data: Get cell_volts array - first 10 values ###"
curl -s "${BASE_URL}/series?names=cell_volts&window=10" | jq '.series.cell_volts[0][1][0:10]'
echo ""

echo "### 19. Data: Get cell_temp array - first 10 values ###"
curl -s "${BASE_URL}/series?names=cell_temp&window=10" | jq '.series.cell_temp[0][1][0:10]'
echo ""

echo "### 20. Data: Get cell_soc array - first 10 values ###"
curl -s "${BASE_URL}/series?names=cell_soc&window=10" | jq '.series.cell_soc[0][1][0:10]'
echo ""

echo "### 21. Data: Get cell_soh array - first 10 values ###"
curl -s "${BASE_URL}/series?names=cell_soh&window=10" | jq '.series.cell_soh[0][1][0:10]'
echo ""

echo "### 22. Data: Get all cell metrics at once ###"
curl -s "${BASE_URL}/series?names=cell_volts,cell_temp,cell_soc,cell_soh&window=5" | jq 'keys'
echo ""

echo "### 23. Data: Verify cell_volts structure (timestamp + array) ###"
curl -s "${BASE_URL}/series?names=cell_volts&window=5" | jq '.series.cell_volts[0] | [.[0], (.[1] | type), (.[1] | length)]'
echo "Expected: [timestamp, \"array\", 480]"
echo ""

echo "### 24. Data: Get mixed scalar + array series ###"
curl -s "${BASE_URL}/series?names=mbps,cell_volts,temp_c,cell_temp&window=10" | jq 'keys'
echo ""

echo "### 25. Data: Cell volts with category filter (power) ###"
curl -s "${BASE_URL}/series?category=power&names=cell_volts&window=5" | jq '.series.cell_volts[0][1][0:5]'
echo ""

# ==========================================
# Telemetry Metadata Management Tests
# ==========================================
echo "=========================================="
echo "Telemetry Metadata Management Tests"
echo "=========================================="
echo ""

echo "### 26. Create new telemetry series metadata ###"
curl -s -X POST "${BASE_URL}/series" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test_voltage",
           "label": "Test Voltage",
           "unit": "V",
           "category": "power"
         }' | jq .
echo ""

echo "### 27. Verify new series appears in discovery ###"
curl -s "${BASE_URL}/series?category=power" | jq '.available[] | select(.name == "test_voltage")'
echo ""

echo "### 28. Update existing telemetry series metadata ###"
curl -s -X POST "${BASE_URL}/series" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test_voltage",
           "label": "Test Voltage (Updated)",
           "unit": "mV",
           "category": "power"
         }' | jq .
echo ""

# ==========================================
# Edge Cases and Error Handling
# ==========================================
echo "=========================================="
echo "Edge Cases and Error Handling"
echo "=========================================="
echo ""

echo "### 29. Request unknown series name ###"
curl -s "${BASE_URL}/series?names=unknown_metric&window=5" | jq .
echo "Expected: empty series object"
echo ""

echo "### 30. Request cell data with category mismatch ###"
curl -s "${BASE_URL}/series?category=network&names=cell_volts&window=5" | jq .
echo "Expected: empty series (cell_volts is in 'power' category)"
echo ""

echo "### 31. Mixed valid and invalid series names ###"
curl -s "${BASE_URL}/series?names=mbps,invalid_name,cell_volts&window=5" | jq 'keys'
echo "Expected: only mbps and cell_volts"
echo ""

# ==========================================
# Performance/Scale Tests
# ==========================================
echo "=========================================="
echo "Performance Tests"
echo "=========================================="
echo ""

echo "### 32. Request large window for cell data ###"
echo "Timing: cell_volts with window=300"
time curl -s "${BASE_URL}/series?names=cell_volts&window=300" > /dev/null
echo ""

echo "### 33. Request all cell metrics with large window ###"
echo "Timing: all cell metrics with window=100"
time curl -s "${BASE_URL}/series?names=cell_volts,cell_temp,cell_soc,cell_soh&window=100" > /dev/null
echo ""

# ==========================================
# Cleanup and Summary
# ==========================================
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo ""

echo "### 34. Final profile list ###"
curl -s -X GET "${BASE_URL}/api/profiles" | jq 'length'
echo "profiles created during test"
echo ""

echo "### 35. Final telemetry catalog count ###"
curl -s "${BASE_URL}/series" | jq '.available | length'
echo "telemetry series registered"
echo ""

echo "=========================================="
echo "All tests completed!"
echo "=========================================="
# ```

# ### Key additions for cell data testing:

# **Tests 17-25**: Cell/module array data
# - Verify array length (480 items)
# - Sample first 10 values from each metric
# - Test all cell metrics together
# - Verify data structure (timestamp + array)
# - Test mixed scalar + array queries
# - Test category filtering with cell data

# **Tests 26-28**: Metadata management
# - Create new series
# - Verify it appears in discovery
# - Update existing series

# **Tests 29-31**: Edge cases
# - Unknown series names
# - Category mismatches
# - Mixed valid/invalid names

# **Tests 32-33**: Performance
# - Large window requests
# - Multiple array metrics at once
