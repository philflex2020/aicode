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

# ====
# Existing alarms namespace
# ====

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
        enum=['greater_than', 'less_than', 'equal', 'not_equal', 'greater_or_equal', 'less_or_equal', 'aggregate'],
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
    'duration': fields.String(description='Duration in seconds on:off'),
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

json_operation_model = ns.model('JSONOperation', {
    'directory': fields.String(required=True, description='Directory path for JSON file'),
    'file_name': fields.String(required=True, description='JSON file name')
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
    'write_data': fields.List(fields.Integer, required=True, description='Data to write')
})

target_get_response_model = ns.model('TargetGetResponse', {
    'seq': fields.Integer,
    'offset': fields.Integer,
    'read_data': fields.List(fields.Integer, description='Data read from target')
})

target_set_response_model = ns.model('TargetSetResponse', {
    'seq': fields.Integer,
    'offset': fields.Integer,
    'written_data': fields.List(fields.Integer, description='Data written to target (echo)')
})


# --- Target connection management ---
TARGET_CONFIG_FILE = "target_config.json"
_target_client = None


@ns.route('/config')
class AlarmConfig(Resource):
    @ns.doc('get_alarm_config')
    @ns.marshal_with(alarm_config_model)
    def get(self):
        """Fetch all alarm configuration data (definitions, levels, limits)"""
        # First ensure all alarms have their required records
        print("running get /config")
        ensure_all_alarms_have_records(verbose=True)
        
        alarm_defs = AlarmDefinition.query.all()
        alarm_levels = AlarmLevelAction.query.all()
        limits_vals = LimitsValues.query.all()
        if alarm_levels:
            first_level = alarm_levels[0]
            print(f"First alarm level: alarm_num={first_level.alarm_num}, level={first_level.level}, duration={first_level.duration!r}")
        else:
            print("No alarm levels found")

        # Create a lookup for alarm_name by alarm_num
        alarm_name_lookup = {ad.alarm_num: ad.alarm_name for ad in alarm_defs}

        # Prepare alarm_level_actions with alarm_name
        prepared_alarm_levels = []
        for al in alarm_levels:
            level_dict = {
                'alarm_num': al.alarm_num,
                'level': al.level,
                'enabled': al.enabled,
                'duration': al.duration,
                'actions': al.actions,
                'notes': al.notes,
                'alarm_name': alarm_name_lookup.get(al.alarm_num, 'N/A')
            }
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
        print("[/config POST] Update Incoming JSON:", data)
        print("[/config POST] Running Update",)

        try:
            # Delete all existing records
            AlarmLevelAction.query.delete()
            LimitsValues.query.delete()
            AlarmDefinition.query.delete()

            # Track which limits_structures we've already created

            # STEP 1: Add alarm definitions
            for ad_data in data.get('alarm_definitions', []):
                # Normalize comparison_type if needed
                if 'comparison_type' in ad_data:
                    ad_data['comparison_type'] = normalize_comparison_type(ad_data['comparison_type'])

                ad = AlarmDefinition(**ad_data)
                db.session.add(ad)

            # STEP 2: Add alarm_level_actions from UI (preserves user edits)
            provided_levels = data.get('alarm_level_actions', [])
            print(f"[/config POST] Received {len(provided_levels)} alarm_level_actions")

            if provided_levels:
                # Debug: Print the first alarm level action data
                ala_data = provided_levels[0]
                print("ala_data:", ala_data)

            for ala_data in provided_levels:
                # Remove alarm_name (display-only field)
                ala_data_clean = {k: v for k, v in ala_data.items() if k != 'alarm_name'}

                if ala_data_clean.get('duration') is None:
                        ala_data_clean['duration'] = '4:6'
                # print(f"[/config POST] Adding alarm_level_action: {ala_data_clean}")
                ala = AlarmLevelAction(**ala_data_clean)
                db.session.add(ala)

            # STEP 3: Add limits_values from UI (preserves user edits)
            provided_limits = data.get('limits_values', [])
            print(f"[/config POST] Received {len(provided_limits)} limits_values")
            for lv_data in provided_limits:
                # Remove alarm_name (display-only field)
                lv_data_clean = {k: v for k, v in lv_data.items() if k != 'alarm_name'}
                lv_data_clean['last_updated'] = datetime.utcnow().isoformat()
                print(f"[/config POST] Adding limits_value: {lv_data_clean}")
                lv = LimitsValues(**lv_data_clean)
                db.session.add(lv)

            db.session.commit()
            print("[/config POST] Successfully committed all changes")

            return {'message': 'Alarm configuration updated successfully'}, 200

        except Exception as e:
            db.session.rollback()
            print(f"[/config POST] ERROR: {str(e)}")
            import traceback
            traceback.print_exc()
            return {'message': f'Error updating alarm configuration: {str(e)}'}, 500

        #     created_limits = set()

        #     # Add alarm definitions and auto-generate related records
        #     for ad_data in data.get('alarm_definitions', []):
        #         # Normalize comparison_type if needed (optional helper)
        #         if 'comparison_type' in ad_data:
        #             ad_data['comparison_type'] = normalize_comparison_type(ad_data['comparison_type'])

        #         ad = AlarmDefinition(**ad_data)
        #         db.session.add(ad)
        #         db.session.flush()  # Get alarm_num if auto-generated

        #         # Auto-generate alarm_level_actions based on num_levels
        #         num_levels = ad_data.get('num_levels', 1)
        #         for level in range(1, num_levels + 1):
        #             ala = AlarmLevelAction(
        #             alarm_num=ad.alarm_num,
        #             level=level,
        #             enabled=True,
        #             duration='3:3',
        #             actions='',
        #             notes=''
        #             )
        #             db.session.add(ala)

        #         # Auto-generate limits_values if limits_structure is specified and doesn't exist yet
        #         limits_structure = ad_data.get('limits_structure')
        #         if limits_structure and limits_structure.strip() and limits_structure not in created_limits:
        #             lv = LimitsValues(
        #             limits_structure=limits_structure,
        #             level1_limit=0,
        #             level2_limit=0,
        #             level3_limit=0,
        #             hysteresis=0,
        #             last_updated=datetime.utcnow().isoformat(),
        #             notes=''
        #             )
        #             db.session.add(lv)
        #             created_limits.add(limits_structure)

        #     # Now handle any manually edited alarm_level_actions from the UI
        #     provided_levels = data.get('alarm_level_actions', [])
        #     if provided_levels:
        #         # Delete auto-generated ones and replace with provided
        #         AlarmLevelAction.query.delete()
        #         for ala_data in provided_levels:
        #             ala_data_clean = {k: v for k, v in ala_data.items() if k != 'alarm_name'}
        #             ala = AlarmLevelAction(**ala_data_clean)
        #             db.session.add(ala)

        #     # Handle any manually edited limits_values from the UI
        #     provided_limits = data.get('limits_values', [])
        #     if provided_limits:
        #         # Delete auto-generated ones and replace with provided
        #         LimitsValues.query.delete()
        #         for lv_data in provided_limits:
        #             lv_data_clean = {k: v for k, v in lv_data.items() if k != 'alarm_name'}
        #             lv_data_clean['last_updated'] = datetime.utcnow().isoformat()
        #             lv = LimitsValues(**lv_data_clean)
        #             db.session.add(lv)

        #     db.session.commit()

        #     return {'message': 'Alarm configuration updated successfully with auto-generated records'}, 200

        # except Exception as e:
        #     db.session.rollback()
        #     return {'message': f'Error updating alarm configuration: {str(e)}'}, 500


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
    @ns.marshal_with(target_get_response_model)
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
            
            return {'seq': resp.get('seq'), 'offset': resp.get('offset'), 'read_data': resp.get('data')}, 200

        except Exception as e:
            return {'error': str(e)}, 500


