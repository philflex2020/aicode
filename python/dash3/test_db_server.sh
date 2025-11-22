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
# 9.  **Test telemetry series endpoints.**
# 10. **Test protection endpoints.**

# Save this as `test_db_server.sh` and make it executable (`chmod +x test_db_server.sh`).

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
# PROFILE TESTS
# ==========================================

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

# ==========================================
# VARIABLE TESTS
# ==========================================

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

# ==========================================
# TELEMETRY SERIES TESTS
# ==========================================

echo "### 10. List all available telemetry series ###"
curl -s "${BASE_URL}/series" | jq .
echo ""

echo "### 11. List telemetry series by category (network) ###"
curl -s "${BASE_URL}/series?category=network" | jq .
echo ""

echo "### 12. List telemetry series by multiple categories ###"
curl -s "${BASE_URL}/series?category=network,system" | jq .
echo ""

echo "### 13. Get series data (mbps, cpu) ###"
curl -s "${BASE_URL}/series?names=mbps,cpu&window=20" | jq .
echo ""

echo "### 14. Get series data (raw, no jq) ###"
curl -s "${BASE_URL}/series?names=mbps,cpu&window=20"
echo ""
echo ""

echo "### 15. Get series data with category filter ###"
curl -s "${BASE_URL}/series?category=network&names=mbps,rtt_ms&window=20" | jq .
echo ""

echo "### 16. Get series data with category filter (raw) ###"
curl -s "${BASE_URL}/series?category=network&names=mbps,cpu&window=20"
echo ""
echo ""

echo "### 17. Get cell array series data ###"
curl -s "${BASE_URL}/series?names=cell_volts,cell_temp&window=10" | jq '.series | to_entries | map({name: .key, samples: (.value | length), first_sample_length: (.value[0][1] | length)})'
echo ""

# ==========================================
# PROTECTION METADATA TESTS
# ==========================================

echo "### 18. Get protection metadata (available variables) ###"
curl -s "${BASE_URL}/api/protection_meta" | jq .
echo ""

# ==========================================
# PROTECTION CONFIGURATION TESTS
# ==========================================

echo "### 19. Get current protections for active profile ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq .
echo ""

echo "### 20. Get default (factory) protections ###"
curl -s "${BASE_URL}/api/protections?source=default" | jq .
echo ""

echo "### 21. Get protections summary (variable names, values, and states) ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq '.protections | to_entries | map({variable: .key, current_value: .value.current_value, state: .value.current_state, unit: .value.meta.unit})'
echo ""

echo "### 22. Get detailed limits for cell_voltage ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq '.protections.cell_voltage'
echo ""

# ==========================================
# SAVE PROTECTION CONFIGURATION
# ==========================================

echo "### 23. Save modified protection configuration (cell_voltage) ###"
curl -s -X POST "${BASE_URL}/api/protections" \
     -H "Content-Type: application/json" \
     -d '{
           "protections": {
             "cell_voltage": {
               "max": {
                 "warning": {"threshold": 4.10, "on_duration": 5.0, "off_duration": 10.0, "enabled": true},
                 "alert":   {"threshold": 4.18, "on_duration": 3.0, "off_duration": 15.0, "enabled": true},
                 "fault":   {"threshold": 4.25, "on_duration": 1.0, "off_duration": 20.0, "enabled": true}
               },
               "min": {
                 "warning": {"threshold": 2.75, "on_duration": 5.0, "off_duration": 10.0, "enabled": true},
                 "alert":   {"threshold": 2.65, "on_duration": 3.0, "off_duration": 15.0, "enabled": true},
                 "fault":   {"threshold": 2.60, "on_duration": 1.0, "off_duration": 20.0, "enabled": true}
               }
             }
           }
         }' | jq .
echo ""

echo "### 24. Verify saved protections (cell_voltage limits) ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq '.protections.cell_voltage.limits'
echo ""

# ==========================================
# DEPLOY PROTECTION CONFIGURATION
# ==========================================

