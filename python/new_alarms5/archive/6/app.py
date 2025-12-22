# app.py
import os
from flask import Flask, send_from_directory
from flask_restx import Api
from extensions import db

def create_app():
    app = Flask(__name__)

    # --- Paths / directories ---
    base_dir = app.root_path                    # e.g. /home/sysdcs/work/new_alarms3
    instance_dir = os.path.join(base_dir, 'instance')
    os.makedirs(instance_dir, exist_ok=True)    # Needed for SQLite

    # --- Database URIs (absolute paths) ---
    alarms_db_path = os.path.join(instance_dir, 'alarms.db')
    data_db_path   = os.path.join(instance_dir, 'data.db')

    app.config['SQLALCHEMY_DATABASE_URI'] = f'sqlite:///{alarms_db_path}'
    app.config['SQLALCHEMY_BINDS'] = {
        'data': f'sqlite:///{data_db_path}',
    }
    app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
    # app.config['RESTX_VALIDATE'] = True
    app.config['RESTX_VALIDATE'] = False
    app.config['RESTX_SWAGGER_UI_DOC_EXPANSION'] = 'list'

    db.init_app(app)

    # Swagger UI at /api/
    api = Api(
        app,
        version='1.0',
        title='Alarm Management API',
        description='Alarm definitions, levels, limits, and data defs',
        doc='/api/'
    )

    # Import models after db.init_app
    from alarm_models import (
        DataDefinition,
        AlarmDefinition,
        AlarmLevelAction,
        LimitsValues,
        AlarmHistory
    )

    # Create tables + bootstrap
    with app.app_context():
        db.create_all()            # creates tables for all models/binds
        bootstrap_databases(app)

    # Register API namespace
    from api_endpoints import ns as alarm_namespace, pub_ns

    api.add_namespace(alarm_namespace, path='/api/alarms')
    api.add_namespace(pub_ns, path='/api/pub_defs')

    # Register web UI route(s)
    register_routes(app)

    return app


def register_routes(app):
    """
    Register web UI routes (served from static/ folder)
    """
    @app.route('/')
    @app.route('/index')
    def index():
        """
        Serve the compact CSV editor page.
        Looks for index.html in a 'static' folder next to app.py.
        """
        base_dir = app.root_path
        static_dir = os.path.join(base_dir, 'static')
        return send_from_directory(static_dir, 'index.html')


def bootstrap_databases(app):
    """
    Create initial contents from alarm_definition.csv and data_definition.json
    if the respective tables are empty.
    """
    from alarm_models import AlarmDefinition, DataDefinition
    from alarm_utils import load_alarm_defs_from_csv, load_data_defs_from_json

    base_dir = app.root_path

    # --- alarms.db bootstrap ---
    alarm_count = AlarmDefinition.query.count()
    if alarm_count == 0:
        print("🔄 alarms.db is empty, bootstrapping from alarm_definition.csv...")
        csv_paths = [
            os.path.join(base_dir, 'alarm_definition.csv'),
            os.path.join(base_dir, 'csv_exports', 'alarm_definition.csv'),
        ]
        csv_file = next((p for p in csv_paths if os.path.exists(p)), None)
        if csv_file:
            print(f"   Found: {csv_file}")
            load_alarm_defs_from_csv(csv_file, verbose=True)
        else:
            print(f"   ⚠️  alarm_definition.csv not found in: {csv_paths}")
    else:
        print(f"✅ alarms.db already has {alarm_count} alarm(s), skipping bootstrap.")

    # --- data.db bootstrap ---
    data_count = DataDefinition.query.count()
    if data_count == 0:
        print("🔄 data.db is empty, bootstrapping from data_definition.json...")
        json_paths = [
            os.path.join(base_dir, 'data_definition.json'),
            os.path.join(base_dir, 'data_definitions.json'),
        ]
        json_file = next((p for p in json_paths if os.path.exists(p)), None)
        if json_file:
            print(f"   Found: {json_file}")
            load_data_defs_from_json(json_file, verbose=True)
        else:
            print(f"   ⚠️  data_definition.json not found in: {json_paths}")
    else:
        print(f"✅ data.db already has {data_count} data definition(s), skipping bootstrap.")


if __name__ == '__main__':
    app = create_app()
    print("Available routes:")
    for rule in app.url_map.iter_rules():
        print(" ", rule)
    app.run(debug=True, host='0.0.0.0', port=5003)
    