@ns.route('/target/set')
class TargetSet(Resource):
    @ns.doc('set_target_data')
    @ns.expect(target_set_model)
    @ns.marshal_with(target_set_response_model)
    def post(self):
        """Set data on target"""
        payload = ns.payload
        sm_name = payload['sm_name']
        reg_type = payload['reg_type']
        offset_str = payload['offset']
        write_data = payload['write_data']

        try:
            offset = resolve_offset(offset_str)

            client = get_target_client()
            resp = client.set_data(sm_name, reg_type, offset, write_data)
            
            return {'seq': resp.get('seq'), 'offset': resp.get('offset'), 'written_data': resp.get('data')}, 200

        except Exception as e:
            return {'error': str(e)}, 500


# --- CSV Import/Export endpoints ---

@ns.route('/import-csv')
class ImportCSV(Resource):
    @ns.doc('import_csv')
    @ns.expect(csv_operation_model)
    def post(self):
        """Import alarm configuration from CSV files in specified directory"""
        data = request.get_json()
        directory = data.get('directory', 'csv_exports')
        
        # Remove trailing slash if present
        directory = directory.rstrip('/')

        try:
            # Check if directory exists
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
        
        # Remove trailing slash if present
        directory = directory.rstrip('/')

        try:
            # Create directory if it doesn't exist
            os.makedirs(directory, exist_ok=True)

            # Export all three CSV files
            export_alarm_definitions_to_csv(directory)
            export_alarm_level_actions_to_csv(directory)
            export_limits_values_to_csv(directory)

            return {'message': f'Successfully exported to {directory}/'}, 200

        except Exception as e:
            return {'message': f'Error exporting CSV: {str(e)}'}, 500


