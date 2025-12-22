Perfect! Now I'll create the updated system that uses **JSON instead of CSV** and follows the **Target tab model** with a dropdown for alarm lists. We can potentially skip the DB entirely and work with in-memory lists.

Let me create the updated files:

---

## 1. Updated `api_endpoints.py` - Add JSON endpoints

Add these new endpoints to the `ns` (alarms) namespace:

```python
# Add these models at the top with other models
alarm_json_list_model = ns.model('AlarmJsonList', {
    'files': fields.List(fields.String, description='Available alarm definition JSON files'),
    'directory': fields.String(description='Directory path')
})

alarm_json_load_model = ns.model('AlarmJsonLoad', {
    'file_name': fields.String(required=True, description='JSON file name to load')
})

alarm_json_save_model = ns.model('AlarmJsonSave', {
    'file_name': fields.String(required=True, description='JSON file name to save'),
    'data': fields.Raw(required=True, description='Alarm definitions JSON data')
})

# Add these endpoints after the existing CSV endpoints

@ns.route('/alarm-lists')
class AlarmLists(Resource):
    @ns.doc('list_alarm_jsons')
    @ns.marshal_with(alarm_json_list_model)
    def get(self):
        """List available alarm definition JSON files"""
        base_dir = os.path.dirname(os.path.abspath(__file__))
        alarm_lists_dir = os.path.join(base_dir, 'alarm_lists')
        
        try:
            if not os.path.isdir(alarm_lists_dir):
                os.makedirs(alarm_lists_dir, exist_ok=True)
                return {'files': [], 'directory': alarm_lists_dir}, 200
            
            files = [
                f for f in os.listdir(alarm_lists_dir)
                if f.lower().endswith('.json') and os.path.isfile(os.path.join(alarm_lists_dir, f))
            ]
            return {'files': sorted(files), 'directory': alarm_lists_dir}, 200
        
        except Exception as e:
            return {'message': f'Error listing alarm lists: {str(e)}'}, 500


@ns.route('/load-alarm-list')
class LoadAlarmList(Resource):
    @ns.doc('load_alarm_list')
    @ns.expect(alarm_json_load_model)
    def post(self):
        """Load an alarm definition JSON file"""
        payload = request.get_json()
        file_name = payload.get('file_name')
        
        if not file_name:
            return {'message': 'file_name is required'}, 400
        
        base_dir = os.path.dirname(os.path.abspath(__file__))
        alarm_lists_dir = os.path.join(base_dir, 'alarm_lists')
        file_path = os.path.join(alarm_lists_dir, file_name)
        
        try:
            if not os.path.exists(file_path):
                return {'message': f'File not found: {file_path}'}, 404
            
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            return data, 200
        
        except json.JSONDecodeError:
            return {'message': f'Invalid JSON in file: {file_path}'}, 400
        except Exception as e:
            return {'message': f'Error loading alarm list: {str(e)}'}, 500


@ns.route('/save-alarm-list')
class SaveAlarmList(Resource):
    @ns.doc('save_alarm_list')
    @ns.expect(alarm_json_save_model)
    def post(self):
        """Save an alarm definition JSON file"""
        payload = request.get_json()
        file_name = payload.get('file_name')
        data = payload.get('data')
        
        if not file_name or not data:
            return {'message': 'file_name and data are required'}, 400
        
        # Ensure .json extension
        if not file_name.endswith('.json'):
            file_name += '.json'
        
        base_dir = os.path.dirname(os.path.abspath(__file__))
        alarm_lists_dir = os.path.join(base_dir, 'alarm_lists')
        os.makedirs(alarm_lists_dir, exist_ok=True)
        
        file_path = os.path.join(alarm_lists_dir, file_name)
        
        try:
            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)
            
            return {'message': f'Alarm list saved: {file_name}', 'file_name': file_name}, 200
        
        except Exception as e:
            return {'message': f'Error saving alarm list: {str(e)}'}, 500
```

---

## 2. Updated `index.html` - Replace alarm tabs with JSON-based UI

Replace the entire `<body>` section with this updated version:Let me create complete updated files for you: