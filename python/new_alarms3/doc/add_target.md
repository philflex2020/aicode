Perfect! Now I'll give you **updated versions** with:

1. **New "Target" tab** in `index.html` with:
   - Connection config (IP, port, secure)
   - A **variable table** where you can type var names / shared mem refs and get/set values
2. **New endpoints** in `api_endpoints.py`:
   - `GET /api/alarms/target/config`
   - `POST /api/alarms/target/config`
   - `POST /api/alarms/target/get`
   - `POST /api/alarms/target/set`
3. **New file** `target_client.py` (WebSocket client wrapper)

---

## Updated Files

### 1. `target_client.py` (NEW)

Create this file in your project root:

```python
# target_client.py
import json
import websocket
import threading
import time

class TargetClient:
    """WebSocket client for communicating with embedded target"""
    
    def __init__(self, ip, port, secure=False):
        self.ip = ip
        self.port = port
        self.secure = secure
        self.ws = None
        self.seq_counter = 0
        self.pending_responses = {}  # seq -> response
        self.lock = threading.Lock()
        self.connected = False
        self.ws_thread = None

    def connect(self):
        """Connect to target WebSocket server"""
        protocol = "wss" if self.secure else "ws"
        url = f"{protocol}://{self.ip}:{self.port}"
        
        self.ws = websocket.WebSocketApp(
            url,
            on_message=self._on_message,
            on_error=self._on_error,
            on_close=self._on_close,
            on_open=self._on_open
        )
        
        # Run in background thread
        self.ws_thread = threading.Thread(target=self.ws.run_forever)
        self.ws_thread.daemon = True
        self.ws_thread.start()
        
        # Wait for connection (up to 5 seconds)
        for _ in range(50):
            if self.connected:
                print(f"[TargetClient] Connected to {url}")
                return True
            time.sleep(0.1)
        
        print(f"[TargetClient] Failed to connect to {url}")
        return False

    def _on_open(self, ws):
        self.connected = True

    def _on_message(self, ws, message):
        try:
            resp = json.loads(message)
            seq = resp.get("seq")
            if seq is not None:
                with self.lock:
                    self.pending_responses[seq] = resp
        except Exception as e:
            print(f"[TargetClient] Error parsing message: {e}")

    def _on_error(self, ws, error):
        print(f"[TargetClient] WebSocket error: {error}")

    def _on_close(self, ws, close_status_code, close_msg):
        self.connected = False
        print(f"[TargetClient] Connection closed: {close_status_code} {close_msg}")

    def send_command(self, action, sm_name, reg_type, offset, num=None, data=None, timeout=5):
        """Send a command and wait for response"""
        if not self.connected:
            raise Exception("Not connected to target")

        with self.lock:
            self.seq_counter += 1
            seq = self.seq_counter

        cmd = {
            "action": action,
            "seq": seq,
            "sm_name": sm_name,
            "reg_type": reg_type,
            "offset": str(offset)
        }
        if num is not None:
            cmd["num"] = num
        if data is not None:
            cmd["data"] = data

        self.ws.send(json.dumps(cmd))

        # Wait for response
        start = time.time()
        while time.time() - start < timeout:
            with self.lock:
                if seq in self.pending_responses:
                    resp = self.pending_responses.pop(seq)
                    return resp
            time.sleep(0.05)

        raise TimeoutError(f"No response for seq {seq} within {timeout}s")

    def get_data(self, sm_name, reg_type, offset, num):
        """Get data from target"""
        return self.send_command("get", sm_name, reg_type, offset, num=num)

    def set_data(self, sm_name, reg_type, offset, data):
        """Set data on target"""
        return self.send_command("set", sm_name, reg_type, offset, data=data)

    def close(self):
        """Close connection"""
        if self.ws:
            self.ws.close()
        self.connected = False
```

---

### 2. `api_endpoints.py` (UPDATED)

Add these imports at the top and new endpoints at the bottom:

