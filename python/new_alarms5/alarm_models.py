# alarm_models.py
from extensions import db
from datetime import datetime

class DataDefinition(db.Model):
    """
    Defines data points in the system (measured variables and limits structures)
    """
    __tablename__ = 'data_definitions'
    __bind_key__ = 'data' # Bind to the 'data' database

    id = db.Column(db.Integer, primary_key=True)
    table_name = db.Column(db.String(100), nullable=False)  # e.g., 'sbmu:input', 'rtos:hold'
    item_name = db.Column(db.String(100), nullable=False)   # e.g., 'Max_cell_temp', 'at_cell_voltage_high'
    offset = db.Column(db.Integer, nullable=False)          # Modbus register offset
    size = db.Column(db.Integer, nullable=False)            # 1 for single value, 4 for u16x4 limits
    type = db.Column(db.String(20))                         # 'i16', 'u16', 'u16x4', etc.
    notes = db.Column(db.Text)
    
    @property
    def full_path(self):
        """Returns the full path like 'sbmu:input:Max_cell_temp'"""
        return f"{self.table_name}:{self.item_name}"
    
    def to_dict(self):
        return {
            'id': self.id,
            'table_name': self.table_name,
            'item_name': self.item_name,
            'full_path': self.full_path,
            'offset': self.offset,
            'size': self.size,
            'type': self.type,
            'notes': self.notes
        }

class LimitsDef(db.Model):
    __tablename__ = 'limits_def'

    id = db.Column(db.Integer, primary_key=True)
    # limits_structure = db.Column(db.String, nullable=False, index=True)
    table_name = db.Column(db.String(100), nullable=False)  # e.g., 'sbmu:input', 'rtos:hold'
    name = db.Column(db.String, nullable=False)
    sm_name = db.Column(db.String)
    reg_type = db.Column(db.String)
    offset = db.Column(db.String)  # or Integer if appropriate
    num = db.Column(db.Integer)
    write_data = db.Column(db.JSON)  # or Text storing JSON string
    read_data = db.Column(db.JSON)   # or Text storing JSON string

    def to_dict(self):
        # Parse JSON strings to Python lists if needed
        def parse_json_field(field):
            if field is None:
                return []
            if isinstance(field, str):
                try:
                    return json.loads(field)
                except json.JSONDecodeError:
                    return []
            return field  # already a list/dict

        return {
            # 'table': self.table_name,
            'name': self.name,
            'sm_name': self.sm_name,
            'reg_type': self.reg_type,
            'offset': self.offset,
            'num': self.num,
            'write_data': parse_json_field(self.write_data),
            'read_data': parse_json_field(self.read_data),
        }

    # def to_dict(self):
    #     return {
    #         'table_name': self.table_name,
    #         'name': self.name,
    #         'sm_name': self.sm_name,
    #         'reg_type': self.reg_type,
    #         'offset': self.offset,
    #         'num': self.num,
    #         'write_data': self.write_data,
    #         'read_data': self.read_data,
    #     }


