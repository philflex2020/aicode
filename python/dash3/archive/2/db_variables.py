# db_variables.py
# python3 db_variables.py --config data_def.json
from sqlalchemy import create_engine, Column, Integer, String, Text, DateTime, Boolean, ForeignKey, CheckConstraint
#from sqlalchemy.ext.declarative import declarative_base
# from sqlalchemy import declarative_base
from sqlalchemy.orm import relationship, sessionmaker, Session,  declarative_base
from datetime import datetime
import json
import argparse
import sys

Base = declarative_base()

# ============================================================================
# Configuration (set via command-line args or defaults)
# ============================================================================

class Config:
    """Global configuration"""
    DATABASE_URL = "sqlite:///./variables.db"
    CONFIG_FILE = None
    DATA_SOURCE_IP = "127.0.0.1"
    DATA_SOURCE_PORT = 8901
    SERVER_PORT = 8902
    
config = Config()

def parse_args():
    """Parse command-line arguments"""
    parser = argparse.ArgumentParser(description='Variable Database Server')
    parser.add_argument('--db', type=str, default='variables.db',
                        help='Database file path (default: variables.db)')
    parser.add_argument('--config', type=str, default=None,
                        help='JSON config file to import on startup')
    parser.add_argument('--data-source-ip', type=str, default='127.0.0.1',
                        help='Data source IP address (default: 127.0.0.1)')
    parser.add_argument('--data-source-port', type=int, default=8901,
                        help='Data source port (default: 8901)')
    parser.add_argument('--port', type=int, default=8902,
                        help='Server port (default: 8902)')
    return parser.parse_args()

def apply_config(args):
    """Apply command-line arguments to global config"""
    config.DATABASE_URL = f"sqlite:///./{args.db}"
    config.CONFIG_FILE = args.config
    config.DATA_SOURCE_IP = args.data_source_ip
    config.DATA_SOURCE_PORT = args.data_source_port
    config.SERVER_PORT = args.port

# ============================================================================
# Models
# ============================================================================

class SystemVersion(Base):
    """Single-row table tracking global version counter"""
    __tablename__ = "system_version"
    
    id = Column(Integer, primary_key=True, default=1)
    system_version = Column(Integer, default=0, nullable=False)
    last_updated = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    __table_args__ = (
        CheckConstraint('id = 1', name='single_row_check'),
    )


class SystemVariable(Base):
    """Core variable registry"""
    __tablename__ = "system_variables"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String, unique=True, nullable=False, index=True)
    display_name = Column(String, nullable=False)
    units = Column(String, default="")
    description = Column(Text, default="")
    
    # Dual versioning
    variable_version = Column(Integer, default=1, nullable=False)
    system_version = Column(Integer, nullable=False, index=True)
    
    last_updated = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    # Sync metadata
    source_system = Column(String, default="local")
    
    # Relationships
    access_paths = relationship("VariableAccessPath", back_populates="variable", cascade="all, delete-orphan")


class VariableAccessPath(Base):
    """Access paths for variables (memory, modbus, can, etc.)"""
    __tablename__ = "variable_access_paths"
    
    id = Column(Integer, primary_key=True, autoincrement=True)
    variable_id = Column(Integer, ForeignKey("system_variables.id"), nullable=False, index=True)
    
    access_type = Column(String, nullable=False)  # "memory_offset", "modbus", "can", "rs485"
    reference = Column(Text, nullable=False)  # JSON blob: {"table": "rack:sm16", "offset": 100, "size": 2, "num": 1}
    
    priority = Column(Integer, default=10)
    read_only = Column(Boolean, default=False)
    
    rack_number = Column(Integer)
    device_id = Column(String)
    
    # Versioning
    path_version = Column(Integer, default=1, nullable=False)
    system_version = Column(Integer, nullable=False, index=True)
    
    last_updated = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    active = Column(Boolean, default=True)
    
    variable = relationship("SystemVariable", back_populates="access_paths")


# ============================================================================
# Database setup
# ============================================================================

engine = None
SessionLocal = None

def init_db():
    """Initialize database tables"""
    global engine, SessionLocal
    
    engine = create_engine(config.DATABASE_URL, connect_args={"check_same_thread": False})
    SessionLocal = sessionmaker(bind=engine)
    
    Base.metadata.create_all(bind=engine)
    
    # Ensure system_version row exists
    with SessionLocal() as db:
        sys_ver = db.query(SystemVersion).filter_by(id=1).first()
        if not sys_ver:
            db.add(SystemVersion(id=1, current_version=0))
            db.commit()
            print(f"✓ Initialized system version to 0")
    
    print(f"✓ Database initialized: {config.DATABASE_URL}")


def get_session():
    """Get the SessionLocal factory (must call init_db first)"""
    if SessionLocal is None:
        raise RuntimeError("Database not initialized. Call init_db() first.")
    return SessionLocal

# ============================================================================
# Version management helpers
# ============================================================================

def set_system_version(db: Session, version: int) -> int:
    """Set system version to a specific value (for sync purposes)"""
    sys_ver = db.query(SystemVersion).filter_by(id=1).with_for_update().first()
    if not sys_ver:
        sys_ver = SystemVersion(id=1, current_version=version)
        db.add(sys_ver)
    else:
        sys_ver.current_version = version
        sys_ver.last_updated = datetime.utcnow()
    db.flush()
    return sys_ver.current_version

