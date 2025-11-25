#!/bin/bash
set -e  # Exit on error

# ==========================================
# RBMS Dashboard Startup Script
# ==========================================

# Configuration
VARS_PORT=8902
DB_PORT=8084
MAIN_PORT=8081
STARTUP_DELAY=2
HEALTH_TIMEOUT=10

# Log files
VARS_LOG="variables.log"
DB_LOG="db_server.log"
MAIN_LOG="main.log"
SYNC_LOG="sync.log"

# PID tracking
PIDS=()

# ==========================================
# Helper Functions
# ==========================================

log_info() {
    echo "ℹ $1"
}

log_success() {
    echo "✓ $1"
}

log_error() {
    echo "✗ $1" >&2
}

log_section() {
    echo ""
    echo "=========================================="
    echo "$1"
    echo "=========================================="
}

cleanup() {
    log_section "Stopping all services..."
    for pid in "${PIDS[@]}"; do
        if ps -p "$pid" > /dev/null 2>&1; then
            kill "$pid" 2>/dev/null || true
        fi
    done
    wait 2>/dev/null || true
    log_success "All services stopped"
    exit 0
}

wait_for_health() {
    local url=$1
    local name=$2
    local timeout=$3
    local elapsed=0
    
    while [ $elapsed -lt $timeout ]; do
        if curl -sf "$url" > /dev/null 2>&1; then
            log_success "$name is healthy"
            return 0
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    
    log_error "$name failed to start (timeout after ${timeout}s)"
    return 1
}

start_service() {
    local name=$1
    local cmd=$2
    local logfile=$3
    local port=$4
    
    log_info "Starting $name on port $port..."
    $cmd > "$logfile" 2>&1 &
    local pid=$!
    PIDS+=("$pid")
    
    sleep $STARTUP_DELAY
    
    if ! ps -p "$pid" > /dev/null 2>&1; then
        log_error "$name failed to start (process died)"
        log_error "Last 10 lines of $logfile:"
        tail -10 "$logfile" >&2
        return 1
    fi
    
    log_success "$name started (PID: $pid)"
    return 0
}

# ==========================================
# Cleanup existing instances
# ==========================================

log_section "Cleaning up existing instances"

pkill -f "db_variables_api.py" 2>/dev/null || true
pkill -f "db_server_prot.py" 2>/dev/null || true
pkill -f "main_fixed.py" 2>/dev/null || true
pkill -f "db_sync_client.py" 2>/dev/null || true

sleep 2
log_success "Cleanup complete"

# Optional: Clean databases (uncomment if needed)
# log_info "Removing old databases..."
# rm -f primary.db secondary.db rbms.db

# ==========================================
# Start Services
# ==========================================

log_section "Starting RBMS Dashboard Services"

# 1. Variables Server
start_service \
    "Variables Server" \
    "python3 db_variables_api.py --db primary.db --config data_def.json --metadata var_metadata.json --port $VARS_PORT" \
    "$VARS_LOG" \
    "$VARS_PORT" || exit 1

wait_for_health "http://localhost:$VARS_PORT/api/health" "Variables Server" $HEALTH_TIMEOUT || exit 1

# 2. DB Server (Protections)
start_service \
    "DB Server" \
    "python3 db_server_prot.py --port $DB_PORT" \
    "$DB_LOG" \
    "$DB_PORT" || exit 1

wait_for_health "http://localhost:$DB_PORT/api/profiles" "DB Server" $HEALTH_TIMEOUT || exit 1

# 3. Frontend (Main Dashboard)
# start_service \
#     "Frontend Server" \
#     "python3 main_fixed.py --port $MAIN_PORT --db-url http://localhost:$DB_PORT --var-url http://localhost:$VARS_PORT" \
#     "$MAIN_LOG" \
#     "$MAIN_PORT" || exit 1
start_service \
    "Frontend Server" \
    "python3 main_fixed.py --port $MAIN_PORT " \
    "$MAIN_LOG" \
    "$MAIN_PORT" || exit 1

wait_for_health "http://localhost:$MAIN_PORT/" "Frontend Server" $HEALTH_TIMEOUT || exit 1

# 4. Sync Client (Optional - only if you have a secondary server)
# Uncomment if you need sync functionality
# log_info "Starting sync client..."
# python3 db_sync_client.py --primary http://localhost:$VARS_PORT --secondary http://localhost:8903 > "$SYNC_LOG" 2>&1 &
# SYNC_PID=$!
# PIDS+=("$SYNC_PID")
# sleep $STARTUP_DELAY
# if ps -p "$SYNC_PID" > /dev/null 2>&1; then
#     log_success "Sync client started (PID: $SYNC_PID)"
# else
#     log_error "Sync client failed to start"
#     tail -20 "$SYNC_LOG"
# fi

# ==========================================
# Summary
# ==========================================

log_section "All Services Running"

echo "Services:"
echo "  • Variables Server:  http://localhost:$VARS_PORT"
echo "  • DB Server:         http://localhost:$DB_PORT"
echo "  • Frontend:          http://localhost:$MAIN_PORT"
echo ""
echo "Logs:"
echo "  • Variables:  tail -f $VARS_LOG"
echo "  • DB Server:  tail -f $DB_LOG"
echo "  • Frontend:   tail -f $MAIN_LOG"
# echo "  • Sync:       tail -f $SYNC_LOG"
echo ""
echo "Press Ctrl+C to stop all services"

log_section "Ready"

# ==========================================
# Wait for interrupt
# ==========================================

trap cleanup INT TERM

# Keep script running
wait