class AlarmDefinition(db.Model):
    """
    Defines an alarm with its measured variable and limits structure
    """
    __tablename__ = 'alarm_definitions'
    
    id = db.Column(db.Integer, primary_key=True)
    alarm_num = db.Column(db.Integer, unique=True, nullable=False)
    alarm_name = db.Column(db.String(200), nullable=False)

    num_levels = db.Column(db.Integer, default=3)

    measured_variable = db.Column(db.String(200))           # e.g., 'sbmu:input:Max_cell_temp'
    limits_structure = db.Column(db.String(200))            # e.g., 'rtos:hold:at_single_charge_temp_over'
    comparison_type = db.Column(db.String(20))              # 'greater_than', 'less_than', etc.

    alarm_variable = db.Column(db.String(200))
    latched_variable = db.Column(db.String(200))
    limits_def = db.Column(db.String(200))            # e.g., 'Terminal Over Temp'

    notes = db.Column(db.Text)
    
    # Relationship to levels
    levels = db.relationship('AlarmLevelAction', backref='alarm', cascade='all, delete-orphan',
                             primaryjoin="AlarmDefinition.alarm_num == AlarmLevelAction.alarm_num")
    
    def to_dict(self):
        return {
            'id'                : self.id,
            'alarm_num'         : self.alarm_num,
            'alarm_name'        : self.alarm_name,
            'num_levels'        : self.num_levels,
            'measured_variable' : self.measured_variable,
            'limits_structure'  : self.limits_structure,
            'comparison_type'   : self.comparison_type,
            'alarm_variable'    : self.alarm_variable,
            'latched_variable'  : self.latched_variable,
            'notes'             : self.notes,
            # Include the nested limit_def dict here
            'limits_def'        : self.limits_def,# if self.limit_defs else {}
            # Include other nested fields like actions, levels if needed
        }

    def sync_levels(self):
        """Ensure alarm_level_actions has exactly num_levels rows"""
        existing = {level.level: level for level in self.levels}
        
        # Create missing levels
        for lvl in range(1, self.num_levels + 1):
            if lvl not in existing:
                new_level = AlarmLevelAction(
                    alarm_num=self.alarm_num,
                    level=lvl,
                    enabled=True,
                    duration="3:3",
                    actions='',
                    notes=''
                )
                db.session.add(new_level)
        
        # Delete extra levels
        for lvl in list(existing.keys()):
            if lvl > self.num_levels:
                db.session.delete(existing[lvl])

    def ensure_limits_structure(self):
        """Create limits_values row if it doesn't exist"""
        if not self.limits_structure:
            return
        
        existing = LimitsValues.query.filter_by(limits_structure=self.limits_structure).first()
        if not existing:
            new_limits = LimitsValues(
                limits_structure=self.limits_structure,
                level1_limit=0.0,
                level2_limit=0.0,
                level3_limit=0.0,
                hysteresis=0.0,
                last_updated=datetime.utcnow().isoformat(),
                notes=f'Auto-created for alarm {self.alarm_num}'
            )
            db.session.add(new_limits)


class AlarmLevelAction(db.Model):
    """
    Defines actions and settings for each alarm level (1, 2, 3)
    """
    __tablename__ = 'alarm_level_actions'
    
    id = db.Column(db.Integer, primary_key=True)
    alarm_num = db.Column(db.Integer, db.ForeignKey('alarm_definitions.alarm_num'), nullable=False)
    level = db.Column(db.Integer, nullable=False)           # 1, 2, or 3
    enabled = db.Column(db.Boolean, default=True) 
    duration = db.Column(db.Text, default='3:3')            # On:Off Duration in seconds
    actions = db.Column(db.Text)                            # Comma-separated actions
    notes = db.Column(db.Text)
    
    __table_args__ = (
        db.UniqueConstraint('alarm_num', 'level', name='unique_alarm_level'),
    )
    
    def to_dict(self):
        return {
            'id': self.id,
            'alarm_num': self.alarm_num,
            'level': self.level,
            'enabled': self.enabled,
            'duration': self.duration,
            'actions': self.actions,
            'notes': self.notes
        }


class LimitsValues(db.Model):
    """
    Stores the actual threshold values for limits structures
    """
    __tablename__ = 'limits_values'
    
    id = db.Column(db.Integer, primary_key=True)
    limits_structure = db.Column(db.String(200), unique=True, nullable=False)  # e.g., 'rtos:hold:at_cell_voltage_high'
    level1_limit = db.Column(db.Float)
    level2_limit = db.Column(db.Float)
    level3_limit = db.Column(db.Float)
    hysteresis = db.Column(db.Float)
    last_updated = db.Column(db.String(50))                 # ISO timestamp
    notes = db.Column(db.Text)
    
    def to_dict(self):
        return {
            'id': self.id,
            'limits_structure': self.limits_structure,
            'level1_limit': self.level1_limit,
            'level2_limit': self.level2_limit,
            'level3_limit': self.level3_limit,
            'hysteresis': self.hysteresis,
            'last_updated': self.last_updated,
            'notes': self.notes
        }


class AlarmHistory(db.Model):
    """
    Records alarm events (triggered, cleared, etc.)
    """
    __tablename__ = 'alarm_history'
    
    id = db.Column(db.Integer, primary_key=True)
    alarm_num = db.Column(db.Integer, nullable=False)
    level = db.Column(db.Integer)
    event_type = db.Column(db.String(50))                   # 'triggered', 'cleared', 'acknowledged'
    timestamp = db.Column(db.String(50), nullable=False)    # ISO timestamp
    measured_value = db.Column(db.Float)
    threshold_value = db.Column(db.Float)
    notes = db.Column(db.Text)
    
    def to_dict(self):
        return {
            'id': self.id,
            'alarm_num': self.alarm_num,
            'level': self.level,
            'event_type': self.event_type,
            'timestamp': self.timestamp,
            'measured_value': self.measured_value,
            'threshold_value': self.threshold_value,
            'notes': self.notes
        }