# --- JSON Import/Export endpoints ---

@ns.route('/export-json')
class ExportJSON(Resource):
    @ns.doc('export_json')
    @ns.expect(json_operation_model)
    def post(self):
        """Export alarm configuration to JSON file in the format with embedded actions"""
        data = request.get_json()
        directory = data.get('directory', 'json_exports')
        file_name = data.get('file_name', 'alarm_config.json')
        
        # Remove trailing slash if present
        directory = directory.rstrip('/')

        try:
            # Create directory if it doesn't exist
            os.makedirs(directory, exist_ok=True)

            # Fetch all config data
            ensure_all_alarms_have_records(verbose=True)
            
            alarm_defs = AlarmDefinition.query.order_by(AlarmDefinition.alarm_num).all()
            alarm_levels = AlarmLevelAction.query.order_by(AlarmLevelAction.alarm_num, AlarmLevelAction.level).all()
            limits_vals = LimitsValues.query.all()

            # Build JSON in the format: {"alarms": [...]}
            alarms = []
            
            # Create lookup maps
            alarm_name_lookup = {ad.alarm_num: ad.alarm_name for ad in alarm_defs}
            limits_lookup = {lv.limits_structure: lv for lv in limits_vals}
            
            # Group levels by alarm_num
            levels_by_alarm = {}
            for al in alarm_levels:
                if al.alarm_num not in levels_by_alarm:
                    levels_by_alarm[al.alarm_num] = []
                levels_by_alarm[al.alarm_num].append(al)
            
            # Build alarm objects
            for ad in alarm_defs:
                # Get limits array
                limits_array = [0, 0, 0, 0]
                if ad.limits_structure and ad.limits_structure in limits_lookup:
                    lv = limits_lookup[ad.limits_structure]
                    limits_array = [
                        lv.level1_limit or 0,
                        lv.level2_limit or 0,
                        lv.level3_limit or 0,
                        lv.hysteresis or 0
                    ]
                
                # Get actions
                actions_list = []
                if ad.alarm_num in levels_by_alarm:
                    for al in sorted(levels_by_alarm[ad.alarm_num], key=lambda x: x.level):
                        # Convert duration from seconds to "on:off" format
                        duration_str = al.duration or '2:3'
                        
                        # Parse actions JSON
                        try:
                            actions_data = json.loads(al.actions) if al.actions else []
                        except:
                            actions_data = []
                        
                        actions_list.append({
                            "level": al.level,
                            "level_enabled": al.enabled,
                            "duration": duration_str,
                            "actions": actions_data,
                            "notes": al.notes or ""
                        })
                
                alarm_obj = {
                    "num": ad.alarm_num,
                    "name": ad.alarm_name,
                    "levels": ad.num_levels,
                    "measured": ad.measured_variable or "",
                    "limits": limits_array,
                    "compare": ad.comparison_type or "greater_than",
                    "alarm_var": ad.alarm_variable or "",
                    "latch_var": ad.latched_variable or "",
                    "limits_var": ad.limits_structure or "",
                    "notes": ad.notes or "",
                    "actions": actions_list
                }
                alarms.append(alarm_obj)
            
            output = {"alarms": alarms}
            
            # Write to file
            file_path = os.path.join(directory, file_name)
            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(output, f, indent=2)

            return {'message': f'{file_path}'}, 200

        except Exception as e:
            return {'message': f'Error exporting JSON: {str(e)}'}, 500


