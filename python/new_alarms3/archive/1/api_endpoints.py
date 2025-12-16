# api_endpoints.py


import os
import json
from collections import defaultdict
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

# import os
# from datetime import datetime
# from flask import request
# from flask_restx import Namespace, Resource, fields
# from extensions import db
# from alarm_models import AlarmDefinition, AlarmLevelAction, LimitsValues
# from alarm_utils import (
#     load_alarm_defs_from_csv,
#     export_alarm_definitions_to_csv,
#     export_alarm_level_actions_to_csv,
#     export_limits_values_to_csv,
#     ensure_all_alarms_have_records,
#     normalize_comparison_type,
#     CSV_ALARM_DEF_FILE
# )

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

var_group_export_model = ns.model('VarGroupExport', {
    'group_name': fields.String(required=True, description='Name for the variable group file (e.g., "hithium")'),
    'pub_filter': fields.String(description='Optional filter for the "pub" field (e.g., "config")'),
    'directory': fields.String(description='Directory to save the JSON file (default: "var_groups")', default='var_groups'),
})

@ns.route('/export-group')
class ExportVarGroup(Resource):
    @ns.doc('export_variable_group')
    @ns.expect(var_group_export_model)
    def post(self):
        """Export DataDefinitions to a structured JSON variable group file."""
        data = request.get_json()
        group_name = data.get('group_name')
        pub_filter = data.get('pub_filter')
        directory = data.get('directory', 'var_groups')

        if not group_name:
            return {'message': 'group_name is required'}, 400

        try:
            os.makedirs(directory, exist_ok=True)
            file_path = os.path.join(directory, f'vars_{group_name}.json')

            query = DataDefinition.query
            if pub_filter:
                query = query.filter_by(pub=pub_filter)
            
            data_defs = query.all()

            # Group by pub, then table, then group_path
            grouped_data = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
            for dd in data_defs:
                # Ensure pub, table, group_path are not None for grouping
                current_pub = dd.pub if dd.pub else "default_pub"
                current_table = dd.table if dd.table else "default_table"
                current_group_path = dd.group_path if dd.group_path else ""

                grouped_data[current_pub][current_table][current_group_path].append(dd)

            output_json = []
            for pub, tables in grouped_data.items():
                pub_entry = {"pub": pub, "items": []}
                for table, groups in tables.items():
                    table_entry = {"table": table, "items": []}
                    
                    # Sort group_paths to ensure consistent order
                    sorted_group_paths = sorted(groups.keys())

                    for group_path in sorted_group_paths:
                        group_items = groups[group_path]
                        
                        # Reconstruct nested groups from group_path
                        # e.g., "Configuration / Clock" -> {"name": "Configuration", "items": [{"name": "Clock", "items": [...]}]}
                        
                        current_level_items = table_entry["items"]
                        path_parts = group_path.split(' / ') if group_path else []
                        
                        for i, part in enumerate(path_parts):
                            # Find existing group or create new one
                            found_group = next((item for item in current_level_items if item.get("name") == part and "items" in item), None)
                            if not found_group:
                                found_group = {"name": part, "items": []}
                                current_level_items.append(found_group)
                            current_level_items = found_group["items"]
                        
                        # Add leaf variables to the innermost group
                        for dd in group_items:
                            var_entry = {
                                "name": dd.name,
                                "src": dd.shm_ref,
                            }
                            if dd.data_type:
                                var_entry["type"] = dd.data_type
                            if dd.fmt:
                                var_entry["fmt"] = dd.fmt
                            if dd.example:
                                var_entry["example"] = dd.example
                            # Add other fields if needed, e.g., "description", "units", "min_val", "max_val", "read_only"
                            current_level_items.append(var_entry)
                
                output_json.append(pub_entry)

            # Write the first pub entry (assuming one pub per file for simplicity for now)
            # If you want multiple pubs in one file, we'd adjust this.
            if output_json:
                with open(file_path, 'w', encoding='utf-8') as f:
                    json.dump(output_json[0], f, indent=4) # Dump the first pub entry

            return {'message': f'Successfully exported variable group to {file_path}'}, 200

        except Exception as e:
            return {'message': f'Error exporting variable group: {str(e)}'}, 500

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


#----------------------------------------------------------