def get_system_version():
    version = db.query(SystemVersion).first()
    if version:
        return {
            "system_version": version.system_version,
            "timestamp": version.timestamp
        }
    return {"system_version": 0, "timestamp": None}



def increment_system_version(db: Session) -> int:
    """Increment and return new system version (thread-safe)"""
    sys_ver = db.query(SystemVersion).filter_by(id=1).with_for_update().first()
    if not sys_ver:
        sys_ver = SystemVersion(id=1, current_version=0)
        db.add(sys_ver)
        db.flush()
    
    sys_ver.current_version += 1
    sys_ver.last_updated = datetime.utcnow()
    db.flush()
    
    return sys_ver.current_version


# ============================================================================
# Helper functions
# ============================================================================

def get_variable_memory_size(reference: dict) -> int:
    """
    Calculate total memory size in bytes for a variable.
    
    reference = {"table": "rack:sm16", "offset": 100, "size": 2, "num": 96}
    Returns: 192 (96 elements × 2 bytes each)
    """
    size = reference.get("size", 2)
    num = reference.get("num", 1)
    return size * num


# ============================================================================
# JSON import
# ============================================================================

def import_json_definitions(db: Session, json_data: list, source_system: str = "local"):
    """
    Import variable definitions from JSON format.
    
    Expected format:
    [
      {
        "table": "rack:sm16",
        "items": [
          {"name": "online", "offset": 26626, "size": 2, "num": 1},
          {"name": "cell_voltages", "offset": 200, "size": 2, "num": 96},
          ...
        ]
      },
      ...
    ]
    """
    imported_count = 0
    skipped_count = 0
    updated_count = 0
    
    for table_def in json_data:
        table_name = table_def.get("table")
        items = table_def.get("items", [])
        
        for item in items:
            var_name = item.get("name")
            offset = item.get("offset")
            size = item.get("size", 2)  # Default to 16-bit (sm16)
            num = item.get("num", 1)    # Default to scalar
            
            # Skip gaps or internal placeholders
            if var_name.startswith("_gap"):
                skipped_count += 1
                continue
            
            # Check if variable already exists
            var = db.query(SystemVariable).filter_by(name=var_name).first()
            
            if not var:
                # Get new system version
                new_sys_ver = increment_system_version(db)
                
                # Create new variable
                # If num > 1, indicate it's an array in the display name
                display_suffix = f" [{num}]" if num > 1 else ""
                
                var = SystemVariable(
                    name=var_name,
                    display_name=var_name.replace("_", " ").title() + display_suffix,
                    units="",
                    description=f"Imported from {table_name}, {num} element(s) of size {size} bytes",
                    variable_version=1,
                    system_version=new_sys_ver,
                    source_system=source_system
                )
                db.add(var)
                db.flush()
                imported_count += 1
            
            # Create access path
            reference = {
                "table": table_name,
                "offset": offset,
                "size": size,
                "num": num
            }
            
            # Check if this exact path already exists
            existing_path = db.query(VariableAccessPath).filter_by(
                variable_id=var.id,
                access_type="memory_offset",
                reference=json.dumps(reference)
            ).first()
            
            if not existing_path:
                new_sys_ver = increment_system_version(db)
                
                path = VariableAccessPath(
                    variable_id=var.id,
                    access_type="memory_offset",
                    reference=json.dumps(reference),
                    priority=10,
                    path_version=1,
                    system_version=new_sys_ver,
                    read_only=False
                )
                db.add(path)
                updated_count += 1
    
    db.commit()
    
    return {
        "imported": imported_count,
        "updated": updated_count,
        "skipped": skipped_count,
        "system_version": get_system_version(db)
    }


def load_config_file(filepath: str):
    """Load and import JSON config file"""
    print(f"Loading config file: {filepath}")
    with open(filepath, 'r') as f:
        json_data = json.load(f)
    
    with SessionLocal() as db:
        result = import_json_definitions(db, json_data, source_system="config_file")
        print(f"✓ Imported: {result['imported']} variables")
        print(f"✓ Updated: {result['updated']} access paths")
        print(f"✓ Skipped: {result['skipped']} gaps")
        print(f"✓ System version: {result['system_version']}")


# ============================================================================
# Main entry point
# ============================================================================

if __name__ == "__main__":
    args = parse_args()
    apply_config(args)
    
    print("=" * 60)
    print("Variable Database Server")
    print("=" * 60)
    print(f"Database:        {config.DATABASE_URL}")
    print(f"Config file:     {config.CONFIG_FILE or '(none)'}")
    print(f"Data source:     {config.DATA_SOURCE_IP}:{config.DATA_SOURCE_PORT}")
    print(f"Server port:     {config.SERVER_PORT}")
    print("=" * 60)
    
    # Initialize database
    init_db()
    
    # Load config file if provided
    if config.CONFIG_FILE:
        try:
            load_config_file(config.CONFIG_FILE)
        except Exception as e:
            print(f"✗ Error loading config file: {e}")
            sys.exit(1)
    
    print("\n✓ Ready!")