@ns.route('/import-json')
class ImportJSON(Resource):
    @ns.doc('import_json')
    @ns.expect(json_operation_model)
    def post(self):
        """Import alarm configuration from JSON file with embedded actions format - SAFE VERSION"""
        data = request.get_json()
        directory = data.get('directory', 'json_exports')
        file_name = data.get('file_name', 'alarm_config.json')
        print(f"[import-json] Loading JSON from {directory}/{file_name}")
        
        # Remove trailing slash if present
        directory = directory.rstrip('/')

        try:
            # Check if directory exists
            if not os.path.isdir(directory):
                return {'message': f'Directory not found: {directory}'}, 404
            
            file_path = os.path.join(directory, file_name)
            if not os.path.exists(file_path):
                return {'message': f'JSON file not found: {file_path}'}, 404

            # STEP 1: Load and validate JSON structure BEFORE touching database
            print(f"[import-json] Loading JSON from {file_path}")
            with open(file_path, 'r', encoding='utf-8') as f:
                json_data = json.load(f)
            
            # Validate structure
            if not isinstance(json_data, dict):
                return {'message': 'Invalid JSON: expected object at root'}, 400
            
            if 'alarms' not in json_data:
                return {'message': 'Invalid JSON: missing "alarms" array'}, 400
            
            if not isinstance(json_data['alarms'], list):
                return {'message': 'Invalid JSON: "alarms" must be an array'}, 400
            
            if len(json_data['alarms']) == 0:
                return {'message': 'Invalid JSON: "alarms" array is empty - refusing to wipe database'}, 400

            alarms_data = json_data['alarms']
            print(f"[import-json] Found {len(alarms_data)} alarms in JSON")

            # STEP 2: Parse and validate all alarms BEFORE deleting anything
            parsed_alarms = []
            for idx, alarm in enumerate(alarms_data):
                try:
                    alarm_num = alarm.get('num')
                    alarm_name = alarm.get('name', f'Alarm {alarm_num}')
                    num_levels = alarm.get('levels', 1)
                    measured_variable = alarm.get('measured', '')
                    limits_structure = alarm.get('limits_var', '')
                    comparison_type = normalize_comparison_type(alarm.get('compare', 'greater_than'))
                    alarm_variable = alarm.get('alarm_var', '')
                    latched_variable = alarm.get('latch_var', '')
                    notes = alarm.get('notes', '')
                    
                    # Parse limits array [level1, level2, level3, hysteresis]
                    limits = alarm.get('limits', [0, 0, 0, 0])
                    if not isinstance(limits, list) or len(limits) < 4:
                        limits = [0, 0, 0, 0]
                    
                    # Parse actions
                    actions = alarm.get('actions', [])
                    parsed_actions = []
                    for action in actions:
                        level = action.get('level', 1)
                        enabled = action.get('level_enabled', True)
                        duration_str = action.get('duration', '4:3')
                                                
                        actions_json = json.dumps(action.get('actions', []))
                        action_notes = action.get('notes', '')
                        
                        parsed_actions.append({
                            'level': level,
                            'enabled': enabled,
                            'duration': duration_str,
                            'actions': actions_json,
                            'notes': action_notes
                        })
                    
                    parsed_alarms.append({
                        'alarm_num': alarm_num,
                        'alarm_name': alarm_name,
                        'num_levels': num_levels,
                        'measured_variable': measured_variable,
                        'limits_structure': limits_structure,
                        'comparison_type': comparison_type,
                        'alarm_variable': alarm_variable,
                        'latched_variable': latched_variable,
                        'notes': notes,
                        'limits': limits,
                        'actions': parsed_actions
                    })
                    
                except Exception as e:
                    return {'message': f'Error parsing alarm at index {idx}: {str(e)}'}, 400

            print(f"[import-json] Successfully parsed {len(parsed_alarms)} alarms")

            # STEP 3: Now that everything is validated, delete and rebuild database
            print("[import-json] Deleting existing records...")
            AlarmLevelAction.query.delete()
            LimitsValues.query.delete()
            AlarmDefinition.query.delete()

            created_limits = set()

            # STEP 4: Insert all parsed data
            print("[import-json] Inserting new records...")
            for alarm_data in parsed_alarms:
                # Create alarm definition
                ad = AlarmDefinition(
                    alarm_num=alarm_data['alarm_num'],
                    alarm_name=alarm_data['alarm_name'],
                    num_levels=alarm_data['num_levels'],
                    measured_variable=alarm_data['measured_variable'],
                    limits_structure=alarm_data['limits_structure'],
                    comparison_type=alarm_data['comparison_type'],
                    alarm_variable=alarm_data['alarm_variable'],
                    latched_variable=alarm_data['latched_variable'],
                    notes=alarm_data['notes']
                )
                db.session.add(ad)

                # Create alarm_level_actions
                for action in alarm_data['actions']:
                    ala = AlarmLevelAction(
                        alarm_num=alarm_data['alarm_num'],
                        level=action['level'],
                        enabled=action['enabled'],
                        duration=action['duration'],
                        actions=action['actions'],
                        notes=action['notes']
                    )
                    db.session.add(ala)

                # Create limits_values if limits_structure exists
                limits_structure = alarm_data['limits_structure']
                if limits_structure and limits_structure.strip() and limits_structure not in created_limits:
                    lv = LimitsValues(
                        limits_structure=limits_structure,
                        level1_limit=alarm_data['limits'][0],
                        level2_limit=alarm_data['limits'][1],
                        level3_limit=alarm_data['limits'][2],
                        hysteresis=alarm_data['limits'][3],
                        last_updated=datetime.utcnow().isoformat(),
                        notes=''
                    )
                    db.session.add(lv)
                    created_limits.add(limits_structure)

            db.session.commit()
            print(f"[import-json] Successfully imported {len(parsed_alarms)} alarms")

            return {'message': f'Successfully imported {len(parsed_alarms)} alarm(s) from {file_path}'}, 200

        except json.JSONDecodeError as e:
            return {'message': f'Invalid JSON file: {str(e)}'}, 400
        except Exception as e:
            db.session.rollback()
            print(f"[import-json] ERROR: {str(e)}")
            return {'message': f'Error importing JSON: {str(e)}'}, 500