# --- Flask-RESTX Models for API documentation ---
# alarm_def_model = ns.model('AlarmDefinition', {
#     'alarm_num': fields.Integer(required=True, description='Alarm number (primary key)'),
#     'alarm_name': fields.String(required=True, description='Alarm name'),
#     'num_levels': fields.Integer(required=True, description='Number of alarm levels (1-3)'),
#     'measured_variable': fields.String(description='Measured variable reference'),
#     'limits_structure': fields.String(description='Limits structure reference'),
#     'comparison_type': fields.String(
#         description='Comparison type',
#         enum=['greater_than', 'less_than', 'equal', 'not_equal', 'greater_or_equal', 'less_or_equal'],
#         default='greater_than'
#     ),
#     'alarm_variable': fields.String(description='Alarm variable reference'),
#     'latched_variable': fields.String(description='Latched variable reference'),
#     'notes': fields.String(description='Notes'),
# })

# alarm_level_action_model = ns.model('AlarmLevelAction', {
#     'alarm_num': fields.Integer(required=True, description='Alarm number (FK)'),
#     'level': fields.Integer(required=True, description='Alarm level (1-3)'),
#     'enabled': fields.Boolean(description='Whether this level is enabled'),
#     'duration': fields.Integer(description='Duration in seconds'),
#     'actions': fields.String(description='Actions to take'),
#     'notes': fields.String(description='Notes'),
#     'alarm_name': fields.String(description='Alarm name (for display)'),
# })

# limits_value_model = ns.model('LimitsValue', {
#     'limits_structure': fields.String(required=True, description='Limits structure name (primary key)'),
#     'level1_limit': fields.Integer(description='Level 1 limit'),
#     'level2_limit': fields.Integer(description='Level 2 limit'),
#     'level3_limit': fields.Integer(description='Level 3 limit'),
#     'hysteresis': fields.Integer(description='Hysteresis value'),
#     'last_updated': fields.String(description='Last updated timestamp'),
#     'notes': fields.String(description='Notes'),
#     'alarm_name': fields.String(description='Alarm name (for display)'),
# })

# alarm_config_model = ns.model('AlarmConfig', {
#     'alarm_definitions': fields.List(fields.Nested(alarm_def_model)),
#     'alarm_level_actions': fields.List(fields.Nested(alarm_level_action_model)),
#     'limits_values': fields.List(fields.Nested(limits_value_model)),
# })

    # csv_operation_model = ns.model('CSVOperation', {
    # 'directory': fields.String(required=True, description='Directory path for CSV files'),
# })

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
#----------------------------------------------

# @ns.route('/config')
# class AlarmConfig(Resource):
#     @ns.doc('get_alarm_config')
#     @ns.marshal_with(alarm_config_model)
#     def get(self):
#         """Fetch all alarm configuration data (definitions, levels, limits)"""
#         # First ensure all alarms have their required records
#         ensure_all_alarms_have_records(verbose=True)
        
#         alarm_defs = AlarmDefinition.query.all()
#         alarm_levels = AlarmLevelAction.query.all()
#         limits_vals = LimitsValues.query.all()

#         # Create a lookup for alarm_name by alarm_num
#         alarm_name_lookup = {ad.alarm_num: ad.alarm_name for ad in alarm_defs}

#         # Prepare alarm_level_actions with alarm_name
#         prepared_alarm_levels = []
#         for al in alarm_levels:
#             level_dict = al.to_dict()
#             level_dict['alarm_name'] = alarm_name_lookup.get(al.alarm_num, 'N/A')
#             prepared_alarm_levels.append(level_dict)

#         # Prepare limits_values with alarm_name
#         limits_structure_to_alarm_name = {}
#         for ad in alarm_defs:
#             if ad.limits_structure and ad.limits_structure not in limits_structure_to_alarm_name:
#                 limits_structure_to_alarm_name[ad.limits_structure] = ad.alarm_name

#         prepared_limits_vals = []
#         for lv in limits_vals:
#             limits_dict = lv.to_dict()
#             limits_dict['alarm_name'] = limits_structure_to_alarm_name.get(lv.limits_structure, 'N/A')
#             prepared_limits_vals.append(limits_dict)

#         return {
#             'alarm_definitions': [ad.to_dict() for ad in alarm_defs],
#             'alarm_level_actions': prepared_alarm_levels,
#             'limits_values': prepared_limits_vals
#         }

#     @ns.doc('update_alarm_config')
#     @ns.expect(alarm_config_model)
#     def post(self):
#         """Update all alarm configuration data with auto-generation of levels and limits"""
#         data = request.get_json()
#         print("[/config POST] Incoming JSON:", data)  # <--- add this

#         try:
#             # Delete all existing records
#             AlarmLevelAction.query.delete()
#             LimitsValues.query.delete()
#             AlarmDefinition.query.delete()

#             # Track which limits_structures we've already created
#             created_limits = set()

#             # Add alarm definitions and auto-generate related records
#             for ad_data in data.get('alarm_definitions', []):
#                 # Create the alarm definition
#                 ad = AlarmDefinition(**ad_data)
#                 db.session.add(ad)
#                 db.session.flush()  # Get the alarm_num if it's auto-generated

#                 # Auto-generate alarm_level_actions based on num_levels
#                 num_levels = ad_data.get('num_levels', 1)
#                 for level in range(1, num_levels + 1):
#                     ala = AlarmLevelAction(
#                         alarm_num=ad.alarm_num,
#                         level=level,
#                         enabled=True,
#                         duration=0,
#                         actions='',
#                         notes=''
#                     )
#                     db.session.add(ala)

#                 # Auto-generate limits_values if limits_structure is specified and doesn't exist yet
#                 limits_structure = ad_data.get('limits_structure')
#                 if limits_structure and limits_structure.strip() and limits_structure not in created_limits:
#                     lv = LimitsValues(
#                         limits_structure=limits_structure,
#                         level1_limit=0,
#                         level2_limit=0,
#                         level3_limit=0,
#                         hysteresis=0,
#                         last_updated=datetime.utcnow().isoformat(),
#                         notes=''
#                     )
#                     db.session.add(lv)
#                     created_limits.add(limits_structure)

#             # Now handle any manually edited alarm_level_actions from the UI
#             # (merge with auto-generated ones)
#             provided_levels = data.get('alarm_level_actions', [])
#             if provided_levels:
#                 # Delete auto-generated ones and replace with provided
#                 AlarmLevelAction.query.delete()
#                 for ala_data in provided_levels:
#                     ala_data_clean = {k: v for k, v in ala_data.items() if k != 'alarm_name'}
#                     ala = AlarmLevelAction(**ala_data_clean)
#                     db.session.add(ala)

#             # Handle any manually edited limits_values from the UI
#             provided_limits = data.get('limits_values', [])
#             if provided_limits:
#                 # Delete auto-generated ones and replace with provided
#                 LimitsValues.query.delete()
#                 for lv_data in provided_limits:
#                     lv_data_clean = {k: v for k, v in lv_data.items() if k != 'alarm_name'}
#                     # Update last_updated timestamp
#                     lv_data_clean['last_updated'] = datetime.utcnow().isoformat()
#                     lv = LimitsValues(**lv_data_clean)
#                     db.session.add(lv)

#             db.session.commit()

#             return {'message': 'Alarm configuration updated successfully with auto-generated records'}, 200

#         except Exception as e:
#             db.session.rollback()
#             return {'message': f'Error updating alarm configuration: {str(e)}'}, 500


# @ns.route('/import-csv')
# class ImportCSV(Resource):
#     @ns.doc('import_csv')
#     @ns.expect(csv_operation_model)
#     def post(self):
#         """Import alarm configuration from CSV files in specified directory"""
#         data = request.get_json()
#         directory = data.get('directory', 'csv_exports')
        
#         # Remove trailing slash if present
#         directory = directory.rstrip('/')

#         try:
#             # Check if directory exists
#             if not os.path.isdir(directory):
#                 return {'message': f'Directory not found: {directory}'}, 404
            
#             csv_path = os.path.join(directory, CSV_ALARM_DEF_FILE)
#             if not os.path.exists(csv_path):
#                 return {'message': f'CSV file not found: {csv_path}'}, 404

#             load_alarm_defs_from_csv(csv_path, verbose=True)

#             return {'message': f'Successfully imported from {directory}/'}, 200

#         except Exception as e:
#             return {'message': f'Error importing CSV: {str(e)}'}, 500


# @ns.route('/export-csv')
# class ExportCSV(Resource):
#     @ns.doc('export_csv')
#     @ns.expect(csv_operation_model)
#     def post(self):
#         """Export alarm configuration to CSV files in specified directory"""
#         data = request.get_json()
#         directory = data.get('directory', 'csv_exports')
        
#         # Remove trailing slash if present
#         directory = directory.rstrip('/')

#         try:
#             # Create directory if it doesn't exist
#             os.makedirs(directory, exist_ok=True)

#             # Export all three CSV files
#             export_alarm_definitions_to_csv(directory)
#             export_alarm_level_actions_to_csv(directory)
#             export_limits_values_to_csv(directory)

#             return {'message': f'Successfully exported to {directory}/'}, 200

#         except Exception as e:
#             return {'message': f'Error exporting CSV: {str(e)}'}, 500