```python
# api_endpoints.py
import os
import json
from datetime import datetime
from flask import request
from flask_restx import Namespace, Resource, fields
from extensions import db
from alarm_models import AlarmDefinition, AlarmLevelAction, LimitsValues, DataDefinition
from alarm_utils import (
    load_alarm_defs_from_csv,
    export_alarm_definitions_to_csv,
    export_alarm_level_actions_to_csv,
    export_limits_values_to_csv,
    ensure_all_alarms_have_records,
    normalize_comparison_type,
    CSV_ALARM_DEF_FILE
)
from target_client import TargetClient

ns = Namespace('alarms', description='Alarm management operations')

# --- Flask-RESTX Models for API documentation ---
alarm_def_model = ns.model('AlarmDefinition', {
    'alarm_num': fields.Integer(required=True, description='Alarm number (primary key)'),
    'alarm_name': fields.String(required=True, description='Alarm name'),
    'num_levels': fields.Integer(required=True, description='Number of alarm levels (1-3)'),
    'measured_variable': fields.String(description='Measured variable reference'),
    'limits_structure': fields.String(description='Limits structure reference'),
    'comparison_type': fields.String(
        description='Comparison type',
        enum=['greater_than', 'less_than', 'equal', 'not_equal', 'greater_or_equal', 'less_or_equal'],
        default='greater_than'
    ),
    'alarm_variable': fields.String(description='Alarm variable reference'),
    'latched_variable': fields.String(description='Latched variable reference'),
    'notes': fields.String(description='Notes'),
})

alarm_level_action_model = ns.model('AlarmLevelAction', {
    'alarm_num': fields.Integer(required=True, description='Alarm number (FK)'),
    'level': fields.Integer(required=True, description='Alarm level (1-3)'),
    'enabled': fields.Boolean(description='Whether this level is enabled'),
    'duration': fields.Integer(description='Duration in seconds'),
    'actions': fields.String(description='Actions to take'),
    'notes': fields.String(description='Notes'),
    'alarm_name': fields.String(description='Alarm name (for display)'),
})

limits_value_model = ns.model('LimitsValue', {
    'limits_structure': fields.String(required=True, description='Limits structure name (primary key)'),
    'level1_limit': fields.Integer(description='Level 1 limit'),
    'level2_limit': fields.Integer(description='Level 2 limit'),
    'level3_limit': fields.Integer(description='Level 3 limit'),
    'hysteresis': fields.Integer(description='Hysteresis value'),
    'last_updated': fields.String(description='Last updated timestamp'),
    'notes': fields.String(description='Notes'),
    'alarm_name': fields.String(description='Alarm name (for display)'),
})

alarm_config_model = ns.model('AlarmConfig', {
    'alarm_definitions': fields.List(fields.Nested(alarm_def_model)),
    'alarm_level_actions': fields.List(fields.Nested(alarm_level_action_model)),
    'limits_values': fields.List(fields.Nested(limits_value_model)),
})

csv_operation_model = ns.model('CSVOperation', {
    'directory': fields.String(required=True, description='Directory path for CSV files'),
})

# --- Target communication models ---
target_config_model = ns.model('TargetConfig', {
    'ip': fields.String(required=True, description='Target IP address'),
    'port': fields.Integer(required=True, description='Target port'),
    'secure': fields.Boolean(required=True, description='Use wss:// if true')
})

target_get_model = ns.model('TargetGet', {
    'sm_name': fields.String(required=True, description='State machine name (e.g. "rtos")'),
    'reg_type': fields.String(required=True, description='Register type (e.g. "mb_hold")'),
    'offset': fields.String(required=True, description='Offset (numeric, hex, or data_definition name)'),
    'num': fields.Integer(required=True, description='Number of registers to read')
})

target_set_model = ns.model('TargetSet', {
    'sm_name': fields.String(required=True),
    'reg_type': fields.String(required=True),
    'offset': fields.String(required=True),
    'data': fields.List(fields.Integer, required=True, description='Data to write')
})

target_response_model = ns.model('TargetResponse', {
    'seq': fields.Integer,
    'offset': fields.Integer,
    'data': fields.List(fields.Integer)
})


# --- Target connection management ---
TARGET_CONFIG_FILE = "target_config.json"
_target_client = None

def load_target_config():
    """Load target connection config from JSON file"""
    if os.path.exists(TARGET_CONFIG_FILE):
        with open(TARGET_CONFIG_FILE, 'r') as f:
            return json.load(f)
    return {"ip": "127.0.0.1", "port": 9001, "secure": False}

def save_target_config(config):
    """Save target connection config to JSON file"""
    with open(TARGET_CONFIG_FILE, 'w') as f:
        json.dump(config, f, indent=2)

def get_target_client():
    """Get or create target client connection"""
    global _target_client
    if _target_client is None or not _target_client.connected:
        cfg = load_target_config()
        _target_client = TargetClient(cfg["ip"], cfg["port"], cfg["secure"])
        if not _target_client.connect():
            raise Exception(f"Failed to connect to target at {cfg['ip']}:{cfg['port']}")
    return _target_client

def resolve_offset(offset_str):
    """
    Resolve offset from string:
    - If hex (0x...), return int
    - If decimal, return int
    - Otherwise, look up in data_definitions table
    """
    # Try hex
    if offset_str.startswith("0x") or offset_str.startswith("0X"):
        return int(offset_str, 16)
    # Try decimal
    try:
        return int(offset_str)
    except ValueError:
        pass
    # Look up in data_definitions
    dd = DataDefinition.query.filter_by(name=offset_str).first()
    if dd and dd.offset is not None:
        return dd.offset
    raise ValueError(f"Cannot resolve offset: {offset_str}")


# --- Existing endpoints ---

@ns.route('/config')
class AlarmConfig(Resource):
    @ns.doc('get_alarm_config')
    @ns.marshal_with(alarm_config_model)
    def get(self):
        """Fetch all alarm configuration data (definitions, levels, limits)"""
        # First ensure all alarms have their required records
        ensure_all_alarms_have_records(verbose=True)
        
        alarm_defs = AlarmDefinition.query.all()
        alarm_levels = AlarmLevelAction.query.all()
        limits_vals = LimitsValues.query.all()

        # Create a lookup for alarm_name by alarm_num
        alarm_name_lookup = {ad.alarm_num: ad.alarm_name for ad in alarm_defs}

        # Prepare alarm_level_actions with alarm_name
        prepared_alarm_levels = []
        for al in alarm_levels:
            level_dict = al.to_dict()
            level_dict['alarm_name'] = alarm_name_lookup.get(al.alarm_num, 'N/A')
            prepared_alarm_levels.append(level_dict)

        # Prepare limits_values with alarm_name
        limits_structure_to_alarm_name = {}
        for ad in alarm_defs:
            if ad.limits_structure and ad.limits_structure not in limits_structure_to_alarm_name:
                limits_structure_to_alarm_name[ad.limits_structure] = ad.alarm_name

        prepared_limits_vals = []
        for lv in limits_vals:
            limits_dict = lv.to_dict()
            limits_dict['alarm_name'] = limits_structure_to_alarm_name.get(lv.limits_structure, 'N/A')
            prepared_limits_vals.append(limits_dict)

        return {
            'alarm_definitions': [ad.to_dict() for ad in alarm_defs],
            'alarm_level_actions': prepared_alarm_levels,
            'limits_values': prepared_limits_vals
        }

    @ns.doc('update_alarm_config')
    @ns.expect(alarm_config_model)
    def post(self):
        """Update all alarm configuration data with auto-generation of levels and limits"""
        data = request.get_json()
        print("[/config POST] Incoming JSON:", data)

        try:
            # Delete all existing records
            AlarmLevelAction.query.delete()
            LimitsValues.query.delete()
            AlarmDefinition.query.delete()

            # Track which limits_structures we've already created
            created_limits = set()

            # Add alarm definitions and auto-generate related records
            for ad_data in data.get('alarm_definitions', []):
                # Create the alarm definition
                ad = AlarmDefinition(**ad_data)
                db.session.add(ad)
                db.session.flush()

                # Auto-generate alarm_level_actions based on num_levels
                num_levels = ad_data.get('num_levels', 1)
                for level in range(1, num_levels + 1):
                    ala = AlarmLevelAction(
                        alarm_num=ad.alarm_num,
                        level=level,
                        enabled=True,
                        duration=0,
                        actions='',
                        notes=''
                    )
                    db.session.add(ala)

                # Auto-generate limits_values if limits_structure is specified
                limits_structure = ad_data.get('limits_structure')
                if limits_structure and limits_structure.strip() and limits_structure not in created_limits:
                    lv = LimitsValues(
                        limits_structure=limits_structure,
                        level1_limit=0,
                        level2_limit=0,
                        level3_limit=0,
                        hysteresis=0,
                        last_updated=datetime.utcnow().isoformat(),
                        notes=''
                    )
                    db.session.add(lv)
                    created_limits.add(limits_structure)

            # Handle manually edited alarm_level_actions
            provided_levels = data.get('alarm_level_actions', [])
            if provided_levels:
                AlarmLevelAction.query.delete()
                for ala_data in provided_levels:
                    ala_data_clean = {k: v for k, v in ala_data.items() if k != 'alarm_name'}
                    ala = AlarmLevelAction(**ala_data_clean)
                    db.session.add(ala)

            # Handle manually edited limits_values
            provided_limits = data.get('limits_values', [])
            if provided_limits:
                LimitsValues.query.delete()
                for lv_data in provided_limits:
                    lv_data_clean = {k: v for k, v in lv_data.items() if k != 'alarm_name'}
                    lv_data_clean['last_updated'] = datetime.utcnow().isoformat()
                    lv = LimitsValues(**lv_data_clean)
                    db.session.add(lv)

            db.session.commit()

            return {'message': 'Alarm configuration updated successfully with auto-generated records'}, 200

        except Exception as e:
            db.session.rollback()
            return {'message': f'Error updating alarm configuration: {str(e)}'}, 500


@ns.route('/import-csv')
class ImportCSV(Resource):
    @ns.doc('import_csv')
    @ns.expect(csv_operation_model)
    def post(self):
        """Import alarm configuration from CSV files in specified directory"""
        data = request.get_json()
        directory = data.get('directory', 'csv_exports')
        directory = directory.rstrip('/')

        try:
            if not os.path.isdir(directory):
                return {'message': f'Directory not found: {directory}'}, 404
            
            csv_path = os.path.join(directory, CSV_ALARM_DEF_FILE)
            if not os.path.exists(csv_path):
                return {'message': f'CSV file not found: {csv_path}'}, 404

            load_alarm_defs_from_csv(csv_path, verbose=True)

            return {'message': f'Successfully imported from {directory}/'}, 200

        except Exception as e:
            return {'message': f'Error importing CSV: {str(e)}'}, 500


@ns.route('/export-csv')
class ExportCSV(Resource):
    @ns.doc('export_csv')
    @ns.expect(csv_operation_model)
    def post(self):
        """Export alarm configuration to CSV files in specified directory"""
        data = request.get_json()
        directory = data.get('directory', 'csv_exports')
        directory = directory.rstrip('/')

        try:
            os.makedirs(directory, exist_ok=True)

            export_alarm_definitions_to_csv(directory)
            export_alarm_level_actions_to_csv(directory)
            export_limits_values_to_csv(directory)

            return {'message': f'Successfully exported to {directory}/'}, 200

        except Exception as e:
            return {'message': f'Error exporting CSV: {str(e)}'}, 500


# --- NEW: Target communication endpoints ---

@ns.route('/target/config')
class TargetConfig(Resource):
    @ns.doc('get_target_config')
    @ns.marshal_with(target_config_model)
    def get(self):
        """Get current target connection config"""
        return load_target_config()

    @ns.doc('update_target_config')
    @ns.expect(target_config_model)
    @ns.marshal_with(target_config_model)
    def post(self):
        """Update target connection config"""
        config = ns.payload
        save_target_config(config)
        
        # Force reconnect on next request
        global _target_client
        if _target_client:
            _target_client.close()
            _target_client = None
        
        return config


@ns.route('/target/get')
class TargetGet(Resource):
    @ns.doc('get_target_data')
    @ns.expect(target_get_model)
    def post(self):
        """Get data from target"""
        payload = ns.payload
        sm_name = payload['sm_name']
        reg_type = payload['reg_type']
        offset_str = payload['offset']
        num = payload['num']

        try:
            # Resolve offset (numeric or lookup from data_definitions)
            offset = resolve_offset(offset_str)

            client = get_target_client()
            resp = client.get_data(sm_name, reg_type, offset, num)
            
            return resp, 200

        except Exception as e:
            return {'error': str(e)}, 500


@ns.route('/target/set')
class TargetSet(Resource):
    @ns.doc('set_target_data')
    @ns.expect(target_set_model)
    def post(self):
        """Set data on target"""
        payload = ns.payload
        sm_name = payload['sm_name']
        reg_type = payload['reg_type']
        offset_str = payload['offset']
        data = payload['data']

        try:
            offset = resolve_offset(offset_str)

            client = get_target_client()
            resp = client.set_data(sm_name, reg_type, offset, data)
            
            return resp, 200

        except Exception as e:
            return {'error': str(e)}, 500
```

---

### 3. `index.html` (UPDATED)

Replace your existing `index.html` with this version (adds Target tab):Let me provide the updated `index.html` directly:

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Alarm Config Editor</title>
    <style>
        body {
            font-family: sans-serif;
            margin: 0;
            padding: 0;
        }
        header {
            background: #222;
            color: #eee;
            padding: 10px 16px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        header h1 {
            margin: 0;
            font-size: 18px;
        }
        header .controls {
            display: flex;
            gap: 8px;
            align-items: center;
        }
        header input[type="text"] {
            padding: 4px 6px;
            font-size: 12px;
        }
        header button {
            padding: 4px 8px;
            font-size: 12px;
            cursor: pointer;
        }
        .tabs {
            display: flex;
            border-bottom: 1px solid #ccc;
            background: #f0f0f0;
        }
        .tab {
            padding: 8px 12px;
            cursor: pointer;
            border-right: 1px solid #ccc;
        }
        .tab.active {
            background: #fff;
            border-bottom: 2px solid #007bff;
            font-weight: bold;
        }
        .tab-content {
            display: none;
            padding: 10px;
            overflow: auto;
            max-height: calc(100vh - 150px);
        }
        .tab-content.active {
            display: block;
        }
        .tab-controls {
            margin-bottom: 8px;
            display: flex;
            gap: 8px;
        }
        .tab-controls button {
            padding: 4px 8px;
            font-size: 12px;
            cursor: pointer;
        }
        table {
            border-collapse: collapse;
            width: 100%;
            font-size: 12px;
        }
        th, td {
            border: 1px solid #ddd;
            padding: 4px 6px;
            text-align: left;
            white-space: nowrap;
        }
        th {
            background: #f9f9f9;
            position: sticky;
            top: 0;
            z-index: 1;
        }
        tbody tr:nth-child(even) {
            background: #fafafa;
        }
        tbody tr.new-row {
            background: #e8f5e9;
        }
        tbody tr.deleted-row {
            background: #ffebee;
            text-decoration: line-through;
            opacity: 0.6;
        }
        td[contenteditable="true"] {
            background: #fffef0;
        }
        td[contentselect="true"] {
            background: #fffef0;
        }
        td select {
            width: 100%;
            padding: 0;
            margin: 0;
            border: none;
            background-color: transparent;
            font-family: inherit;
            font-size: inherit;
            color: inherit;
            line-height: inherit;
            -webkit-appearance: none;
            -moz-appearance: none;
            appearance: none;
            cursor: pointer;
            box-sizing: border-box;
        }
        td select:focus {
            outline: none;
        }
        td.selectable-cell {
            position: relative;
            padding-right: 16px;
            cursor: pointer;
        }
        td.selectable-cell::after {
            content: "▼";
            position: absolute;
            right: 4px;
            top: 50%;
            transform: translateY(-50%);
            font-size: 9px;
            color: #888;
            pointer-events: none;
        }
        td.selectable-cell:hover::after {
            color: #333;
        }
        td.action-cell {
            text-align: center;
        }
        td.action-cell button {
            padding: 2px 6px;
            font-size: 11px;
            cursor: pointer;
        }
        .status-bar {
            padding: 6px 10px;
            background: #f8f8f8;
            border-top: 1px solid #ddd;
            font-size: 11px;
            color: #555;
            position: fixed;
            bottom: 0;
            left: 0;
            right: 0;
        }
        .status-bar span {
            margin-right: 15px;
        }
        .status-ok {
            color: #0a0;
        }
        .status-error {
            color: #a00;
        }
        
        /* Target tab specific styles */
        .target-section {
            margin-bottom: 20px;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
            background: #fafafa;
        }
        .target-section h3 {
            margin-top: 0;
            font-size: 14px;
            color: #333;
        }
        .target-form {
            display: flex;
            gap: 10px;
            align-items: center;
            flex-wrap: wrap;
        }
        .target-form label {
            font-size: 12px;
        }
        .target-form input[type="text"],
        .target-form input[type="number"] {
            padding: 4px 6px;
            font-size: 12px;
            border: 1px solid #ccc;
            border-radius: 3px;
        }
        .target-form input[type="checkbox"] {
            margin-left: 4px;
        }
        .target-form button {
            padding: 4px 8px;
            font-size: 12px;
            cursor: pointer;
        }
        #targetVarsTable td[contenteditable="true"] {
            background: #fffef0;
            cursor: text;
        }
    </style>
</head>
<body>
<header>
    <h1>Alarm Config Editor</h1>
    <div class="controls">
        <label>CSV Dir:
            <input type="text" id="csvDirInput" value="csv_exports">
        </label>
        <button id="loadBtn">Load from DB</button>
        <button id="saveBtn">Save to DB</button>
        <button id="exportBtn">Export CSV</button>
        <button id="importBtn">Import CSV</button>
    </div>
</header>

<div class="tabs">
    <div class="tab active" data-target="tab-defs">alarm_definitions</div>
    <div class="tab" data-target="tab-levels">alarm_level_actions</div>
    <div class="tab" data-target="tab-limits">limits_values</div>
    <div class="tab" data-target="tab-target">target</div>
</div>

<div id="tab-defs" class="tab-content active">
    <div class="tab-controls">
        <button id="addAlarmBtn">➕ Add Alarm</button>
        <span style="color: #666; font-size: 11px;">Click "Delete" on a row to mark for deletion. Save to apply changes.</span>
    </div>
    <table id="defsTable">
        <thead>
            <tr>
                <th>alarm_num</th>
                <th>alarm_name</th>
                <th>num_levels</th>
                <th>measured_variable</th>
                <th>limits_structure</th>
                <th>comparison_type</th>
                <th>alarm_variable</th>
                <th>latched_variable</th>
                <th>notes</th>
                <th>Actions</th>
            </tr>
        </thead>
        <tbody>
            <!-- filled by JS -->
        </tbody>
    </table>
</div>

<div id="tab-levels" class="tab-content">
    <table id="levelsTable">
        <thead>
            <tr>
                <th>alarm_num</th>
                <th>alarm_name</th>
                <th>level</th>
                <th>enabled</th>
                <th>duration</th>
                <th>actions</th>
                <th>notes</th>
            </tr>
        </thead>
        <tbody>
            <!-- filled by JS -->
        </tbody>
    </table>
</div>

<div id="tab-limits" class="tab-content">
    <table id="limitsTable">
        <thead>
            <tr>
                <th>limits_structure</th>
                <th>alarm_name</th>
                <th>level1_limit</th>
                <th>level2_limit</th>
                <th>level3_limit</th>
                <th>hysteresis</th>
                <th>last_updated</th>
                <th>notes</th>
            </tr>
        </thead>
        <tbody>
            <!-- filled by JS -->
        </tbody>
    </table>
</div>

<div id="tab-target" class="tab-content">
    <div class="target-section">
        <h3>Target Connection</h3>
        <div class="target-form">
            <label>IP: <input type="text" id="targetIp" value="127.0.0.1" /></label>
            <label>Port: <input type="number" id="targetPort" value="8080" /></label>
            <label>Secure: <input type="checkbox" id="targetSecure" /></label>
            <button onclick="saveTargetConfig()">💾 Save Config</button>
            <button onclick="loadTargetConfig()">🔄 Load Config</button>
        </div>
    </div>

    <div class="target-section">
        <h3>Variable Get/Set</h3>
        <div class="tab-controls">
            <button onclick="addTargetVar()">➕ Add Variable</button>
            <span style="color: #666; font-size: 11px;">Type variable name or shared mem ref, then click Get/Set</span>
        </div>
        <table id="targetVarsTable">
            <thead>
                <tr>
                    <th>Variable Name / Ref</th>
                    <th>sm_name</th>
                    <th>reg_type</th>
                    <th>offset</th>
                    <th>num</th>
                    <th>data (JSON array)</th>
                    <th>Actions</th>
                </tr>
            </thead>
            <tbody>
                <!-- filled by JS -->
            </tbody>
        </table>
    </div>
</div>

<div class="status-bar">
    <span id="statusText">Ready.</span>
</div>

<script>
const apiBase = location.origin + '/api/alarms';

const COMPARISON_OPTIONS = [
    'greater_than',
    'less_than',
    'equal',
    'not_equal',
    'greater_or_equal',
    'less_or_equal',
    'aggregate'
];

function makeComparisonTypeEditable(td) {
    if (td.querySelector('select')) return;

    const current = (td.textContent || '').trim() || 'greater_than';

    const select = document.createElement('select');
    COMPARISON_OPTIONS.forEach(opt => {
        const o = document.createElement('option');
        o.value = opt;
        o.textContent = opt;
        if (opt === current) {
            o.selected = true;
        }
        select.appendChild(o);
    });

    td.textContent = '';
    td.appendChild(select);
    select.focus();

    const finalize = () => {
        const value = select.value;
        td.removeChild(select);
        td.textContent = value;
    };

    select.addEventListener('blur', finalize);
    select.addEventListener('change', finalize);
}

function setStatus(msg, isError = false) {
    const el = document.getElementById('statusText');
    el.textContent = msg;
    el.className = isError ? 'status-error' : 'status-ok';
}

// Simple tab switching
document.querySelectorAll('.tab').forEach(tab => {
    tab.addEventListener('click', () => {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        tab.classList.add('active');
        document.getElementById(tab.dataset.target).classList.add('active');
    });
});

// Render helpers
function renderDefs(data) {
    const tbody = document.querySelector('#defsTable tbody');
    tbody.innerHTML = '';
    data.forEach(row => {
        const tr = document.createElement('tr');
        const cols = ['alarm_num','alarm_name','num_levels','measured_variable','limits_structure','comparison_type','alarm_variable','latched_variable','notes'];
        cols.forEach(col => {
            const td = document.createElement('td');
            td.textContent = row[col] ?? '';
            td.setAttribute('data-col', col);
            if (col !== 'alarm_num') {
                if (col === 'comparison_type') {
                    td.classList.add('selectable-cell');
                    td.addEventListener('click', () => {
                        makeComparisonTypeEditable(td);
                    });
                    td.setAttribute('contentselect', 'true');
                } else {
                    td.setAttribute('contenteditable', 'true');
                }
            }
            tr.appendChild(td);
        });
        
        const actionTd = document.createElement('td');
        actionTd.className = 'action-cell';
        const deleteBtn = document.createElement('button');
        deleteBtn.textContent = '🗑️ Delete';
        deleteBtn.onclick = () => toggleDeleteRow(tr);
        actionTd.appendChild(deleteBtn);
        tr.appendChild(actionTd);
        
        tbody.appendChild(tr);
    });
}

function renderLevels(data) {
    const tbody = document.querySelector('#levelsTable tbody');
    tbody.innerHTML = '';
    data.forEach(row => {
        const tr = document.createElement('tr');
        const cols = ['alarm_num','alarm_name','level','enabled','duration','actions','notes'];
        cols.forEach(col => {
            const td = document.createElement('td');
            let val = row[col];
            if (col === 'enabled') {
                val = row[col] ? 1 : 0;
            }
            td.textContent = val ?? '';
            td.setAttribute('data-col', col);
            if (col !== 'alarm_num' && col !== 'alarm_name' && col !== 'level') {
                td.setAttribute('contenteditable', 'true');
            }
            tr.appendChild(td);
        });
        tbody.appendChild(tr);
    });
}

function renderLimits(data) {
    const tbody = document.querySelector('#limitsTable tbody');
    tbody.innerHTML = '';
    data.forEach(row => {
        const tr = document.createElement('tr');
        const cols = ['limits_structure','alarm_name','level1_limit','level2_limit','level3_limit','hysteresis','last_updated','notes'];
        cols.forEach(col => {
            const td = document.createElement('td');
            td.textContent = row[col] ?? '';
            td.setAttribute('data-col', col);
            if (col !== 'limits_structure' && col !== 'alarm_name' && col !== 'last_updated') {
                td.setAttribute('contenteditable', 'true');
            }
            tr.appendChild(td);
        });
        tbody.appendChild(tr);
    });
}

function addNewAlarm() {
    const tbody = document.querySelector('#defsTable tbody');
    const existingRows = tbody.querySelectorAll('tr:not(.deleted-row)');
    
    let maxAlarmNum = -1;
    existingRows.forEach(tr => {
        const numCell = tr.querySelector('td[data-col="alarm_num"]');
        if (numCell) {
            const num = parseInt(numCell.textContent);
            if (!isNaN(num) && num > maxAlarmNum) {
                maxAlarmNum = num;
            }
        }
    });
    const newAlarmNum = maxAlarmNum + 1;
    
    const tr = document.createElement('tr');
    tr.className = 'new-row';
    
    const cols = ['alarm_num','alarm_name','num_levels','measured_variable','limits_structure','comparison_type','alarm_variable','latched_variable','notes'];
    const defaults = {
        'alarm_num': newAlarmNum,
        'alarm_name': 'New Alarm',
        'num_levels': 1,
        'measured_variable': '',
        'limits_structure': '',
        'comparison_type': 'greater_than',
        'alarm_variable': '',
        'latched_variable': '',
        'notes': ''
    };
    
    cols.forEach(col => {
        const td = document.createElement('td');
        td.textContent = defaults[col];
        td.setAttribute('data-col', col);
        if (col !== 'alarm_num') {
            td.setAttribute('contenteditable', 'true');
        }
        tr.appendChild(td);
    });
    
    const actionTd = document.createElement('td');
    actionTd.className = 'action-cell';
    const deleteBtn = document.createElement('button');
    deleteBtn.textContent = '🗑️ Delete';
    deleteBtn.onclick = () => toggleDeleteRow(tr);
    actionTd.appendChild(deleteBtn);
    tr.appendChild(actionTd);
    
    tbody.appendChild(tr);
    setStatus('New alarm added. Edit and click "Save to DB" to persist.', false);
}

function toggleDeleteRow(tr) {
    if (tr.classList.contains('deleted-row')) {
        tr.classList.remove('deleted-row');
        setStatus('Row unmarked for deletion.', false);
    } else {
        tr.classList.add('deleted-row');
        setStatus('Row marked for deletion. Click "Save to DB" to apply.', false);
    }
}

function collectDefs() {
    const rows = [];
    document.querySelectorAll('#defsTable tbody tr:not(.deleted-row)').forEach(tr => {
        const obj = {};
        tr.querySelectorAll('td[data-col]').forEach(td => {
            const col = td.getAttribute('data-col');
            let val = td.textContent.trim();

            if (col === 'alarm_num' || col === 'num_levels') {
                val = val === '' ? null : Number(val);
            }

            obj[col] = (val === '' ? null : val);
        });

        if (obj.alarm_num == null) {
            console.warn('Skipping row without alarm_num:', obj);
            return;
        }
        if (!obj.alarm_name || obj.alarm_name.toString().trim() === '') {
            console.warn('Skipping row without alarm_name:', obj);
            return;
        }
        if (obj.num_levels == null || isNaN(obj.num_levels)) {
            console.warn('Skipping row without valid num_levels:', obj);
            return;
        }

        rows.push(obj);
    });
    return rows;
}

function collectLevels() {
    const rows = [];
    document.querySelectorAll('#levelsTable tbody tr').forEach(tr => {
        const obj = {};
        tr.querySelectorAll('td').forEach(td => {
            const col = td.getAttribute('data-col');
            if (col === 'alarm_name') return;
            
            let val = td.textContent.trim();
            if (col === 'alarm_num' || col === 'level') {
                val = val === '' ? null : Number(val);
            } else if (col === 'enabled') {
                val = val === '' ? false : (val === '1' || val.toLowerCase() === 'true');
            } else if (col === 'duration') {
                val = val === '' ? null : Number(val);
            }
            obj[col] = val === '' ? null : val;
        });
        if (obj.alarm_num != null && obj.level != null) {
            rows.push(obj);
        }
    });
    return rows;
}

function collectLimits() {
    const rows = [];
    document.querySelectorAll('#limitsTable tbody tr').forEach(tr => {
        const obj = {};
        tr.querySelectorAll('td').forEach(td => {
            const col = td.getAttribute('data-col');
            if (col === 'alarm_name') return;
            
            let val = td.textContent.trim();
            if (['level1_limit','level2_limit','level3_limit','hysteresis'].includes(col)) {
                val = val === '' ? null : Number(val);
            }
            obj[col] = val === '' ? null : val;
        });
        if (obj.limits_structure) {
            rows.push(obj);
        }
    });
    return rows;
}

async function loadFromDB() {
    setStatus('Loading from /api/alarms/config ...');
    try {
        const res = await fetch(apiBase + '/config');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();
        renderDefs(data.alarm_definitions || []);
        renderLevels(data.alarm_level_actions || []);
        renderLimits(data.limits_values || []);
        setStatus('Loaded configuration from DB.', false);
    } catch (err) {
        console.error(err);
        setStatus('Error loading config: ' + err.message, true);
    }
}

async function saveToDB() {
    const payload = {
        alarm_definitions: collectDefs(),
        alarm_level_actions: collectLevels(),
        limits_values: collectLimits()
    };
    setStatus('Saving to /api/alarms/config ...');
    try {
        const res = await fetch(apiBase + '/config', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || ('HTTP ' + res.status));
        setStatus('Saved configuration to DB. Reloading...', false);
        await loadFromDB();
    } catch (err) {
        console.error(err);
        setStatus('Error saving config: ' + err.message, true);
    }
}

async function exportCSV() {
    const dir = document.getElementById('csvDirInput').value.trim() || 'csv_exports';
    setStatus('Exporting CSV to dir: ' + dir + ' ...');
    try {
        const res = await fetch(apiBase + '/export-csv', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ directory: dir })
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || ('HTTP ' + res.status));
        setStatus('Exported CSV to: ' + data.message, false);
    } catch (err) {
        console.error(err);
        setStatus('Error exporting CSV: ' + err.message, true);
    }
}

async function importCSV() {
    const dir = document.getElementById('csvDirInput').value.trim() || 'csv_exports';
    setStatus('Importing CSV from dir: ' + dir + ' ...');
    try {
        const res = await fetch(apiBase + '/import-csv', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ directory: dir })
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || ('HTTP ' + res.status));
        setStatus('Imported CSV from: ' + dir + '. Reloading ...', false);
        await loadFromDB();
    } catch (err) {
        console.error(err);
        setStatus('Error importing CSV: ' + err.message, true);
    }
}