# @ns.route('/import-json')
# class ImportJSON(Resource):
#     @ns.doc('import_json')
#     @ns.expect(json_operation_model)
#     def post(self):
#         """Import alarm configuration from JSON file with embedded actions format"""
#         data = request.get_json()
#         directory = data.get('directory', 'json_exports')
#         file_name = data.get('file_name', 'alarm_config.json')
        
#         # Remove trailing slash if present
#         directory = directory.rstrip('/')

#         try:
#             # Check if directory exists
#             if not os.path.isdir(directory):
#                 return {'message': f'Directory not found: {directory}'}, 404
            
#             file_path = os.path.join(directory, file_name)
#             if not os.path.exists(file_path):
#                 return {'message': f'JSON file not found: {file_path}'}, 404
#                 # Validate JSON structure BEFORE touching the database
#             with open(file_path, 'r', encoding='utf-8') as f:
#                 json_data = json.load(f)
            
#             # Check if it has the expected structure
#             if not isinstance(json_data, dict):
#                 return {'message': 'Invalid JSON: expected object at root'}, 400
            
#             if 'alarms' not in json_data:
#                 return {'message': 'Invalid JSON: missing "alarms" array'}, 400
            
#             if not isinstance(json_data['alarms'], list):
#                 return {'message': 'Invalid JSON: "alarms" must be an array'}, 400
            
#             if len(json_data['alarms']) == 0:
#                 return {'message': 'Invalid JSON: "alarms" array is empty'}, 400

#             # Use the loader from alarm_utils
#             from alarm_utils import load_alarm_defs_from_json
#             load_alarm_defs_from_json(file_path, verbose=True)

#             return {'message': f'Successfully imported from {file_path}'}, 200

#         except Exception as e:
#             return {'message': f'Error importing JSON: {str(e)}'}, 500





# @ns.route('/export-json')
# class ExportJSON(Resource):
#     @ns.doc('export_json')
#     @ns.expect(json_operation_model)
#     def post(self):
#         """Export alarm configuration to a JSON file"""
#         data = request.get_json()
#         directory = data.get('directory', 'json_exports')
#         file_name = data.get('file_name', 'alarm_config.json')
        
#         # Remove trailing slash if present
#         directory = directory.rstrip('/')

#         try:
#             # Create directory if it doesn't exist
#             os.makedirs(directory, exist_ok=True)

#             # Get all alarm configuration data
#             alarm_defs = AlarmDefinition.query.all()
#             alarm_levels = AlarmLevelAction.query.all()
#             limits_vals = LimitsValues.query.all()

#             # Create a lookup for alarm_name by alarm_num
#             alarm_name_lookup = {ad.alarm_num: ad.alarm_name for ad in alarm_defs}

