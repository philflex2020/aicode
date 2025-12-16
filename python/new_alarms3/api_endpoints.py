# api_endpoints.py
import os
from datetime import datetime
from flask import request
from flask_restx import Namespace, Resource, fields
from extensions import db
from alarm_models import AlarmDefinition, AlarmLevelAction, LimitsValues
from alarm_utils import (
    load_alarm_defs_from_csv,
    export_alarm_definitions_to_csv,
    export_alarm_level_actions_to_csv,
    export_limits_values_to_csv,
    ensure_all_alarms_have_records,
    normalize_comparison_type,
    CSV_ALARM_DEF_FILE
)

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
        print("[/config POST] Incoming JSON:", data)  # <--- add this

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
                db.session.flush()  # Get the alarm_num if it's auto-generated

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

                # Auto-generate limits_values if limits_structure is specified and doesn't exist yet
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

            # Now handle any manually edited alarm_level_actions from the UI
            # (merge with auto-generated ones)
            provided_levels = data.get('alarm_level_actions', [])
            if provided_levels:
                # Delete auto-generated ones and replace with provided
                AlarmLevelAction.query.delete()
                for ala_data in provided_levels:
                    ala_data_clean = {k: v for k, v in ala_data.items() if k != 'alarm_name'}
                    ala = AlarmLevelAction(**ala_data_clean)
                    db.session.add(ala)

            # Handle any manually edited limits_values from the UI
            provided_limits = data.get('limits_values', [])
            if provided_limits:
                # Delete auto-generated ones and replace with provided
                LimitsValues.query.delete()
                for lv_data in provided_limits:
                    lv_data_clean = {k: v for k, v in lv_data.items() if k != 'alarm_name'}
                    # Update last_updated timestamp
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