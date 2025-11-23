Perfect. Let's build this incrementally, one piece at a time.

---

## Phase 1: Core schema + JSON import

Let's start with:
1. The database tables with dual versioning
2. A script to import your existing JSON definitions
3. Basic API endpoints to view variables

---

### Step 1a: Create `db_variables.py` with the core models

I'll create a new file for the variable registry system:

```python
# db_variables.py
from sqlalchemy import create_engine, Column, Integer, String, Text, DateTime, Boolean, ForeignKey, CheckConstraint
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship, sessionmaker, Session
from datetime import datetime
import json

Base = declarative_base()

# ============================================================================
# Models
# ============================================================================

class SystemVersion(Base):
    """Single-row table tracking global version counter"""
    __tablename__ = "system_version"
    
    id = Column(Integer, primary_key=True, default=1)
    current_version = Column(Integer, default=0, nullable=False)
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
    reference = Column(Text, nullable=False)  # JSON blob
    
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

DATABASE_URL = "sqlite:///./variables.db"
engine = create_engine(DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(bind=engine)

def init_db():
    """Initialize database tables"""
    Base.metadata.create_all(bind=engine)
    
    # Ensure system_version row exists
    with SessionLocal() as db:
        sys_ver = db.query(SystemVersion).filter_by(id=1).first()
        if not sys_ver:
            db.add(SystemVersion(id=1, current_version=0))
            db.commit()
            print("Initialized system version to 0")


# ============================================================================
# Version management helpers
# ============================================================================

def get_system_version(db: Session) -> int:
    """Get current system version"""
    sys_ver = db.query(SystemVersion).filter_by(id=1).first()
    return sys_ver.current_version if sys_ver else 0


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
          {"name": "online", "offset": 26626, "size": 1},
          ...
        ]
      },
      ...
    ]
    """
    imported_count = 0
    skipped_count = 0
    
    for table_def in json_data:
        table_name = table_def.get("table")
        items = table_def.get("items", [])
        
        for item in items:
            var_name = item.get("name")
            offset = item.get("offset")
            size = item.get("size")
            
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
                var = SystemVariable(
                    name=var_name,
                    display_name=var_name.replace("_", " ").title(),
                    units="",
                    description=f"Imported from {table_name}",
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
                "size": size
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
    
    db.commit()
    
    return {
        "imported": imported_count,
        "skipped": skipped_count,
        "system_version": get_system_version(db)
    }


if __name__ == "__main__":
    print("Initializing variable database...")
    init_db()
    print("Database initialized successfully!")
```

---

### Step 1b: Test the database creation

Let's create a simple test script:

```python
# test_variables_db.py
from db_variables import init_db, SessionLocal, get_system_version, SystemVariable
import json

def test_init():
    """Test database initialization"""
    print("=== Testing Database Initialization ===")
    init_db()
    
    with SessionLocal() as db:
        sys_ver = get_system_version(db)
        print(f"✓ System version: {sys_ver}")
        
        var_count = db.query(SystemVariable).count()
        print(f"✓ Variables in DB: {var_count}")
    
    print()

if __name__ == "__main__":
    test_init()
```

---

### Step 1c: Create a sample JSON file for testing

```json
# sample_variables.json
[
  {
    "table": "rack:sm16",
    "items": [
      {"name": "online", "offset": 26626, "size": 1},
      {"name": "rs_485_1", "offset": 7182, "size": 16},
      {"name": "rs_485_2", "offset": 7198, "size": 16},
      {"name": "config_cfg", "offset": 7218, "size": 20}
    ]
  },
  {
    "table": "rtos:bits",
    "items": [
      {"name": "_gap1", "offset": 0, "size": 1},
      {"name": "terminal_overvoltage", "offset": 1, "size": 3},
      {"name": "terminal_undervoltage", "offset": 4, "size": 3}
    ]
  },
  {
    "table": "sbmu:sm16",
    "items": [
      {"name": "soc", "offset": 100, "size": 1},
      {"name": "cell_voltage", "offset": 200, "size": 1}
    ]
  }
]
```

---

### Step 1d: Test JSON import

```python
# test_import.py
from db_variables import init_db, SessionLocal, import_json_definitions, get_system_version, SystemVariable
import json

def test_import():
    """Test importing JSON definitions"""
    print("=== Testing JSON Import ===")
    
    # Initialize DB
    init_db()
    
    # Load sample JSON
    with open("sample_variables.json", "r") as f:
        json_data = json.load(f)
    
    # Import
    with SessionLocal() as db:
        result = import_json_definitions(db, json_data, source_system="test_import")
        print(f"✓ Imported: {result['imported']} variables")
        print(f"✓ Skipped: {result['skipped']} gaps")
        print(f"✓ System version: {result['system_version']}")
        print()
        
        # Show what was imported
        print("=== Variables in Database ===")
        vars = db.query(SystemVariable).all()
        for var in vars:
            print(f"  {var.name:30s} v{var.variable_version} (sys v{var.system_version})")
            for path in var.access_paths:
                if path.active:
                    ref = json.loads(path.reference)
                    print(f"    → {path.access_type:15s} {ref}")

if __name__ == "__main__":
    test_import()
```

---

## Let's run Phase 1

Before we move to the web interface, let's get this foundation working:

1. **Create these files:**
   - `db_variables.py` (the core module)
   - `sample_variables.json` (test data)
   - `test_variables_db.py` (basic test)
   - `test_import.py` (import test)

2. **Run the tests:**
   ```bash
   python test_variables_db.py
   python test_import.py
   ```

This will:
- ✅ Create the `variables.db` SQLite database
- ✅ Initialize system version to 0
- ✅ Import your sample variables
- ✅ Show the imported variables with their versions

Once this is working, we'll move to **Phase 2: Basic REST API** to view/list variables.

Does this look good to start with? Should I proceed with creating these files, or would you like to adjust anything first?

Perfect addition! So we need to handle:

- **`size`**: Width of each element (1 = sm8, 2 = sm16, 4 = sm32)
- **`num`**: Number of elements in the array (default 1 for scalar)

So a variable can be:
- Scalar: `{"name": "soc", "offset": 100, "size": 2, "num": 1}` → single 16-bit value
- Array: `{"name": "cell_voltages", "offset": 200, "size": 2, "num": 96}` → array of 96 16-bit values

---

## Updated schema and import logic

### 1. Update `VariableAccessPath` to store `num`

```python
class VariableAccessPath(Base):
    """Access paths for variables (memory, modbus, can, etc.)"""
    __tablename__ = "variable_access_paths"
    
    id = Column(Integer, primary_key=True, autoincrement=True)
    variable_id = Column(Integer, ForeignKey("system_variables.id"), nullable=False, index=True)
    
    access_type = Column(String, nullable=False)  # "memory_offset", "modbus", "can", "rs485"
    reference = Column(Text, nullable=False)  # JSON blob
    
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
```

The `reference` JSON will now include `num`:

```json
{
  "table": "rack:sm16",
  "offset": 200,
  "size": 2,
  "num": 96
}
```

---

### 2. Update JSON import to handle `num`

```python
def import_json_definitions(db: Session, json_data: list, source_system: str = "local"):
    """
    Import variable definitions from JSON format.
    
    Expected format:
    [
      {
        "table": "rack:sm16",
        "items": [
          {"name": "online", "offset": 26626, "size": 1, "num": 1},
          {"name": "cell_voltages", "offset": 200, "size": 2, "num": 96},
          ...
        ]
      },
      ...
    ]
    """
    imported_count = 0
    skipped_count = 0
    
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
                    description=f"Imported from {table_name}, {num} element(s) of size {size}",
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
    
    db.commit()
    
    return {
        "imported": imported_count,
        "skipped": skipped_count,
        "system_version": get_system_version(db)
    }
```

---

### 3. Update sample JSON to include `num`

```json
[
  {
    "table": "rack:sm16",
    "items": [
      {"name": "online", "offset": 26626, "size": 2, "num": 1},
      {"name": "rs_485_1", "offset": 7182, "size": 2, "num": 16},
      {"name": "rs_485_2", "offset": 7198, "size": 2, "num": 16},
      {"name": "config_cfg", "offset": 7218, "size": 2, "num": 20},
      {"name": "cell_voltages", "offset": 8000, "size": 2, "num": 96}
    ]
  },
  {
    "table": "rtos:bits",
    "items": [
      {"name": "_gap1", "offset": 0, "size": 1, "num": 1},
      {"name": "terminal_overvoltage", "offset": 1, "size": 1, "num": 3},
      {"name": "terminal_undervoltage", "offset": 4, "size": 1, "num": 3}
    ]
  },
  {
    "table": "sbmu:sm16",
    "items": [
      {"name": "soc", "offset": 100, "size": 2, "num": 1},
      {"name": "temperatures", "offset": 300, "size": 2, "num": 32}
    ]
  },
  {
    "table": "sbmu:sm32",
    "items": [
      {"name": "total_energy", "offset": 500, "size": 4, "num": 1}
    ]
  }
]
```

---

### 4. Updated test output

The test will now show:

```
=== Variables in Database ===
  online                         v1 (sys v1)
    → memory_offset   {'table': 'rack:sm16', 'offset': 26626, 'size': 2, 'num': 1}
  rs_485_1                       v1 (sys v2)
    → memory_offset   {'table': 'rack:sm16', 'offset': 7182, 'size': 2, 'num': 16}
  cell_voltages                  v1 (sys v5)
    → memory_offset   {'table': 'rack:sm16', 'offset': 8000, 'size': 2, 'num': 96}
  terminal_overvoltage           v1 (sys v6)
    → memory_offset   {'table': 'rtos:bits', 'offset': 1, 'size': 1, 'num': 3}
  soc                            v1 (sys v8)
    → memory_offset   {'table': 'sbmu:sm16', 'offset': 100, 'size': 2, 'num': 1}
  temperatures                   v1 (sys v9)
    → memory_offset   {'table': 'sbmu:sm16', 'offset': 300, 'size': 2, 'num': 32}
  total_energy                   v1 (sys v10)
    → memory_offset   {'table': 'sbmu:sm32', 'offset': 500, 'size': 4, 'num': 1}
```