#             # Prepare alarm_level_actions with alarm_name
#             prepared_alarm_levels = []
#             for al in alarm_levels:
#                 level_dict = al.to_dict()
#                 level_dict['alarm_name'] = alarm_name_lookup.get(al.alarm_num, 'N/A')
#                 prepared_alarm_levels.append(level_dict)

#             # Prepare limits_values with alarm_name
#             limits_structure_to_alarm_name = {}
#             for ad in alarm_defs:
#                 if ad.limits_structure and ad.limits_structure not in limits_structure_to_alarm_name:
#                     limits_structure_to_alarm_name[ad.limits_structure] = ad.alarm_name

#             prepared_limits_vals = []
#             for lv in limits_vals:
#                 limits_dict = lv.to_dict()
#                 limits_dict['alarm_name'] = limits_structure_to_alarm_name.get(lv.limits_structure, 'N/A')
#                 prepared_limits_vals.append(limits_dict)

#             # Build the complete config
#             config_data = {
#                 'alarm_definitions': [ad.to_dict() for ad in alarm_defs],
#                 'alarm_level_actions': prepared_alarm_levels,
#                 'limits_values': prepared_limits_vals
#             }

#             # Write to JSON file
#             file_path = os.path.join(directory, file_name)
#             with open(file_path, 'w', encoding='utf-8') as f:
#                 json.dump(config_data, f, indent=2)

#             return {'message': f'Successfully exported to {file_path}'}, 200

#         except Exception as e:
#             return {'message': f'Error exporting JSON: {str(e)}'}, 500