// --- Target tab functions ---

async function loadTargetConfig() {
    setStatus('Loading target config...');
    try {
        const res = await fetch(apiBase + '/target/config');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const config = await res.json();
        document.getElementById('targetIp').value = config.ip;
        document.getElementById('targetPort').value = config.port;
        document.getElementById('targetSecure').checked = config.secure;
        setStatus('Target config loaded.', false);
    } catch (err) {
        console.error(err);
        setStatus('Error loading target config: ' + err.message, true);
    }
}

async function saveTargetConfig() {
    const config = {
        ip: document.getElementById('targetIp').value,
        port: parseInt(document.getElementById('targetPort').value),
        secure: document.getElementById('targetSecure').checked
    };
    setStatus('Saving target config...');
    try {
        const res = await fetch(apiBase + '/target/config', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(config)
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || ('HTTP ' + res.status));
        setStatus('Target config saved.', false);
    } catch (err) {
        console.error(err);
        setStatus('Error saving target config: ' + err.message, true);
    }
}

function addTargetVar() {
    const tbody = document.querySelector('#targetVarsTable tbody');
    const tr = document.createElement('tr');
    
    const cols = ['var_name', 'sm_name', 'reg_type', 'offset', 'num', 'data'];
    const defaults = {
        'var_name': 'rtos:hold:alarm_limits',
        'sm_name': 'rtos',
        'reg_type': 'mb_hold',
        'offset': '9',
        'num': '4',
        'data': '[]'
    };
    
    cols.forEach(col => {
        const td = document.createElement('td');
        td.textContent = defaults[col];
        td.setAttribute('data-col', col);
        td.setAttribute('contenteditable', 'true');
        tr.appendChild(td);
    });
    
    const actionTd = document.createElement('td');
    actionTd.className = 'action-cell';
    
    const getBtn = document.createElement('button');
    getBtn.textContent = '📥 Get';
    getBtn.onclick = () => getTargetData(tr);
    actionTd.appendChild(getBtn);
    
    const setBtn = document.createElement('button');
    setBtn.textContent = '📤 Set';
    setBtn.onclick = () => setTargetData(tr);
    actionTd.appendChild(setBtn);
    
    const delBtn = document.createElement('button');
    delBtn.textContent = '🗑️';
    delBtn.onclick = () => tr.remove();
    actionTd.appendChild(delBtn);
    
    tr.appendChild(actionTd);
    tbody.appendChild(tr);
}

async function getTargetData(tr) {
    const cells = tr.querySelectorAll('td[data-col]');
    const payload = {
        sm_name: cells[1].textContent.trim(),
        reg_type: cells[2].textContent.trim(),
        offset: cells[3].textContent.trim(),
        num: parseInt(cells[4].textContent.trim())
    };
    
    setStatus('Getting data from target...');
    try {
        const res = await fetch(apiBase + '/target/get', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.error || ('HTTP ' + res.status));
        
        // Update data column
        cells[5].textContent = JSON.stringify(data.data || []);
        setStatus('Got data: ' + JSON.stringify(data), false);
    } catch (err) {
        console.error(err);
        setStatus('Error getting data: ' + err.message, true);
    }
}

async function setTargetData(tr) {
    const cells = tr.querySelectorAll('td[data-col]');
    const dataStr = cells[5].textContent.trim();
    let dataArray;
    try {
        dataArray = JSON.parse(dataStr);
    } catch (e) {
        setStatus('Invalid JSON in data field', true);
        return;
    }
    
    const payload = {
        sm_name: cells[1].textContent.trim(),
        reg_type: cells[2].textContent.trim(),
        offset: cells[3].textContent.trim(),
        data: dataArray
    };
    
    setStatus('Setting data on target...');
    try {
        const res = await fetch(apiBase + '/target/set', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.error || ('HTTP ' + res.status));
        
        setStatus('Set data: ' + JSON.stringify(data), false);
    } catch (err) {
        console.error(err);
        setStatus('Error setting data: ' + err.message, true);
    }
}

// Wire buttons
document.getElementById('loadBtn').addEventListener('click', loadFromDB);
document.getElementById('saveBtn').addEventListener('click', saveToDB);
document.getElementById('exportBtn').addEventListener('click', exportCSV);
document.getElementById('importBtn').addEventListener('click', importCSV);
document.getElementById('addAlarmBtn').addEventListener('click', addNewAlarm);

// Initial load
loadFromDB();
loadTargetConfig();
</script>
</body>
</html>
```

---

## Summary

You now have:

1. **`target_client.py`** - WebSocket client wrapper
2. **`api_endpoints.py`** - Updated with 4 new `/target/*` endpoints
3. **`index.html`** - New "Target" tab with:
   - Connection config (IP/port/secure)
   - Variable table where you can type var names, shared mem refs, and get/set values

**Next steps:**

1. Install `websocket-client`: `pip install websocket-client`
2. Test with a mock WebSocket server or your real target
3. Later we can add "Read Limits from Target" / "Write Limits to Target" buttons that auto-populate from your alarm definitions

Let me know if you want me to create a **mock WebSocket server** for testing, or if you're ready to test with your real target!