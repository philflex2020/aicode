# app.py
import os
from flask import Flask
from flask_sqlalchemy import SQLAlchemy
from flask_restx import Api

# Initialize SQLAlchemy outside of app creation for models
db = SQLAlchemy()

def create_app():
    app = Flask(__name__)

    # --- Configuration ---
    app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///instance/alarms.db'  # Default DB for alarms
    app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
    app.config['SQLALCHEMY_BINDS'] = {
        'data': 'sqlite:///instance/data.db',  # Separate DB for data definitions
    }
    app.config['RESTX_VALIDATE'] = True # Enable validation for Flask-RESTX
    app.config['RESTX_SWAGGER_UI_DOC_EXPANSION'] = 'list' # For Swagger UI

    # Ensure instance folder exists
    instance_path = os.path.join(app.root_path, 'instance')
    os.makedirs(instance_path, exist_ok=True)

    # Initialize extensions
    db.init_app(app)
    api = Api(app, version='1.0', title='Alarm Management API',
              description='A RESTful API for managing alarm definitions, levels, and limits.')

    # Import models (must be done after db is initialized with app)
    from alarm_models import DataDefinition, AlarmDefinition, AlarmLevelAction, LimitsValues, AlarmHistory

    # --- Database Creation ---
    with app.app_context():
        # Create tables for the default bind (alarms.db)
        db.create_all()
        # Create tables for the 'data' bind (data.db)
        db.create_all(bind='data')
        
        # --- Bootstrap from CSV and JSON if databases are empty ---
        bootstrap_databases(app)

    # Import and register API namespaces
    from api_endpoints import ns as alarm_namespace
    api.add_namespace(alarm_namespace, path='/api/alarms')

    return app


def bootstrap_databases(app):
    """
    Bootstrap the databases from initial CSV and JSON files if they're empty.
    Expected files:
      - alarm_definition.csv (in project root or csv_exports/)
      - data_definition.json (in project root)
    """
    from alarm_models import AlarmDefinition, DataDefinition
    from alarm_utils import load_alarm_defs_from_csv, load_data_defs_from_json
    
    # Check if alarms.db is empty
    alarm_count = AlarmDefinition.query.count()
    if alarm_count == 0:
        print("🔄 alarms.db is empty, bootstrapping from alarm_definition.csv...")
        
        # Look for alarm_definition.csv in multiple locations
        csv_paths = [
            os.path.join(app.root_path, 'alarm_definition.csv'),
            os.path.join(app.root_path, 'csv_exports', 'alarm_definition.csv'),
        ]
        
        csv_file = None
        for path in csv_paths:
            if os.path.exists(path):
                csv_file = path
                break
        
        if csv_file:
            print(f"   Found: {csv_file}")
            load_alarm_defs_from_csv(csv_file, verbose=True)
        else:
            print(f"   ⚠️  alarm_definition.csv not found in: {csv_paths}")
    else:
        print(f"✅ alarms.db already has {alarm_count} alarm(s), skipping bootstrap.")
    
    # Check if data.db is empty
    data_count = DataDefinition.query.count()
    if data_count == 0:
        print("🔄 data.db is empty, bootstrapping from data_definitions.json...")
        
        # Look for data_definitions.json in project root
        json_paths = [
            os.path.join(app.root_path, 'data_definition.json'),
            os.path.join(app.root_path, 'data_defs.json'),
        ]
        
        json_file = None
        for path in json_paths:
            if os.path.exists(path):
                json_file = path
                break
        
        if json_file:
            print(f"   Found: {json_file}")
            load_data_defs_from_json(json_file, verbose=True)
        else:
            print(f"   ⚠️  data_definition.json not found in: {json_paths}")
    else:
        print(f"✅ data.db already has {data_count} data definition(s), skipping bootstrap.")


if __name__ == '__main__':
    app = create_app()
    app.run('0.0.0.0', debug=True, port=5002) # Run on port 5002