# --- Helper functions ---

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
    - Otherwise, look up in data_definitions table by name
    """
    offset_str = str(offset_str).strip()
    
    # Try hex
    if offset_str.startswith("0x") or offset_str.startswith("0X"):
        try:
            return int(offset_str, 16)
        except ValueError:
            raise ValueError(f"Invalid hex offset: {offset_str}")
    
    # Try decimal
    try:
        return int(offset_str)
    except ValueError:
        pass
    
    # Look up in data_definitions by name
    try:
        dd = DataDefinition.query.filter_by(item_name=offset_str).first()
        if dd and dd.offset is not None:
            print(f"[resolve_offset] Resolved '{offset_str}' -> {dd.offset} from DataDefinition")
            return dd.offset
    except Exception as e:
        print(f"[resolve_offset] Error querying DataDefinition: {e}")
    
    # If all else fails, raise error
    raise ValueError(f"Cannot resolve offset '{offset_str}': not a number, not hex, and not found in data_definitions table")


# ====
# NEW: pub_defs namespace
# Simple file-based pub_definition.json handling
# ====

pub_ns = Namespace('pub_defs', description='Publication definition operations')

# Recursive model for variable items (groups and leaves)
var_item_model = pub_ns.model('VarItem', {
    'name': fields.String(description='Group or variable name'),
    'src': fields.String(description='Source reference (for leaf items)'),
    'type': fields.String(description='Data type (e.g., date, int, float)'),
    'fmt': fields.String(description='Format string'),
    'example': fields.String(description='Example value'),
    'items': fields.List(fields.Raw, description='Nested items (tables, groups, or variables)')
})

pub_def_model = pub_ns.model('PubDefinition', {
    'pub': fields.String(required=True, description='Publication name (e.g., config)'),
    'items': fields.List(fields.Raw, description='Top-level items (tables, groups, variables)')
})

save_pub_def_model = pub_ns.model('SavePubDefinition', {
    'file_name': fields.String(required=True, description='Output JSON file name (e.g., hithium_vars.json)'),
    'directory': fields.String(description='Directory to save into (default: "pub_definitions")', default='pub_definitions'),
    'data': fields.Nested(pub_def_model, required=True, description='Publication definition JSON')
})

load_pub_def_model = pub_ns.model('LoadPubDefinition', {
    'file_name': fields.String(required=True, description='JSON file name to load (e.g., hithium_vars.json)'),
    'directory': fields.String(description='Directory to load from (default: "pub_definitions")', default='pub_definitions')
})

load_var_list_model = pub_ns.model('LoadVarList', {
    'file_name': fields.String(required=True, description='JSON variable list file (e.g., hithium_vars.json)')
})

save_var_list_model = pub_ns.model('SaveVarList', {
    'file_name': fields.String(required=True, description='JSON variable list file name'),
    'data': fields.Raw(required=True, description='Variable list data (array of tables)')
})


@pub_ns.route('/save')
class SavePubDefinition(Resource):
    @pub_ns.doc('save_pub_definition')
    @pub_ns.expect(save_pub_def_model)
    def post(self):
        """Save a publication definition JSON to disk."""
        payload = request.get_json()
        file_name = payload.get('file_name')
        directory = payload.get('directory', 'pub_definitions')
        data_to_save = payload.get('data')

        if not file_name or not data_to_save:
            return {'message': 'file_name and data are required'}, 400

        try:
            os.makedirs(directory, exist_ok=True)
            file_path = os.path.join(directory, file_name)

            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data_to_save, f, indent=4)

            return {'message': f'Publication definition saved to {file_path}'}, 200

        except Exception as e:
            return {'message': f'Error saving publication definition: {str(e)}'}, 500


@pub_ns.route('/list')
class ListPubDefinitions(Resource):
    @pub_ns.doc('list_pub_definitions')
    def get(self):
        """
        List available publication/variable list JSON files in the var_lists directory.
        """
        base_dir = os.path.dirname(os.path.abspath(__file__))
        var_list_dir = os.path.join(base_dir, 'var_lists')

        try:
            if not os.path.isdir(var_list_dir):
                return {'files': [], 'directory': var_list_dir, 'message': 'Directory does not exist'}, 200

            files = [
                f for f in os.listdir(var_list_dir)
                if f.lower().endswith('.json') and os.path.isfile(os.path.join(var_list_dir, f))
            ]
            return {'files': files, 'directory': var_list_dir}, 200

        except Exception as e:
            return {'message': f'Error listing var_lists: {str(e)}'}, 500


@pub_ns.route('/load_var_list')
class LoadVarList(Resource):
    @pub_ns.doc('load_var_list')
    @pub_ns.expect(load_var_list_model)
    def post(self):
        """
        Load a variable list JSON from var_lists directory.
        This is a convenience wrapper around /load with fixed directory = var_lists.
        """
        payload = request.get_json()
        file_name = payload.get('file_name')
        if not file_name:
            return {'message': 'file_name is required'}, 400

        base_dir = os.path.dirname(os.path.abspath(__file__))
        directory = os.path.join(base_dir, 'var_lists')
        file_path = os.path.join(directory, file_name)

        try:
            if not os.path.exists(file_path):
                return {'message': f'File not found: {file_path}'}, 404

            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            return data, 200

        except json.JSONDecodeError:
            return {'message': f'Invalid JSON in file: {file_path}'}, 400
        except Exception as e:
            return {'message': f'Error loading var list: {str(e)}'}, 500


@pub_ns.route('/save_var_list')
class SaveVarList(Resource):
    @pub_ns.doc('save_var_list')
    @pub_ns.expect(save_var_list_model)
    def post(self):
        """Save a variable list JSON to var_lists directory."""
        payload = request.get_json()
        file_name = payload.get('file_name')
        data = payload.get('data')

        if not file_name or data is None:
            return {'message': 'file_name and data are required'}, 400

        base_dir = os.path.dirname(os.path.abspath(__file__))
        directory = os.path.join(base_dir, 'var_lists')
        os.makedirs(directory, exist_ok=True)
        file_path = os.path.join(directory, file_name)

        try:
            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)
            return {'message': f'Var list saved: {file_name}', 'file_name': file_name}, 200
        except Exception as e:
            return {'message': f'Error saving var list: {str(e)}'}, 500


@pub_ns.route('/load')
class LoadPubDefinition(Resource):
    @pub_ns.doc('load_pub_definition')
    @pub_ns.expect(load_pub_def_model)
    def post(self):
        """Load a publication definition JSON from disk."""
        payload = request.get_json()
        file_name = payload.get('file_name')
        directory = payload.get('directory', 'pub_definitions')

        if not file_name:
            return {'message': 'file_name is required'}, 400

        try:
            file_path = os.path.join(directory, file_name)

            if not os.path.exists(file_path):
                return {'message': f'File not found: {file_path}'}, 404

            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            # We don't enforce pub_def_model on the way out; just return JSON
            return data, 200

        except json.JSONDecodeError:
            return {'message': f'Invalid JSON in file: {file_path}'}, 400
        except Exception as e:
            return {'message': f'Error loading publication definition: {str(e)}'}, 500