---

### 5. Helper to calculate total memory footprint

```python
def get_variable_memory_size(reference: dict) -> int:
    """
    Calculate total memory size in bytes for a variable.
    
    reference = {"table": "rack:sm16", "offset": 100, "size": 2, "num": 96}
    Returns: 192 (96 elements × 2 bytes each)
    """
    size = reference.get("size", 2)
    num = reference.get("num", 1)
    return size * num
```

---

## Complete updated `db_variables.py`

Here's the full file with `num` support:# db_variables.py
from sqlalchemy import create_engine, Column, Integer, String, Text, DateTime, Boolean, ForeignKey, CheckConstraint
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship, sessionmaker, Session
from datetime import datetime
import json

Base = declarative_base()

# ============================================================================
# Models
# ============================================================================

class SystemVersion(Base):
    """Single-row table tracking global version counter"""
    __tablename__ = "system_version"
    
    id = Column(Integer, primary_key=True, default=1)
    current_version = Column(Integer, default=0, nullable=False)
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

DATABASE_URL = "sqlite:///./variables.db"
engine = create_engine(DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(bind=engine)

def init_db():
    """Initialize database tables"""
    Base.metadata.create_all(bind=engine)
    
    # Ensure system_version row exists
    with SessionLocal() as db:
        sys_ver = db.query(SystemVersion).filter_by(id=1).first()
        if not sys_ver:
            db.add(SystemVersion(id=1, current_version=0))
            db.commit()
            print("Initialized system version to 0")


# ============================================================================
# Version management helpers
# ============================================================================

def get_system_version(db: Session) -> int:
    """Get current system version"""
    sys_ver = db.query(SystemVersion).filter_by(id=1).first()
    return sys_ver.current_version if sys_ver else 0


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


if __name__ == "__main__":
    print("Initializing variable database...")
    init_db()
    print("Database initialized successfully!")
I've created the complete `db_variables.py` with **`num` support** added.

### Key changes:

✅ **`reference` JSON now includes `num`**:
```json
{"table": "rack:sm16", "offset": 100, "size": 2, "num": 96}
```

✅ **Import function handles `num`**:
- Defaults to `num=1` for scalars
- Adds `[num]` suffix to display name for arrays (e.g., "Cell Voltages [96]")
- Stores full memory layout info

✅ **Helper function** `get_variable_memory_size()`:
- Calculates total bytes: `size × num`

✅ **Updated description** shows element count and size

---

### Next steps:

1. **Create `sample_variables.json`** with `num` field
2. **Create test scripts** to verify import
3. **Then move to Phase 2**: Basic REST API to view/list variables

Should I create the sample JSON and test scripts next?

You're absolutely right! Let me update the `VariableRegistryWidget` to use `getJSON` instead of `fetch`, which will route through your `main.py` proxy.

---

### Updated `VariableRegistryWidget.js`:

```javascript
// VariableRegistryWidget.js

class VariableRegistryWidget extends BaseWidget {
    constructor(dashboard, uid) {
        super(dashboard, uid);
        this.title = "Variable Registry";
        this.variables = [];
        this.loading = true;
        this.error = null;
    }

    async init() {
        await super.init();
        this.render();
        this.fetchVariables();
    }

    async fetchVariables() {
        this.loading = true;
        this.error = null;
        this.render(); // Show loading state

        try {
            const data = await getJSON('/vars_api/api/vars');
            this.variables = data.variables;
            this.loading = false;
        } catch (error) {
            console.error("Error fetching variables:", error);
            this.error = "Failed to load variables.";
            this.loading = false;
        }
        this.render(); // Update with data or error
    }

    render() {
        const container = this.getContainer(); // Adjust based on your BaseWidget method
        if (!container) return;

        let content = `
            <div class="variable-registry-widget">
                <h3>${this.title}</h3>
        `;

        if (this.loading) {
            content += `<p>Loading variables...</p>`;
        } else if (this.error) {
            content += `<p class="error">${this.error}</p>`;
        } else if (this.variables.length === 0) {
            content += `<p>No variables found.</p>`;
        } else {
            content += `
                <div class="variable-list">
                    ${this.variables.map(v => `
                        <div class="variable-item" data-var-name="${v.name}">
                            <strong>${v.display_name}</strong> <span class="var-name">(${v.name})</span>
                            <div class="var-meta">
                                <span class="badge">v${v.variable_version}</span>
                                <span class="badge">sys v${v.system_version}</span>
                                ${v.units ? `<span class="units">${v.units}</span>` : ''}
                            </div>
                            <p class="var-description">${v.description || 'No description'}</p>
                        </div>
                    `).join('')}
                </div>
            `;
        }

        content += `</div>`;
        container.innerHTML = content;

        // Add click handlers for variable items (for future detail view)
        if (!this.loading && !this.error) {
            container.querySelectorAll('.variable-item').forEach(item => {
                item.addEventListener('click', (e) => {
                    const varName = e.currentTarget.dataset.varName;
                    this.showVariableDetails(varName);
                });
            });
        }
    }

    async showVariableDetails(varName) {
        console.log(`Showing details for: ${varName}`);
        // TODO: Implement detail view in next step
        try {
            const data = await getJSON(`/vars_api/api/vars/${varName}`);
            console.log('Variable details:', data);
            // We'll add a modal or detail panel here later
        } catch (error) {
            console.error("Error fetching variable details:", error);
        }
    }

    // Optional: implement tick() if you want periodic updates
    // async tick(seriesCache) {
    //     // Could check system version and re-fetch if changed
    // }
}
```

---

### Updated CSS (add to your stylesheet):

```css
.variable-registry-widget {
    padding: 15px;
    border: 1px solid #ccc;
    border-radius: 5px;
    background-color: #f9f9f9;
    height: 100%;
    overflow-y: auto;
    font-family: Arial, sans-serif;
}

.variable-registry-widget h3 {
    margin-top: 0;
    color: #333;
    border-bottom: 2px solid #007bff;
    padding-bottom: 5px;
}

.variable-list {
    margin-top: 10px;
}

.variable-item {
    padding: 10px;
    margin-bottom: 8px;
    border: 1px solid #ddd;
    border-radius: 4px;
    background-color: #fff;
    cursor: pointer;
    transition: background-color 0.2s, box-shadow 0.2s;
}

.variable-item:hover {
    background-color: #f0f8ff;
    box-shadow: 0 2px 5px rgba(0,0,0,0.1);
}

.variable-item strong {
    color: #333;
    font-size: 1.1em;
}

.variable-item .var-name {
    color: #666;
    font-size: 0.9em;
    font-family: monospace;
}

.var-meta {
    margin: 5px 0;
}

.var-meta .badge {
    display: inline-block;
    padding: 2px 6px;
    margin-right: 5px;
    background-color: #007bff;
    color: white;
    border-radius: 3px;
    font-size: 0.8em;
}

.var-meta .units {
    color: #28a745;
    font-weight: bold;
    margin-left: 5px;
}

.var-description {
    font-size: 0.9em;
    color: #666;
    margin: 5px 0 0 0;
}

.error {
    color: red;
    font-weight: bold;
}
```

---

### Update `main.py` proxy route:

Make sure your `main.py` has the proxy route for `/vars_api/`:

```python
# In main.py, add this proxy route

@app.api_route("/vars_api/{path:path}", methods=["GET", "POST", "PUT", "DELETE"])
async def proxy_vars_api(path: str, request: Request):
    """Proxy requests to the Variable Registry API server"""
    vars_api_url = f"http://127.0.0.1:8902/{path}"
    
    headers = dict(request.headers)
    headers.pop('host', None)
    
    async with httpx.AsyncClient() as client:
        if request.method == "GET":
            response = await client.get(vars_api_url, params=request.query_params)
        elif request.method == "POST":
            body = await request.body()
            response = await client.post(vars_api_url, content=body, headers=headers)
        elif request.method == "PUT":
            body = await request.body()
            response = await client.put(vars_api_url, content=body, headers=headers)
        elif request.method == "DELETE":
            response = await client.delete(vars_api_url)
        
        return Response(
            content=response.content,
            status_code=response.status_code,
            headers=dict(response.headers)
        )
```

---

### Key changes:

1. **Uses `getJSON('/vars_api/api/vars')`** instead of `fetch()` - routes through your `main.py` proxy
2. **Added click handlers** on variable items to show details (placeholder for now)
3. **Better styling** with badges for versions and hover effects
4. **Uses `this.getContainer()`** - adjust this to match your `BaseWidget` method (might be `this.element`, `this.container`, etc.)

---

### To test:

1. Make sure `vars_server.py` is running on port 8902
2. Make sure your `main.py` has the `/vars_api/` proxy route
3. Add `VariableRegistryWidget` to your `dash.json`
4. Load your dashboard

You should see a list of variables that you can click on (details view coming next).

What does your `BaseWidget` use for getting the container element? Is it `this.element`, `this.container`, or something else?


=======================context==============================
Perfect! I'll create a summary of where we are with the variable registry system.

## Variable Registry System - Current State

### ✅ Completed Components

#### 1. **Database Layer (`db_variables.py`)**
- **Dual versioning system**: 
  - `variable_version`: Increments when a specific variable changes
  - `system_version`: Global counter that increments on ANY change
- **Tables**:
  - `SystemVersion`: Single-row table tracking global version counter
  - `SystemVariable`: Core variable registry with metadata (name, display_name, units, description, versioning)
  - `VariableAccessPath`: Multiple access paths per variable (memory_offset, modbus, can, rs485, etc.)
- **JSON Import**: Can import variable definitions from `data_def.json` format:
  ```json
  [
    {
      "table": "rack:sm16",
      "items": [
        {"name": "online", "offset": 26626, "size": 2, "num": 1},
        {"name": "cell_voltages", "offset": 200, "size": 2, "num": 96}
      ]
    }
  ]
  ```
- **Configuration**: Command-line args for database path, config file, data source IP/port, server port
- **Helper functions**: 
  - `get_system_version()`: Get current global version
  - `increment_system_version()`: Thread-safe increment
  - `get_variable_memory_size()`: Calculate total memory size for a variable
  - `import_json_definitions()`: Import from JSON with version tracking

#### 2. **Inspection Tool (`inspect_vars.py`)**
- Simple CLI tool to view database contents
- Shows all variables with their versions and active access paths
- Usage: `python3 inspect_vars.py`
- Uses `get_session()` pattern to avoid global variable issues

#### 3. **REST API Server (`vars_server.py`)**
- **FastAPI application** running on port 8902 (configurable)
- **Endpoints**:
  - `GET /`: Service info (variable count, system version, database URL)
  - `GET /api/vars`: List all variables with pagination (limit, offset)
  - `GET /api/vars/{var_name}`: Get single variable with all access paths
  - `GET /api/system/version`: Get current system version
- **CORS enabled** for dashboard access
- **Lifespan events** for proper startup/shutdown
- Command-line args: `--db`, `--port`, `--data-source-ip`, `--data-source-port`

#### 4. **Frontend Widget (`VariableRegistryWidget.js`)**
- Extends `BaseWidget` from your dashboard framework
- **Features**:
  - Fetches variables via `getJSON('/vars_api/api/vars')` (routes through `main.py` proxy)
  - Displays list with display name, variable name, versions, units, description
  - Loading and error states
  - Click handlers on each variable item (prepared for detail view)
  - Styled with badges for versions, hover effects
- **CSS**: Complete styling for list view, items, badges, hover states

#### 5. **Proxy Route in `main.py`**
- `/vars_api/{path:path}` proxy route forwards requests to `vars_server.py` (port 8902)
- Supports GET, POST, PUT, DELETE methods
- Strips host headers and forwards query params/body correctly

### 🎯 Current Workflow

1. **Import data**: `python3 db_variables.py --db variables.db --config data_def.json`
2. **Inspect data**: `python3 inspect_vars.py`
3. **Run API server**: `python3 vars_server.py --db variables.db --port 8902`
4. **Run dashboard**: `python3 main.py --port 8000 --vars-api-url http://localhost:8902`
5. **View in browser**: Dashboard shows `VariableRegistryWidget` with clickable variable list

### 📋 Next Steps (Not Yet Implemented)

1. **Variable Detail View**:
   - Modal or expandable panel showing full variable info
   - Display all access paths with their references
   - Show version history

2. **Edit Functionality**:
   - Edit variable metadata (display_name, units, description)
   - Add/remove/edit access paths
   - Save changes with version increment

3. **Create New Variables**:
   - Form to add new variables
   - Add initial access paths
   - Proper version tracking

4. **Search/Filter**:
   - Search by name, display name, or description
   - Filter by access type, table, or version

5. **Sync Endpoints** (for distributed systems):
   - `GET /api/vars/since/{system_version}`: Get all changes since a version
   - `POST /api/vars/sync`: Accept updates from remote systems
   - Conflict resolution for distributed updates

6. **Delete Variables**:
   - Soft delete (mark as inactive) vs hard delete
   - Cascade to access paths

### 🔑 Key Design Decisions

- **Dual versioning** allows tracking both individual variable changes and system-wide synchronization
- **Multiple access paths per variable** supports different protocols (memory, modbus, CAN, RS485)
- **JSON reference field** in access paths allows flexible metadata storage
- **`get_session()` pattern** avoids Python global variable mutation issues
- **Proxy through `main.py`** keeps frontend simple and allows centralized request handling
- **`getJSON`/`postJSON`** pattern matches your existing dashboard architecture

### 📁 File Structure

```
dash3/
├── db_variables.py          # Database models, versioning, JSON import
├── inspect_vars.py          # CLI inspection tool
├── vars_server.py           # FastAPI REST API
├── main.py                  # Dashboard server with /vars_api/ proxy
├── VariableRegistryWidget.js # Frontend widget
├── variables.db             # SQLite database (generated)
├── data_def.json            # Variable definitions (user-provided)
└── dash.json                # Dashboard layout config
```

---

Ready to continue whenever you are! The foundation is solid and ready for the next features. 🚀


#==============================================

Excellent point! You need a way to categorize and locate variables within your system hierarchy. Let's extend the schema to support this.

## Proposed Schema Extensions

### 1. Add `category` and `locator` fields to `SystemVariable`

```python
class SystemVariable(Base):
    """Core variable registry"""
    __tablename__ = "system_variables"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String, unique=True, nullable=False, index=True)
    display_name = Column(String, nullable=False)
    units = Column(String, default="")
    description = Column(Text, default="")
    
    # NEW: Category and locator
    category = Column(String, default="oper", nullable=False, index=True)  # "config", "prot", "oper"
    locator = Column(String, nullable=True, index=True)  # "sbmu.chiller[n]", "rbms.module[n]", "rbms.cell[n]"
    
    # Dual versioning
    variable_version = Column(Integer, default=1, nullable=False)
    system_version = Column(Integer, nullable=False, index=True)
    
    last_updated = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    # Sync metadata
    source_system = Column(String, default="local")
    
    # Relationships
    access_paths = relationship("VariableAccessPath", back_populates="variable", cascade="all, delete-orphan")
```

### 2. Locator Format Examples

```
# Chillers
sbmu.chiller[n]              # Any chiller (n = instance number)
sbmu.chiller[0]              # Specific chiller 0
sbmu.chiller[*]              # All chillers

# Modules
rbms.module[n]               # Any module
rbms.rack[n].module[m]       # Module m in rack n

# Cells
rbms.cell[n]                 # Any cell
rbms.rack[n].module[m].cell[c]  # Specific cell

# Contactors/Relays
rbms.contactor[n]
rbms.relay[n]

# System-level (no instance)
system.global                # System-wide config
rack.global                  # Rack-level config
```

### 3. Category Values

- **`config`**: Configuration parameters (setpoints, limits, enable/disable flags)
- **`prot`**: Protection thresholds (alarms, faults, warnings)
- **`oper`**: Operational data (telemetry, status, measurements)

---

## Updated `db_variables.py`

Here's the updated model section:

```python
class SystemVariable(Base):
    """Core variable registry"""
    __tablename__ = "system_variables"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String, unique=True, nullable=False, index=True)
    display_name = Column(String, nullable=False)
    units = Column(String, default="")
    description = Column(Text, default="")
    
    # Category and locator for filtering/grouping
    category = Column(String, default="oper", nullable=False, index=True)  # "config", "prot", "oper"
    locator = Column(String, nullable=True, index=True)  # "sbmu.chiller[n]", "rbms.module[n]", etc.
    
    # Dual versioning
    variable_version = Column(Integer, default=1, nullable=False)
    system_version = Column(Integer, nullable=False, index=True)
    
    last_updated = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    # Sync metadata
    source_system = Column(String, default="local")
    
    # Relationships
    access_paths = relationship("VariableAccessPath", back_populates="variable", cascade="all, delete-orphan")
```

---

## Updated JSON Import Format

Extend your `data_def.json` to include category and locator:

```json
[
  {
    "table": "sbmu:sm16",
    "category": "config",
    "locator": "sbmu.chiller[n]",
    "items": [
      {"name": "chiller_enable", "offset": 100, "size": 2, "num": 1},
      {"name": "chiller_setpoint", "offset": 102, "size": 2, "num": 1}
    ]
  },
  {
    "table": "rbms:sm16",
    "category": "oper",
    "locator": "rbms.module[n]",
    "items": [
      {"name": "module_voltage", "offset": 200, "size": 2, "num": 16},
      {"name": "module_temperature", "offset": 232, "size": 2, "num": 16}
    ]
  },
  {
    "table": "rbms:sm16",
    "category": "oper",
    "locator": "rbms.cell[n]",
    "items": [
      {"name": "cell_voltage", "offset": 300, "size": 2, "num": 96}
    ]
  },
  {
    "table": "rack:sm16",
    "category": "prot",
    "locator": "system.global",
    "items": [
      {"name": "overvoltage_limit", "offset": 400, "size": 2, "num": 1},
      {"name": "undervoltage_limit", "offset": 402, "size": 2, "num": 1}
    ]
  }
]
```

---

## Updated `import_json_definitions()` Function

```python
def import_json_definitions(db: Session, json_data: list, source_system: str = "local"):
    """
    Import variable definitions from JSON format.
    
    Expected format:
    [
      {
        "table": "rack:sm16",
        "category": "config",  # NEW: optional, defaults to "oper"
        "locator": "sbmu.chiller[n]",  # NEW: optional
        "items": [
          {"name": "online", "offset": 26626, "size": 2, "num": 1},
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
        category = table_def.get("category", "oper")  # Default to "oper"
        locator = table_def.get("locator", None)  # Optional
        items = table_def.get("items", [])
        
        for item in items:
            var_name = item.get("name")
            offset = item.get("offset")
            size = item.get("size", 2)
            num = item.get("num", 1)
            
            # Skip gaps
            if var_name.startswith("_gap"):
                skipped_count += 1
                continue
            
            # Check if variable already exists
            var = db.query(SystemVariable).filter_by(name=var_name).first()
            
            if not var:
                # Get new system version
                new_sys_ver = increment_system_version(db)
                
                # Create new variable
                display_suffix = f" [{num}]" if num > 1 else ""
                
                var = SystemVariable(
                    name=var_name,
                    display_name=var_name.replace("_", " ").title() + display_suffix,
                    units="",
                    description=f"Imported from {table_name}, {num} element(s) of size {size} bytes",
                    category=category,  # NEW
                    locator=locator,    # NEW
                    variable_version=1,
                    system_version=new_sys_ver,
                    source_system=source_system
                )
                db.add(var)
                db.flush()
                imported_count += 1
            else:
                # Update category/locator if they've changed
                changed = False
                if var.category != category:
                    var.category = category
                    changed = True
                if var.locator != locator:
                    var.locator = locator
                    changed = True
                
                if changed:
                    new_sys_ver = increment_system_version(db)
                    var.variable_version += 1
                    var.system_version = new_sys_ver
                    var.last_updated = datetime.utcnow()
                    updated_count += 1
            
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
```

---

## New API Endpoints for `vars_server.py`

Add filtering by category and locator:

```python
@app.get("/api/vars")
def list_variables(
    limit: int = 100, 
    offset: int = 0,
    category: str = None,  # NEW: filter by category
    locator: str = None    # NEW: filter by locator pattern
):
    """List all variables with pagination and filtering"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        query = db.query(SystemVariable)
        
        # Apply filters
        if category:
            query = query.filter(SystemVariable.category == category)
        
        if locator:
            # Support wildcards: "sbmu.chiller[*]" matches "sbmu.chiller[n]"
            if "[*]" in locator:
                pattern = locator.replace("[*]", "[%]")
                query = query.filter(SystemVariable.locator.like(pattern))
            else:
                query = query.filter(SystemVariable.locator == locator)
        
        total = query.count()
        vars_ = query.order_by(SystemVariable.name).limit(limit).offset(offset).all()
        
        result = []
        for v in vars_:
            result.append({
                "id": v.id,
                "name": v.name,
                "display_name": v.display_name,
                "units": v.units,
                "description": v.description,
                "category": v.category,  # NEW
                "locator": v.locator,    # NEW
                "variable_version": v.variable_version,
                "system_version": v.system_version,
                "source_system": v.source_system,
                "last_updated": v.last_updated.isoformat() if v.last_updated else None
            })
        
        return {
            "total": total,
            "limit": limit,
            "offset": offset,
            "variables": result
        }


@app.get("/api/categories")
def list_categories():
    """Get all unique categories"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        categories = db.query(SystemVariable.category).distinct().all()
        return {
            "categories": [c[0] for c in categories if c[0]]
        }


@app.get("/api/locators")
def list_locators(category: str = None):
    """Get all unique locators, optionally filtered by category"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        query = db.query(SystemVariable.locator).distinct()
        
        if category:
            query = query.filter(SystemVariable.category == category)
        
        locators = query.all()
        return {
            "locators": [loc[0] for loc in locators if loc[0]]
        }
```

---

## Migration Strategy

Since you already have a database with data, you'll need to add the new columns:

### Option 1: Drop and recreate (if you don't mind losing data)

```bash
rm variables.db
python3 db_variables.py --db variables.db --config data_def.json
```

### Option 2: Add columns manually (preserve existing data)

```python
# migrate_add_category_locator.py
from sqlalchemy import create_engine, text

engine = create_engine("sqlite:///./variables.db")

with engine.connect() as conn:
    # Add category column (default to "oper")
    conn.execute(text("ALTER TABLE system_variables ADD COLUMN category VARCHAR DEFAULT 'oper' NOT NULL"))
    
    # Add locator column (nullable)
    conn.execute(text("ALTER TABLE system_variables ADD COLUMN locator VARCHAR"))
    
    # Create indexes
    conn.execute(text("CREATE INDEX ix_system_variables_category ON system_variables (category)"))
    conn.execute(text("CREATE INDEX ix_system_variables_locator ON system_variables (locator)"))
    
    conn.commit()

print("✓ Migration complete")
```

Run: `python3 migrate_add_category_locator.py`

---

## Usage Examples

### Query config variables for chillers:
```bash
curl "http://localhost:8902/api/vars?category=config&locator=sbmu.chiller[n]"
```

### Query all protection variables:
```bash
curl "http://localhost:8902/api/vars?category=prot"
```

### Get all categories:
```bash
curl "http://localhost:8902/api/categories"
```

### Get all locators for config variables:
```bash
curl "http://localhost:8902/api/locators?category=config"
```

---

Does this approach work for your needs? Should we adjust the locator format or add any other fields?

Excellent point! You need a way to categorize and locate variables within your system hierarchy. Let's extend the schema to support this.

## Proposed Schema Extensions

### 1. Add `category` and `locator` fields to `SystemVariable`

```python
class SystemVariable(Base):
    """Core variable registry"""
    __tablename__ = "system_variables"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String, unique=True, nullable=False, index=True)
    display_name = Column(String, nullable=False)
    units = Column(String, default="")
    description = Column(Text, default="")
    
    # NEW: Category and locator
    category = Column(String, default="oper", nullable=False, index=True)  # "config", "prot", "oper"
    locator = Column(String, nullable=True, index=True)  # "sbmu.chiller[n]", "rbms.module[n]", "rbms.cell[n]"
    
    # Dual versioning
    variable_version = Column(Integer, default=1, nullable=False)
    system_version = Column(Integer, nullable=False, index=True)
    
    last_updated = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    # Sync metadata
    source_system = Column(String, default="local")
    
    # Relationships
    access_paths = relationship("VariableAccessPath", back_populates="variable", cascade="all, delete-orphan")
```

### 2. Locator Format Examples

```
# Chillers
sbmu.chiller[n]              # Any chiller (n = instance number)
sbmu.chiller[0]              # Specific chiller 0
sbmu.chiller[*]              # All chillers

# Modules
rbms.module[n]               # Any module
rbms.rack[n].module[m]       # Module m in rack n

# Cells
rbms.cell[n]                 # Any cell
rbms.rack[n].module[m].cell[c]  # Specific cell

# Contactors/Relays
rbms.contactor[n]
rbms.relay[n]

# System-level (no instance)
system.global                # System-wide config
rack.global                  # Rack-level config
```

### 3. Category Values

- **`config`**: Configuration parameters (setpoints, limits, enable/disable flags)
- **`prot`**: Protection thresholds (alarms, faults, warnings)
- **`oper`**: Operational data (telemetry, status, measurements)

---

## Updated `db_variables.py`

Here's the updated model section:

```python
class SystemVariable(Base):
    """Core variable registry"""
    __tablename__ = "system_variables"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String, unique=True, nullable=False, index=True)
    display_name = Column(String, nullable=False)
    units = Column(String, default="")
    description = Column(Text, default="")
    
    # Category and locator for filtering/grouping
    category = Column(String, default="oper", nullable=False, index=True)  # "config", "prot", "oper"
    locator = Column(String, nullable=True, index=True)  # "sbmu.chiller[n]", "rbms.module[n]", etc.
    
    # Dual versioning
    variable_version = Column(Integer, default=1, nullable=False)
    system_version = Column(Integer, nullable=False, index=True)
    
    last_updated = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    
    # Sync metadata
    source_system = Column(String, default="local")
    
    # Relationships
    access_paths = relationship("VariableAccessPath", back_populates="variable", cascade="all, delete-orphan")
```

---

## Updated JSON Import Format

Extend your `data_def.json` to include category and locator:

```json
[
  {
    "table": "sbmu:sm16",
    "category": "config",
    "locator": "sbmu.chiller[n]",
    "items": [
      {"name": "chiller_enable", "offset": 100, "size": 2, "num": 1},
      {"name": "chiller_setpoint", "offset": 102, "size": 2, "num": 1}
    ]
  },
  {
    "table": "rbms:sm16",
    "category": "oper",
    "locator": "rbms.module[n]",
    "items": [
      {"name": "module_voltage", "offset": 200, "size": 2, "num": 16},
      {"name": "module_temperature", "offset": 232, "size": 2, "num": 16}
    ]
  },
  {
    "table": "rbms:sm16",
    "category": "oper",
    "locator": "rbms.cell[n]",
    "items": [
      {"name": "cell_voltage", "offset": 300, "size": 2, "num": 96}
    ]
  },
  {
    "table": "rack:sm16",
    "category": "prot",
    "locator": "system.global",
    "items": [
      {"name": "overvoltage_limit", "offset": 400, "size": 2, "num": 1},
      {"name": "undervoltage_limit", "offset": 402, "size": 2, "num": 1}
    ]
  }
]
```

---

## Updated `import_json_definitions()` Function

```python
def import_json_definitions(db: Session, json_data: list, source_system: str = "local"):
    """
    Import variable definitions from JSON format.
    
    Expected format:
    [
      {
        "table": "rack:sm16",
        "category": "config",  # NEW: optional, defaults to "oper"
        "locator": "sbmu.chiller[n]",  # NEW: optional
        "items": [
          {"name": "online", "offset": 26626, "size": 2, "num": 1},
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
        category = table_def.get("category", "oper")  # Default to "oper"
        locator = table_def.get("locator", None)  # Optional
        items = table_def.get("items", [])
        
        for item in items:
            var_name = item.get("name")
            offset = item.get("offset")
            size = item.get("size", 2)
            num = item.get("num", 1)
            
            # Skip gaps
            if var_name.startswith("_gap"):
                skipped_count += 1
                continue
            
            # Check if variable already exists
            var = db.query(SystemVariable).filter_by(name=var_name).first()
            
            if not var:
                # Get new system version
                new_sys_ver = increment_system_version(db)
                
                # Create new variable
                display_suffix = f" [{num}]" if num > 1 else ""
                
                var = SystemVariable(
                    name=var_name,
                    display_name=var_name.replace("_", " ").title() + display_suffix,
                    units="",
                    description=f"Imported from {table_name}, {num} element(s) of size {size} bytes",
                    category=category,  # NEW
                    locator=locator,    # NEW
                    variable_version=1,
                    system_version=new_sys_ver,
                    source_system=source_system
                )
                db.add(var)
                db.flush()
                imported_count += 1
            else:
                # Update category/locator if they've changed
                changed = False
                if var.category != category:
                    var.category = category
                    changed = True
                if var.locator != locator:
                    var.locator = locator
                    changed = True
                
                if changed:
                    new_sys_ver = increment_system_version(db)
                    var.variable_version += 1
                    var.system_version = new_sys_ver
                    var.last_updated = datetime.utcnow()
                    updated_count += 1
            
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
```

---

## New API Endpoints for `vars_server.py`

Add filtering by category and locator:

```python
@app.get("/api/vars")
def list_variables(
    limit: int = 100, 
    offset: int = 0,
    category: str = None,  # NEW: filter by category
    locator: str = None    # NEW: filter by locator pattern
):
    """List all variables with pagination and filtering"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        query = db.query(SystemVariable)
        
        # Apply filters
        if category:
            query = query.filter(SystemVariable.category == category)
        
        if locator:
            # Support wildcards: "sbmu.chiller[*]" matches "sbmu.chiller[n]"
            if "[*]" in locator:
                pattern = locator.replace("[*]", "[%]")
                query = query.filter(SystemVariable.locator.like(pattern))
            else:
                query = query.filter(SystemVariable.locator == locator)
        
        total = query.count()
        vars_ = query.order_by(SystemVariable.name).limit(limit).offset(offset).all()
        
        result = []
        for v in vars_:
            result.append({
                "id": v.id,
                "name": v.name,
                "display_name": v.display_name,
                "units": v.units,
                "description": v.description,
                "category": v.category,  # NEW
                "locator": v.locator,    # NEW
                "variable_version": v.variable_version,
                "system_version": v.system_version,
                "source_system": v.source_system,
                "last_updated": v.last_updated.isoformat() if v.last_updated else None
            })
        
        return {
            "total": total,
            "limit": limit,
            "offset": offset,
            "variables": result
        }


@app.get("/api/categories")
def list_categories():
    """Get all unique categories"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        categories = db.query(SystemVariable.category).distinct().all()
        return {
            "categories": [c[0] for c in categories if c[0]]
        }


@app.get("/api/locators")
def list_locators(category: str = None):
    """Get all unique locators, optionally filtered by category"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        query = db.query(SystemVariable.locator).distinct()
        
        if category:
            query = query.filter(SystemVariable.category == category)
        
        locators = query.all()
        return {
            "locators": [loc[0] for loc in locators if loc[0]]
        }
```

---

## Migration Strategy

Since you already have a database with data, you'll need to add the new columns:

### Option 1: Drop and recreate (if you don't mind losing data)

```bash
rm variables.db
python3 db_variables.py --db variables.db --config data_def.json
```

### Option 2: Add columns manually (preserve existing data)

```python
# migrate_add_category_locator.py
from sqlalchemy import create_engine, text

engine = create_engine("sqlite:///./variables.db")

with engine.connect() as conn:
    # Add category column (default to "oper")
    conn.execute(text("ALTER TABLE system_variables ADD COLUMN category VARCHAR DEFAULT 'oper' NOT NULL"))
    
    # Add locator column (nullable)
    conn.execute(text("ALTER TABLE system_variables ADD COLUMN locator VARCHAR"))
    
    # Create indexes
    conn.execute(text("CREATE INDEX ix_system_variables_category ON system_variables (category)"))
    conn.execute(text("CREATE INDEX ix_system_variables_locator ON system_variables (locator)"))
    
    conn.commit()

print("✓ Migration complete")
```

Run: `python3 migrate_add_category_locator.py`

---

## Usage Examples

### Query config variables for chillers:
```bash
curl "http://localhost:8902/api/vars?category=config&locator=sbmu.chiller[n]"
```

### Query all protection variables:
```bash
curl "http://localhost:8902/api/vars?category=prot"
```

### Get all categories:
```bash
curl "http://localhost:8902/api/categories"
```

### Get all locators for config variables:
```bash
curl "http://localhost:8902/api/locators?category=config"
```

---

Does this approach work for your needs? Should we adjust the locator format or add any other fields?
Great idea! That keeps the basic `data_def.json` simple and allows you to layer on the categorization separately. Let's create a secondary config file for metadata.

---

## Two-File Approach

### 1. **`data_def.json`** - Basic variable definitions (unchanged)

```json
[
  {
    "table": "rack:sm16",
    "items": [
      {"name": "online", "offset": 26626, "size": 2, "num": 1},
      {"name": "rs_485_1", "offset": 7182, "size": 2, "num": 16}
    ]
  },
  {
    "table": "sbmu:sm16",
    "items": [
      {"name": "soc", "offset": 100, "size": 2, "num": 1},
      {"name": "cell_voltages", "offset": 200, "size": 2, "num": 96}
    ]
  }
]
```

### 2. **`var_metadata.json`** - Category and locator mappings

```json
{
  "defaults": {
    "category": "oper",
    "locator": null
  },
  "variables": {
    "chiller_enable": {
      "category": "config",
      "locator": "sbmu.chiller[n]"
    },
    "chiller_setpoint": {
      "category": "config",
      "locator": "sbmu.chiller[n]"
    },
    "overvoltage_limit": {
      "category": "prot",
      "locator": "system.global"
    },
    "undervoltage_limit": {
      "category": "prot",
      "locator": "system.global"
    },
    "cell_voltages": {
      "category": "oper",
      "locator": "rbms.cell[n]"
    },
    "module_voltage": {
      "category": "oper",
      "locator": "rbms.module[n]"
    }
  },
  "patterns": [
    {
      "name_pattern": "^chiller_.*",
      "category": "config",
      "locator": "sbmu.chiller[n]"
    },
    {
      "name_pattern": "^cell_.*",
      "category": "oper",
      "locator": "rbms.cell[n]"
    },
    {
      "name_pattern": "^module_.*",
      "category": "oper",
      "locator": "rbms.module[n]"
    },
    {
      "name_pattern": ".*_limit$",
      "category": "prot",
      "locator": "system.global"
    }
  ]
}
```

---

## Updated `db_variables.py`

### Add metadata loading function:

```python
import re

def load_metadata_config(filepath: str) -> dict:
    """Load variable metadata configuration"""
    print(f"Loading metadata config: {filepath}")
    with open(filepath, 'r') as f:
        return json.load(f)


def apply_metadata(var_name: str, metadata_config: dict) -> tuple:
    """
    Apply metadata (category, locator) to a variable name.
    
    Priority:
    1. Exact match in "variables" dict
    2. First matching pattern in "patterns" list
    3. Defaults
    
    Returns: (category, locator)
    """
    # Check exact match first
    if var_name in metadata_config.get("variables", {}):
        meta = metadata_config["variables"][var_name]
        return (
            meta.get("category", metadata_config["defaults"]["category"]),
            meta.get("locator", metadata_config["defaults"]["locator"])
        )
    
    # Check patterns
    for pattern_def in metadata_config.get("patterns", []):
        pattern = pattern_def.get("name_pattern")
        if pattern and re.match(pattern, var_name):
            return (
                pattern_def.get("category", metadata_config["defaults"]["category"]),
                pattern_def.get("locator", metadata_config["defaults"]["locator"])
            )
    
    # Use defaults
    return (
        metadata_config["defaults"]["category"],
        metadata_config["defaults"]["locator"]
    )


def import_json_definitions(db: Session, json_data: list, source_system: str = "local", metadata_config: dict = None):
    """
    Import variable definitions from JSON format.
    
    Expected format:
    [
      {
        "table": "rack:sm16",
        "items": [
          {"name": "online", "offset": 26626, "size": 2, "num": 1},
          ...
        ]
      },
      ...
    ]
    
    metadata_config: Optional dict from var_metadata.json
    """
    imported_count = 0
    skipped_count = 0
    updated_count = 0
    
    # Default metadata config if none provided
    if metadata_config is None:
        metadata_config = {
            "defaults": {"category": "oper", "locator": None},
            "variables": {},
            "patterns": []
        }
    
    for table_def in json_data:
        table_name = table_def.get("table")
        items = table_def.get("items", [])
        
        for item in items:
            var_name = item.get("name")
            offset = item.get("offset")
            size = item.get("size", 2)
            num = item.get("num", 1)
            
            # Skip gaps
            if var_name.startswith("_gap"):
                skipped_count += 1
                continue
            
            # Apply metadata
            category, locator = apply_metadata(var_name, metadata_config)
            
            # Check if variable already exists
            var = db.query(SystemVariable).filter_by(name=var_name).first()
            
            if not var:
                # Get new system version
                new_sys_ver = increment_system_version(db)
                
                # Create new variable
                display_suffix = f" [{num}]" if num > 1 else ""
                
                var = SystemVariable(
                    name=var_name,
                    display_name=var_name.replace("_", " ").title() + display_suffix,
                    units="",
                    description=f"Imported from {table_name}, {num} element(s) of size {size} bytes",
                    category=category,
                    locator=locator,
                    variable_version=1,
                    system_version=new_sys_ver,
                    source_system=source_system
                )
                db.add(var)
                db.flush()
                imported_count += 1
            else:
                # Update category/locator if they've changed
                changed = False
                if var.category != category:
                    var.category = category
                    changed = True
                if var.locator != locator:
                    var.locator = locator
                    changed = True
                
                if changed:
                    new_sys_ver = increment_system_version(db)
                    var.variable_version += 1
                    var.system_version = new_sys_ver
                    var.last_updated = datetime.utcnow()
                    updated_count += 1
            
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


def load_config_file(filepath: str, metadata_filepath: str = None):
    """Load and import JSON config file with optional metadata"""
    print(f"Loading config file: {filepath}")
    with open(filepath, 'r') as f:
        json_data = json.load(f)
    
    # Load metadata if provided
    metadata_config = None
    if metadata_filepath:
        metadata_config = load_metadata_config(metadata_filepath)
    
    with SessionLocal() as db:
        result = import_json_definitions(db, json_data, source_system="config_file", metadata_config=metadata_config)
        print(f"✓ Imported: {result['imported']} variables")
        print(f"✓ Updated: {result['updated']} access paths")
        print(f"✓ Skipped: {result['skipped']} gaps")
        print(f"✓ System version: {result['system_version']}")
```

---

## Update command-line args:

```python
def parse_args():
    """Parse command-line arguments"""
    parser = argparse.ArgumentParser(description='Variable Database Server')
    parser.add_argument('--db', type=str, default='variables.db',
                        help='Database file path (default: variables.db)')
    parser.add_argument('--config', type=str, default=None,
                        help='JSON config file to import on startup')
    parser.add_argument('--metadata', type=str, default=None,
                        help='JSON metadata file for categories and locators')
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
    config.METADATA_FILE = getattr(args, 'metadata', None)  # NEW
    config.DATA_SOURCE_IP = args.data_source_ip
    config.DATA_SOURCE_PORT = args.data_source_port
    config.SERVER_PORT = args.port
```

Update the `Config` class:

```python
class Config:
    """Global configuration"""
    DATABASE_URL = "sqlite:///./variables.db"
    CONFIG_FILE = None
    METADATA_FILE = None  # NEW
    DATA_SOURCE_IP = "127.0.0.1"
    DATA_SOURCE_PORT = 8901
    SERVER_PORT = 8902
```

Update the main block:

```python
if __name__ == "__main__":
    args = parse_args()
    apply_config(args)
    
    print("=" * 60)
    print("Variable Database Server")
    print("=" * 60)
    print(f"Database:        {config.DATABASE_URL}")
    print(f"Config file:     {config.CONFIG_FILE or '(none)'}")
    print(f"Metadata file:   {config.METADATA_FILE or '(none)'}")
    print(f"Data source:     {config.DATA_SOURCE_IP}:{config.DATA_SOURCE_PORT}")
    print(f"Server port:     {config.SERVER_PORT}")
    print("=" * 60)
    
    # Initialize database
    init_db()
    
    # Load config file if provided
    if config.CONFIG_FILE:
        try:
            load_config_file(config.CONFIG_FILE, config.METADATA_FILE)
        except Exception as e:
            print(f"✗ Error loading config file: {e}")
            sys.exit(1)
    
    print("\n✓ Ready!")
```

---

## Usage Examples

### Import with both files:
```bash
python3 db_variables.py --db variables.db --config data_def.json --metadata var_metadata.json
```

### Import with just data_def (uses defaults):
```bash
python3 db_variables.py --db variables.db --config data_def.json
```

### Update metadata later:
```bash
python3 db_variables.py --db variables.db --config data_def.json --metadata var_metadata.json
```
(This will update existing variables with new category/locator info)

---

## Example `var_metadata.json` for your system:

```json
{
  "defaults": {
    "category": "oper",
    "locator": null
  },
  "variables": {
    "online": {
      "category": "oper",
      "locator": "system.global"
    },
    "soc": {
      "category": "oper",
      "locator": "system.global"
    }
  },
  "patterns": [
    {
      "name_pattern": "^chiller_.*",
      "category": "config",
      "locator": "sbmu.chiller[n]"
    },
    {
      "name_pattern": "^cell_.*",
      "category": "oper",
      "locator": "rbms.cell[n]"
    },
    {
      "name_pattern": "^module_.*",
      "category": "oper",
      "locator": "rbms.module[n]"
    },
    {
      "name_pattern": ".*_voltage.*",
      "category": "oper",
      "locator": "rbms.module[n]"
    },
    {
      "name_pattern": ".*_limit$",
      "category": "prot",
      "locator": "system.global"
    },
    {
      "name_pattern": ".*_threshold$",
      "category": "prot",
      "locator": "system.global"
    }
  ]
}
```

---

This approach gives you:
- ✅ Keep `data_def.json` simple and unchanged
- ✅ Layer metadata separately in `var_metadata.json`
- ✅ Support exact matches and regex patterns
- ✅ Easy to update metadata without touching data definitions
- ✅ Defaults for anything not explicitly configured

Does this work for your needs?
Perfect! Let me create a summary of what we've added for the `vars_context`.

---

## Variable Registry System - Updated State (with Category & Locator)

### ✅ New Features Added

#### 1. **Extended Schema**
- Added `category` field to `SystemVariable`:
  - `"config"`: Configuration parameters
  - `"prot"`: Protection thresholds
  - `"oper"`: Operational data (default)
- Added `locator` field to `SystemVariable`:
  - Examples: `"sbmu.chiller[n]"`, `"rbms.module[n]"`, `"rbms.cell[n]"`, `"system.global"`
  - Indexed for fast filtering

#### 2. **Two-File Configuration System**

**`data_def.json`** - Basic variable definitions (unchanged):
```json
[
  {
    "table": "rack:sm16",
    "items": [
      {"name": "online", "offset": 26626, "size": 2, "num": 1}
    ]
  }
]
```

**`var_metadata.json`** - Category and locator mappings:
```json
{
  "defaults": {
    "category": "oper",
    "locator": null
  },
  "variables": {
    "chiller_enable": {
      "category": "config",
      "locator": "sbmu.chiller[n]"
    }
  },
  "patterns": [
    {
      "name_pattern": "^chiller_.*",
      "category": "config",
      "locator": "sbmu.chiller[n]"
    }
  ]
}
```

#### 3. **Metadata Application Logic**
Priority order:
1. Exact match in `variables` dict
2. First matching regex pattern in `patterns` list
3. Defaults

Functions added:
- `load_metadata_config(filepath)`: Load metadata JSON
- `apply_metadata(var_name, metadata_config)`: Apply category/locator to a variable name

#### 4. **Updated Import Function**
- `import_json_definitions()` now accepts optional `metadata_config` parameter
- Automatically applies metadata during import
- Updates existing variables if category/locator changes

#### 5. **New API Endpoints** (in `vars_server.py`)

```python
# Filter by category and/or locator
GET /api/vars?category=config&locator=sbmu.chiller[n]

# Get all unique categories
GET /api/categories

# Get all unique locators (optionally filtered by category)
GET /api/locators?category=config
```

#### 6. **Updated Command-Line Interface**

```bash
# Import with both files
python3 db_variables.py --db variables.db --config data_def.json --metadata var_metadata.json

# Import with just data_def (uses defaults)
python3 db_variables.py --db variables.db --config data_def.json

# Update metadata later
python3 db_variables.py --db variables.db --config data_def.json --metadata var_metadata.json
```

---

### 🔄 Migration Needed

If you have an existing `variables.db`, you need to add the new columns:

**Option 1: Drop and recreate**
```bash
rm variables.db
python3 db_variables.py --db variables.db --config data_def.json --metadata var_metadata.json
```

**Option 2: Migrate existing database**
```python
# migrate_add_category_locator.py
from sqlalchemy import create_engine, text

engine = create_engine("sqlite:///./variables.db")

with engine.connect() as conn:
    conn.execute(text("ALTER TABLE system_variables ADD COLUMN category VARCHAR DEFAULT 'oper' NOT NULL"))
    conn.execute(text("ALTER TABLE system_variables ADD COLUMN locator VARCHAR"))
    conn.execute(text("CREATE INDEX ix_system_variables_category ON system_variables (category)"))
    conn.execute(text("CREATE INDEX ix_system_variables_locator ON system_variables (locator)"))
    conn.commit()

print("✓ Migration complete")
```

---

### 📋 Use Cases Enabled

1. **Config UI can quickly find chiller config variables:**
   ```bash
   curl "http://localhost:8902/api/vars?category=config&locator=sbmu.chiller[*]"
   ```

2. **Protection widget can list all protection thresholds:**
   ```bash
   curl "http://localhost:8902/api/vars?category=prot"
   ```

3. **Telemetry dashboard can show all cell voltages:**
   ```bash
   curl "http://localhost:8902/api/vars?category=oper&locator=rbms.cell[*]"
   ```

4. **Get all available categories:**
   ```bash
   curl "http://localhost:8902/api/categories"
   ```

5. **Get all locators for config variables:**
   ```bash
   curl "http://localhost:8902/api/locators?category=config"
   ```

---

### 📁 Updated File Structure

```
dash3/
├── db_variables.py              # Extended with category/locator support
├── inspect_vars.py              # Shows category/locator in output
├── vars_server.py               # New filtering endpoints
├── main.py                      # Proxy for /vars_api/
├── VariableRegistryWidget.js    # Frontend widget
├── variables.db                 # SQLite database
├── data_def.json                # Basic variable definitions
├── var_metadata.json            # NEW: Category/locator mappings
└── dash.json                    # Dashboard layout
```

---

### 🎯 Next Steps (when ready)

1. Create your `var_metadata.json` with your actual categories and locators
2. Migrate or recreate your database with the new schema
3. Update `inspect_vars.py` to display category/locator
4. Update `VariableRegistryWidget.js` to show category badges and filter by category/locator
5. Add edit functionality for metadata

---

Ready to continue whenever you are! The foundation now supports hierarchical organization of your variables. 🚀
Ah, I see! You want to encode the category directly in the table name. That's a cleaner approach. Let me update both files:

---

## Updated `data_def.json` (with category in table name)

```json
[
  {
    "table": "oper:rack:sm16",
    "items": [
      {"name": "online",            "offset": 26626, "size": 2, "num": 1},
      {"name": "rs_485_1",          "offset": 7182,  "size": 2, "num": 16},
      {"name": "rs_485_2",          "offset": 7198,  "size": 2, "num": 16}
    ]
  },
  {
    "table": "config:rack:sm16",
    "items": [
      {"name": "config_cfg",        "offset": 7218,  "size": 2, "num": 20}
    ]
  },
  {
    "table": "oper:rtos:bits",
    "items": [
      {"name": "_gap1",                 "offset": 0, "size": 1, "num": 1},
      {"name": "terminal_overvoltage",  "offset": 1, "size": 1, "num": 3},
      {"name": "terminal_undervoltage", "offset": 4, "size": 1, "num": 3}
    ]
  },
  {
    "table": "oper:sbmu:input",
    "items": [
      {"name": "soc",           "offset": 100, "size": 2, "num": 1},
      {"name": "cell_voltages", "offset": 200, "size": 2, "num": 96}
    ]
  },
  {
    "table": "prot:sbmu:hold",
    "items": [
      {"name": "cell_voltage_max_warn",      "offset": 1000, "size": 2, "num": 1},
      {"name": "cell_voltage_max_warn_dur",  "offset": 1002, "size": 2, "num": 1},
      {"name": "cell_voltage_max_alert",     "offset": 1004, "size": 2, "num": 1},
      {"name": "cell_voltage_max_alert_dur", "offset": 1006, "size": 2, "num": 1},
      {"name": "cell_voltage_max_fault",     "offset": 1008, "size": 2, "num": 1},
      {"name": "cell_voltage_max_fault_dur", "offset": 1010, "size": 2, "num": 1},
      {"name": "cell_voltage_max_hyst",      "offset": 1012, "size": 2, "num": 1},
      
      {"name": "cell_voltage_min_warn",      "offset": 1020, "size": 2, "num": 1},
      {"name": "cell_voltage_min_warn_dur",  "offset": 1022, "size": 2, "num": 1},
      {"name": "cell_voltage_min_alert",     "offset": 1024, "size": 2, "num": 1},
      {"name": "cell_voltage_min_alert_dur", "offset": 1026, "size": 2, "num": 1},
      {"name": "cell_voltage_min_fault",     "offset": 1028, "size": 2, "num": 1},
      {"name": "cell_voltage_min_fault_dur", "offset": 1030, "size": 2, "num": 1},
      {"name": "cell_voltage_min_hyst",      "offset": 1032, "size": 2, "num": 1},
      
      {"name": "temperature_max_warn",       "offset": 1040, "size": 2, "num": 1},
      {"name": "temperature_max_warn_dur",   "offset": 1042, "size": 2, "num": 1},
      {"name": "temperature_max_alert",      "offset": 1044, "size": 2, "num": 1},
      {"name": "temperature_max_alert_dur",  "offset": 1046, "size": 2, "num": 1},
      {"name": "temperature_max_fault",      "offset": 1048, "size": 2, "num": 1},
      {"name": "temperature_max_fault_dur",  "offset": 1050, "size": 2, "num": 1},
      {"name": "temperature_max_hyst",       "offset": 1052, "size": 2, "num": 1},
      
      {"name": "current_max_warn",           "offset": 1060, "size": 2, "num": 1},
      {"name": "current_max_warn_dur",       "offset": 1062, "size": 2, "num": 1},
      {"name": "current_max_alert",          "offset": 1064, "size": 2, "num": 1},
      {"name": "current_max_alert_dur",      "offset": 1066, "size": 2, "num": 1},
      {"name": "current_max_fault",          "offset": 1068, "size": 2, "num": 1},
      {"name": "current_max_fault_dur",      "offset": 1070, "size": 2, "num": 1},
      {"name": "current_max_hyst",           "offset": 1072, "size": 2, "num": 1},
      
      {"name": "soc_min_warn",               "offset": 1080, "size": 2, "num": 1},
      {"name": "soc_min_warn_dur",           "offset": 1082, "size": 2, "num": 1},
      {"name": "soc_min_alert",              "offset": 1084, "size": 2, "num": 1},
      {"name": "soc_min_alert_dur",          "offset": 1086, "size": 2, "num": 1},
      {"name": "soc_min_fault",              "offset": 1088, "size": 2, "num": 1},
      {"name": "soc_min_fault_dur",          "offset": 1090, "size": 2, "num": 1},
      {"name": "soc_min_hyst",               "offset": 1092, "size": 2, "num": 1}
    ]
  }
]
```

---

## Updated `var_metadata.json` (simplified - focuses on locators)

```json
{
  "defaults": {
    "category": "oper",
    "locator": null
  },
  "variables": {
    "online": {
      "locator": "system.global"
    },
    "soc": {
      "locator": "sbmu.global"
    },
    "cell_voltages": {
      "locator": "rbms.cell[n]"
    },
    "rs_485_1": {
      "locator": "system.rs485[0]"
    },
    "rs_485_2": {
      "locator": "system.rs485[1]"
    },
    "config_cfg": {
      "locator": "system.global"
    },
    "terminal_overvoltage": {
      "locator": "system.terminal[n]"
    },
    "terminal_undervoltage": {
      "locator": "system.terminal[n]"
    }
  },
  "patterns": [
    {
      "name_pattern": "^cell_voltage_.*",
      "locator": "rbms.cell[n]"
    },
    {
      "name_pattern": "^temperature_.*",
      "locator": "rbms.module[n]"
    },
    {
      "name_pattern": "^current_.*",
      "locator": "system.global"
    },
    {
      "name_pattern": "^soc_.*",
      "locator": "sbmu.global"
    },
    {
      "name_pattern": "^chiller_.*",
      "locator": "sbmu.chiller[n]"
    },
    {
      "name_pattern": "^module_.*",
      "locator": "rbms.module[n]"
    },
    {
      "name_pattern": "^cell_.*",
      "locator": "rbms.cell[n]"
    },
    {
      "name_pattern": "^contactor_.*",
      "locator": "system.contactor[n]"
    },
    {
      "name_pattern": "^relay_.*",
      "locator": "system.relay[n]"
    }
  ]
}
```

---

## Updated `db_variables.py` (parse category from table name)

Add this function after `apply_metadata()`:

```python
def parse_table_name(table_name: str) -> tuple:
    """
    Parse table name to extract category.
    
    Format: "category:device:protocol" or "device:protocol"
    Examples:
      "prot:sbmu:input" -> ("prot", "sbmu:input")
      "config:rack:sm16" -> ("config", "rack:sm16")
      "oper:sbmu:input" -> ("oper", "sbmu:input")
      "rack:sm16" -> (None, "rack:sm16")  # No category prefix
    
    Returns: (category, remaining_table_name)
    """
    parts = table_name.split(":", 1)
    
    # Check if first part is a known category
    if len(parts) >= 2 and parts[0] in ["config", "prot", "oper"]:
        return (parts[0], parts[1])
    
    # No category prefix
    return (None, table_name)
```

Then update `import_json_definitions()` to use it:

```python
def import_json_definitions(db: Session, json_data: list, source_system: str = "local", metadata_config: dict = None):
    """
    Import variable definitions from JSON format.
    
    Expected format:
    [
      {
        "table": "prot:sbmu:input",  # Can include category prefix
        "items": [
          {"name": "cell_voltage_max_warn", "offset": 1000, "size": 2, "num": 1},
          ...
        ]
      },
      ...
    ]
    
    metadata_config: Optional dict from var_metadata.json
    """
    imported_count = 0
    skipped_count = 0
    updated_count = 0
    
    # Default metadata config if none provided
    if metadata_config is None:
        metadata_config = {
            "defaults": {"category": "oper", "locator": None},
            "variables": {},
            "patterns": []
        }
    
    for table_def in json_data:
        table_name = table_def.get("table")
        items = table_def.get("items", [])
        
        # Parse category from table name
        table_category, base_table_name = parse_table_name(table_name)
        
        for item in items:
            var_name = item.get("name")
            offset = item.get("offset")
            size = item.get("size", 2)  # Default to 16-bit (sm16)
            num = item.get("num", 1)    # Default to scalar
            
            # Skip gaps or internal placeholders
            if var_name.startswith("_gap"):
                skipped_count += 1
                continue
            
            # Apply metadata (for locator and fallback category)
            meta_category, locator = apply_metadata(var_name, metadata_config)
            
            # Table category takes precedence over metadata category
            category = table_category if table_category else meta_category
            
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
                    category=category,
                    locator=locator,
                    variable_version=1,
                    system_version=new_sys_ver,
                    source_system=source_system
                )
                db.add(var)
                db.flush()
                imported_count += 1
            else:
                # Update category/locator if they've changed
                changed = False
                if var.category != category:
                    var.category = category
                    changed = True
                if var.locator != locator:
                    var.locator = locator
                    changed = True
                
                if changed:
                    new_sys_ver = increment_system_version(db)
                    var.variable_version += 1
                    var.system_version = new_sys_ver
                    var.last_updated = datetime.utcnow()
                    updated_count += 1
            
            # Create access path (store original table name)
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
```

---

## Now the logic is:

1. **Table name category takes precedence**: `prot:sbmu:input` → all variables are `prot`
2. **Metadata provides locators**: Patterns match variable names to assign locators
3. **Metadata can override category**: If no category in table name, use metadata category

---

## Test it:

```bash
python3 db_variables.py --db variables.db --config data_def.json --metadata var_metadata.json
```

All protection variables should now be categorized as `prot` from the table name! 🎯




The docs come from **FastAPI's automatic OpenAPI/Swagger UI generation**. Here's how to customize it:

---

## 1. **Basic Customization (in FastAPI constructor)**

```python
app = FastAPI(
    title="Variable Registry API",
    description="""
## Variable Registry API

This API provides access to the system variable registry, including:

* **Variables**: CRUD operations for system variables
* **Access Paths**: Manage memory offsets, Modbus, CAN, RS485 paths
* **Filtering**: Query by category, locator, name patterns
* **Versioning**: Track changes with dual versioning system

### Categories
- `config`: Configuration variables
- `prot`: Protection thresholds and limits
- `oper`: Operational/runtime variables

### Locators
Variables are organized by physical location:
- `system.global`: System-wide variables
- `rbms.cell[n]`: Cell-level variables
- `rbms.module[n]`: Module-level variables
- `sbmu.global`: SBMU global variables
- `sbmu.chiller[n]`: Chiller-specific variables

### Versioning
- **Variable Version**: Increments when that specific variable changes
- **System Version**: Global counter that increments on any change
    """,
    version="1.0.0",
    
    # Customize docs URLs
    docs_url="/docs",           # Swagger UI (default: /docs)
    redoc_url="/redoc",         # ReDoc alternative (default: /redoc)
    openapi_url="/openapi.json", # OpenAPI schema (default: /openapi.json)
    
    # Additional metadata
    contact={
        "name": "API Support",
        "email": "support@example.com",
    },
    license_info={
        "name": "MIT",
        "url": "https://opensource.org/licenses/MIT",
    },
    
    # Terms of service
    terms_of_service="https://example.com/terms/",
    
    # OpenAPI tags metadata
    openapi_tags=[
        {
            "name": "variables",
            "description": "Operations with system variables",
        },
        {
            "name": "access-paths",
            "description": "Manage variable access paths",
        },
        {
            "name": "system",
            "description": "System information and health checks",
        },
    ]
)
```

---

## 2. **Customize Individual Endpoints**

Add detailed documentation to each endpoint:

```python
@app.get(
    "/api/variables",
    response_model=List[SystemVariableResponse],
    tags=["variables"],
    summary="List all variables",
    description="""
    Retrieve a list of system variables with optional filtering.
    
    **Filtering Options:**
    - `category`: Filter by category (config, prot, oper)
    - `locator`: Filter by physical location (supports wildcards with [*])
    - `name_pattern`: SQL LIKE pattern matching on variable names
    - `since_version`: Only return variables updated since this system version
    - `include_paths`: Include access path details (default: true)
    
    **Examples:**
    - Get all protections: `?category=prot`
    - Get all cell variables: `?locator=rbms.cell[*]`
    - Get voltage-related: `?name_pattern=voltage`
    - Sync changes: `?since_version=100`
    """,
    response_description="List of variables matching the filter criteria",
    responses={
        200: {
            "description": "Successful response",
            "content": {
                "application/json": {
                    "example": [
                        {
                            "id": 1,
                            "name": "cell_voltage_max_warn",
                            "display_name": "Cell Voltage Max Warn",
                            "units": "mV",
                            "description": "Maximum cell voltage warning threshold",
                            "category": "prot",
                            "locator": "rbms.cell[n]",
                            "variable_version": 1,
                            "system_version": 23,
                            "access_paths": []
                        }
                    ]
                }
            }
        }
    }
)
def list_variables(
    category: Optional[str] = Query(
        None, 
        description="Filter by category (config, prot, oper)",
        example="prot"
    ),
    locator: Optional[str] = Query(
        None, 
        description="Filter by locator (supports wildcards: rbms.cell[*])",
        example="rbms.cell[n]"
    ),
    name_pattern: Optional[str] = Query(
        None, 
        description="Filter by name pattern (SQL LIKE)",
        example="voltage"
    ),
    include_paths: bool = Query(
        True, 
        description="Include access paths in response"
    ),
    since_version: Optional[int] = Query(
        None, 
        description="Only return variables updated since this version",
        example=100
    ),
    db: Session = Depends(get_db)
):
    """List all variables with optional filtering"""
    # ... implementation ...
```

---

## 3. **Customize Pydantic Models**

Add examples and descriptions to your schemas:

```python
class VariableCreateRequest(BaseModel):
    name: str = Field(
        ..., 
        description="Unique variable name (snake_case)",
        example="cell_voltage_max_warn"
    )
    display_name: str = Field(
        ..., 
        description="Human-readable display name",
        example="Cell Voltage Max Warning"
    )
    units: str = Field(
        default="", 
        description="Units of measurement",
        example="mV"
    )
    description: str = Field(
        default="", 
        description="Detailed description",
        example="Maximum cell voltage warning threshold"
    )
    category: str = Field(
        default="oper", 
        description="Variable category",
        example="prot"
    )
    locator: Optional[str] = Field(
        default=None, 
        description="Physical location identifier",
        example="rbms.cell[n]"
    )
    source_system: str = Field(
        default="api", 
        description="Source system identifier",
        example="api"
    )
    
    class Config:
        schema_extra = {
            "example": {
                "name": "cell_voltage_max_warn",
                "display_name": "Cell Voltage Max Warning",
                "units": "mV",
                "description": "Maximum cell voltage warning threshold",
                "category": "prot",
                "locator": "rbms.cell[n]",
                "source_system": "api"
            }
        }
```

---

## 4. **Add Custom Static Documentation**

You can serve custom HTML/CSS for the docs:

```python
from fastapi.openapi.docs import get_swagger_ui_html, get_redoc_html
from fastapi.staticfiles import StaticFiles

# Disable default docs
app = FastAPI(docs_url=None, redoc_url=None)

# Mount static files (for custom CSS/JS)
app.mount("/static", StaticFiles(directory="static"), name="static")

@app.get("/docs", include_in_schema=False)
async def custom_swagger_ui_html():
    return get_swagger_ui_html(
        openapi_url=app.openapi_url,
        title=app.title + " - Swagger UI",
        oauth2_redirect_url=app.swagger_ui_oauth2_redirect_url,
        swagger_js_url="/static/swagger-ui-bundle.js",
        swagger_css_url="/static/swagger-ui.css",
        swagger_favicon_url="/static/favicon.png",
    )

@app.get("/redoc", include_in_schema=False)
async def redoc_html():
    return get_redoc_html(
        openapi_url=app.openapi_url,
        title=app.title + " - ReDoc",
        redoc_js_url="/static/redoc.standalone.js",
        redoc_favicon_url="/static/favicon.png",
    )
```

---

## 5. **Customize OpenAPI Schema Directly**

For advanced customization:

```python
from fastapi.openapi.utils import get_openapi

def custom_openapi():
    if app.openapi_schema:
        return app.openapi_schema
    
    openapi_schema = get_openapi(
        title="Variable Registry API",
        version="1.0.0",
        description="Custom description here",
        routes=app.routes,
    )
    
    # Add custom fields
    openapi_schema["info"]["x-logo"] = {
        "url": "https://example.com/logo.png"
    }
    
    # Add servers
    openapi_schema["servers"] = [
        {"url": "http://localhost:8902", "description": "Development"},
        {"url": "https://api.example.com", "description": "Production"},
    ]
    
    app.openapi_schema = openapi_schema
    return app.openapi_schema

app.openapi = custom_openapi
```

---

## 6. **Complete Enhanced Example**

Here's a fully customized version:

```python
from fastapi import FastAPI, Query, Field
from pydantic import BaseModel

app = FastAPI(
    title="🔋 Variable Registry API",
    description="""
# Battery Management System - Variable Registry

This API manages the complete variable registry for the BMS, including:

## 📊 Features
- **Variable Management**: CRUD operations for system variables
- **Access Paths**: Memory offsets, Modbus, CAN, RS485 mappings
- **Smart Filtering**: Query by category, location, patterns
- **Version Tracking**: Dual versioning for change management
- **Bulk Operations**: Efficient multi-variable reads

## 📁 Categories
| Category | Description | Example Variables |
|----------|-------------|-------------------|
| `config` | Configuration | rs_485_1, config_cfg |
| `prot` | Protections | cell_voltage_max_warn |
| `oper` | Operational | soc, cell_voltages |

## 📍 Locators
Variables are organized by physical location:
- `system.global` - System-wide
- `rbms.cell[n]` - Individual cells
- `rbms.module[n]` - Battery modules
- `sbmu.global` - SBMU controller
- `sbmu.chiller[n]` - Cooling system

## 🔄 Versioning
- **Variable Version**: Per-variable change counter
- **System Version**: Global change counter (for sync)

## 🚀 Quick Start
```bash
# Get all protection variables
curl http://localhost:8902/api/variables?category=prot

# Get specific variable
curl http://localhost:8902/api/variables/cell_voltage_max_warn

# Bulk read
curl -X POST http://localhost:8902/api/variables/bulk \\
  -H "Content-Type: application/json" \\
  -d '{"variable_names": ["soc", "online"]}'
```
    """,
    version="1.0.0",
    contact={
        "name": "BMS Development Team",
        "email": "bms-dev@example.com",
    },
    license_info={
        "name": "Proprietary",
    },
    openapi_tags=[
        {
            "name": "variables",
            "description": "🔧 Variable CRUD operations",
            "externalDocs": {
                "description": "Variable documentation",
                "url": "https://docs.example.com/variables",
            },
        },
        {
            "name": "access-paths",
            "description": "🛣️ Access path management",
        },
        {
            "name": "system",
            "description": "⚙️ System info and health",
        },
    ]
)
```

---

## Where the Docs Live:

1. **Swagger UI**: `http://localhost:8902/docs` (interactive, try-it-out)
2. **ReDoc**: `http://localhost:8902/redoc` (cleaner, read-only)
3. **OpenAPI JSON**: `http://localhost:8902/openapi.json` (raw schema)

The docs are **auto-generated** from your code, but you control every aspect through decorators, docstrings, and Pydantic models! 🎨