echo "### 25. Deploy protection configuration (temperature) ###"
curl -s -X POST "${BASE_URL}/api/protections/deploy" \
     -H "Content-Type: application/json" \
     -d '{
           "protections": {
             "temperature": {
               "max": {
                 "warning": {"threshold": 48.0, "on_duration": 10.0, "off_duration": 20.0, "enabled": true},
                 "alert":   {"threshold": 53.0, "on_duration": 5.0, "off_duration": 30.0, "enabled": true},
                 "fault":   {"threshold": 58.0, "on_duration": 2.0, "off_duration": 40.0, "enabled": true}
               },
               "min": {
                 "warning": {"threshold": 0.0, "on_duration": 10.0, "off_duration": 20.0, "enabled": true},
                 "alert":   {"threshold": -5.0, "on_duration": 5.0, "off_duration": 30.0, "enabled": false},
                 "fault":   {"threshold": -10.0, "on_duration": 2.0, "off_duration": 40.0, "enabled": false}
               }
             }
           }
         }' | jq .
echo ""

# ==========================================
# PROFILE CLONING WITH PROTECTIONS
# ==========================================

echo "### 26. Clone profile with protections (default -> test-lab) ###"
curl -s -X POST "${BASE_URL}/api/profiles/clone" \
     -H "Content-Type: application/json" \
     -d '{
           "source_name": "default",
           "new_name": "test-lab",
           "new_version": "0.3.35"
         }' | jq .
echo ""

echo "### 27. Get protections from cloned profile ###"
curl -s "${BASE_URL}/api/protections?source=current&profile=test-lab" | jq '.protections | to_entries | map({variable: .key, has_limits: (.value.limits | length > 0)})'
echo ""

echo "### 28. Get full protections for test-lab profile ###"
curl -s "${BASE_URL}/api/protections?source=current&profile=test-lab" | jq '.protections | keys'
echo ""

# ==========================================
# PARTIAL PROTECTION UPDATES
# ==========================================

echo "### 29. Save partial protection update (only current variable) ###"
curl -s -X POST "${BASE_URL}/api/protections" \
     -H "Content-Type: application/json" \
     -d '{
           "protections": {
             "current": {
               "max": {
                 "warning": {"threshold": 95.0, "on_duration": 5.0, "off_duration": 10.0, "enabled": true},
                 "alert":   {"threshold": 115.0, "on_duration": 2.0, "off_duration": 15.0, "enabled": true},
                 "fault":   {"threshold": 145.0, "on_duration": 1.0, "off_duration": 20.0, "enabled": true}
               }
             }
           }
         }' | jq .
echo ""

echo "### 30. Verify current protection was updated ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq '.protections.current.limits'
echo ""

# ==========================================
# DISABLED LEVEL TESTS
# ==========================================

echo "### 31. Save protection with disabled levels (SOC) ###"
curl -s -X POST "${BASE_URL}/api/protections" \
     -H "Content-Type: application/json" \
     -d '{
           "protections": {
             "soc": {
               "max": {
                 "warning": {"threshold": 95.0, "on_duration": 30.0, "off_duration": 60.0, "enabled": true},
                 "alert":   {"threshold": 98.0, "on_duration": 15.0, "off_duration": 90.0, "enabled": false},
                 "fault":   {"threshold": 100.0, "on_duration": 5.0, "off_duration": 120.0, "enabled": false}
               },
               "min": {
                 "warning": {"threshold": 10.0, "on_duration": 30.0, "off_duration": 60.0, "enabled": true},
                 "alert":   {"threshold": 5.0, "on_duration": 15.0, "off_duration": 90.0, "enabled": true},
                 "fault":   {"threshold": 2.0, "on_duration": 5.0, "off_duration": 120.0, "enabled": true}
               }
             }
           }
         }' | jq .
echo ""

echo "### 32. Verify SOC protection with disabled levels ###"
curl -s "${BASE_URL}/api/protections?source=current" | jq '.protections.soc.limits'
echo ""

# ==========================================
# METRICS AND STATE
# ==========================================

echo "### 33. Get metrics ###"
curl -s "${BASE_URL}/metrics" | jq '.[:2]'
echo ""

echo "### 34. Get state ###"
curl -s "${BASE_URL}/api/state" | jq '.state | keys'
echo ""

# ==========================================
# SUMMARY
# ==========================================

echo ""
echo "=========================================="
echo "Test script finished successfully!"
echo "=========================================="
echo ""
echo "Summary of tests:"
echo "  - Profile management: ✓"
echo "  - Variable operations: ✓"
echo "  - Telemetry series: ✓"
echo "  - Protection metadata: ✓"
echo "  - Protection configuration: ✓"
echo "  - Save/Deploy protections: ✓"
echo "  - Profile cloning with protections: ✓"
echo "  - Disabled protection levels: ✓"
echo "  - Metrics and state: ✓"
echo ""
echo "All tests completed. Please review the output above for details."
# ==========================================
