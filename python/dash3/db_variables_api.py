# db_variables_api.py

# Features:
# 1. CRUD Operations
# GET /api/variables - List all variables (with filtering)
# GET /api/variables/{name} - Get specific variable
# POST /api/variables - Create new variable
# PUT /api/variables/{name} - Update variable
# DELETE /api/variables/{name} - Delete variable
# 2. Filtering & Querying
# Filter by category: ?category=prot
# Filter by locator: ?locator=rbms.cell[n]
# Wildcard locator: ?locator=rbms.cell[*]
# Name pattern: ?name_pattern=voltage
# Since version: ?since_version=100 (for sync)
# Include/exclude paths: ?include_paths=false
# 3. Bulk Operations
# POST /api/variables/bulk - Read multiple variables in one request
# 4. Access Path Management
# POST /api/access-paths - Create access path
# DELETE /api/access-paths/{id} - Deactivate access path
# 5. Metadata Endpoints
# GET /api/system/version - Get system version
# GET /api/categories - List categories with counts
# GET /api/locators - List locators with counts
# GET /api/health - Health check
# Usage:
# bash
# Copy
# # Start server with database
# python3 db_variables_api.py --db variables.db --port 8902

# # Start with config files
# python3 db_variables_api.py --db variables.db --config data_def.json --metadata var_metadata.json --port 8902
# Test it:
# bash
# Copy
# # Health check
# curl http://localhost:8902/api/health

# # List all variables
# curl http://localhost:8902/api/variables

# # List only protections
# curl http://localhost:8902/api/variables?category=prot

# # Get specific variable
# curl http://localhost:8902/api/variables/cell_voltage_max_warn

# # List categories
# curl http://localhost:8902/api/categories

# # List locators
# curl http://localhost:8902/api/locators

# # Bulk read
# curl -X POST http://localhost:8902/api/variables/bulk \
#   -H "Content-Type: application/json" \
#   -d '{"variable_names": ["soc", "cell_voltage_max_warn", "online"]}'
# Interactive API Docs:
# Visit http://localhost:8902/docs for Swagger UI with interactive testing! 🚀

# FastAPI server for variable registry
# python3 db_variables_api.py --db variables.db --port 8902
from fastapi import WebSocket, WebSocketDisconnect, BackgroundTasks
from fastapi import FastAPI, HTTPException, Query, Depends
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy.orm import Session
from typing import Optional, List   
from pydantic import BaseModel
from typing import Set, Dict
import asyncio
from datetime import datetime
import json
import argparse
import uvicorn

from db_variables import (
    init_db, get_session, config, apply_config, parse_args,
    SystemVariable, VariableAccessPath, SystemVersion,
    get_system_version, increment_system_version, sync_system_version, set_system_version
)


# ============================================================================
# Connection Manager
# ============================================================================

class ConnectionManager:
    """Manage WebSocket connections for real-time updates"""
    def __init__(self):
        self.active_connections: Dict[str, Set[WebSocket]] = {
            "all": set(),
            "variables": set(),
            "sync": set()
        }
    
    async def connect(self, websocket: WebSocket, channel: str = "all"):
        await websocket.accept()
        self.active_connections[channel].add(websocket)
        self.active_connections["all"].add(websocket)
        print(f"✓ WebSocket connected to channel '{channel}' (total: {len(self.active_connections['all'])})")
    
    def disconnect(self, websocket: WebSocket):
        for channel in self.active_connections.values():
            channel.discard(websocket)
        print(f"✓ WebSocket disconnected (remaining: {len(self.active_connections['all'])})")
    
    async def broadcast(self, message: dict, channel: str = "all"):
        """Broadcast message to all connections in a channel"""
        dead_connections = set()
        for connection in self.active_connections[channel]:
            try:
                await connection.send_json(message)
            except Exception as e:
                print(f"✗ Error sending to WebSocket: {e}")
                dead_connections.add(connection)
        
        # Clean up dead connections
        for conn in dead_connections:
            self.disconnect(conn)
    
    async def send_personal(self, message: dict, websocket: WebSocket):
        """Send message to specific connection"""
        try:
            await websocket.send_json(message)
        except Exception as e:
            print(f"✗ Error sending personal message: {e}")
            self.disconnect(websocket)

manager = ConnectionManager()

# ============================================================================
# Pydantic Models (API request/response schemas)
# ============================================================================

class VariableAccessPathResponse(BaseModel):
    id: int
    access_type: str
    reference: dict
    priority: int
    read_only: bool
    rack_number: Optional[int]
    device_id: Optional[str]
    path_version: int
    system_version: int
    active: bool
    last_updated: datetime
    
    class Config:
        from_attributes = True


class SystemVariableResponse(BaseModel):
    id: int
    name: str
    display_name: str
    units: str
    description: str
    category: str
    locator: Optional[str]
    variable_version: int
    system_version: int
    last_updated: datetime
    source_system: str
    access_paths: List[VariableAccessPathResponse] = []
    
    class Config:
        from_attributes = True


class SystemVersionResponse(BaseModel):
    system_version: int
    last_updated: datetime
    
    class Config:
        from_attributes = True


class VariableCreateRequest(BaseModel):
    name: str
    display_name: str
    units: str = ""
    description: str = ""
    category: str = "oper"
    locator: Optional[str] = None
    source_system: str = "api"


class VariableUpdateRequest(BaseModel):
    display_name: Optional[str] = None
    units: Optional[str] = None
    description: Optional[str] = None
    category: Optional[str] = None
    locator: Optional[str] = None


class AccessPathCreateRequest(BaseModel):
    variable_name: str
    access_type: str
    reference: dict
    priority: int = 10
    read_only: bool = False
    rack_number: Optional[int] = None
    device_id: Optional[str] = None


class BulkReadRequest(BaseModel):
    variable_names: List[str]


class BulkReadResponse(BaseModel):
    variables: List[SystemVariableResponse]
    system_version: int


class SyncVersionRequest(BaseModel):
    system_version: int

# ============================================================================
# FastAPI App
# ============================================================================

app = FastAPI(
    title="Variable Registry API",
    description="API for managing system variables and access paths",
    version="1.0.0"
)

# CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ============================================================================
# Dependency: Database session
# ============================================================================

def get_db():
    """Dependency to get database session"""
    SessionLocal = get_session()
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


# ============================================================================
# Helper functions
# ============================================================================

def serialize_variable(var: SystemVariable, include_paths: bool = True) -> SystemVariableResponse:
    """Convert SystemVariable to response model"""
    paths = []
    if include_paths:
        for path in var.access_paths:
            if path.active:
                paths.append(VariableAccessPathResponse(
                    id=path.id,
                    access_type=path.access_type,
                    reference=json.loads(path.reference),
                    priority=path.priority,
                    read_only=path.read_only,
                    rack_number=path.rack_number,
                    device_id=path.device_id,
                    path_version=path.path_version,
                    system_version=path.system_version,
                    active=path.active,
                    last_updated=path.last_updated
                ))
    
    return SystemVariableResponse(
        id=var.id,
        name=var.name,
        display_name=var.display_name,
        units=var.units,
        description=var.description,
        category=var.category,
        locator=var.locator,
        variable_version=var.variable_version,
        system_version=var.system_version,
        last_updated=var.last_updated,
        source_system=var.source_system,
        access_paths=paths
    )

# ============================================================================
# Sync API Endpoints
# ============================================================================

# Add after app initialization
@app.websocket("/ws/sync")
async def websocket_sync(websocket: WebSocket):
    """WebSocket endpoint for real-time sync updates"""
    await manager.connect(websocket, "sync")
    try:
        while True:
            # Keep connection alive and listen for client messages
            data = await websocket.receive_json()
            
            # Echo back for testing
            if data.get("type") == "ping":
                await manager.send_personal({"type": "pong", "timestamp": datetime.utcnow().isoformat()}, websocket)
    
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        print(f"✗ WebSocket error: {e}")
        manager.disconnect(websocket)


@app.websocket("/ws/variables")
async def websocket_variables(websocket: WebSocket):
    """WebSocket endpoint for variable change notifications"""
    await manager.connect(websocket, "variables")
    try:
        while True:
            data = await websocket.receive_json()
            
            if data.get("type") == "ping":
                await manager.send_personal({"type": "pong", "timestamp": datetime.utcnow().isoformat()}, websocket)
    
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception as e:
        print(f"✗ WebSocket error: {e}")
        manager.disconnect(websocket)


# Helper function to broadcast changes
async def broadcast_variable_change(event_type: str, variable: SystemVariable, db: Session):
    """Broadcast variable changes to all connected clients"""
    message = {
        "type": event_type,  # "created", "updated", "deleted"
        "timestamp": datetime.utcnow().isoformat(),
        "system_version": get_system_version(db),
        "variable": {
            "id": variable.id,
            "name": variable.name,
            "display_name": variable.display_name,
            "category": variable.category,
            "locator": variable.locator,
            "variable_version": variable.variable_version,
            "system_version": variable.system_version,
        }
    }
    
    await manager.broadcast(message, "variables")
    await manager.broadcast(message, "sync")


# Update the create_variable endpoint to broadcast changes
@app.post("/api/variables", response_model=SystemVariableResponse, status_code=201)
async def create_variable(request: VariableCreateRequest, background_tasks: BackgroundTasks, db: Session = Depends(get_db)):
    """Create a new variable"""
    # Check if variable already exists
    existing = db.query(SystemVariable).filter_by(name=request.name).first()
    if existing:
        raise HTTPException(status_code=409, detail=f"Variable '{request.name}' already exists")
    
    # Increment system version
    new_sys_ver = increment_system_version(db)
    
    # Create variable
    var = SystemVariable(
        name=request.name,
        display_name=request.display_name,
        units=request.units,
        description=request.description,
        category=request.category,
        locator=request.locator,
        variable_version=1,
        system_version=new_sys_ver,
        source_system=request.source_system
    )
    
    db.add(var)
    db.commit()
    db.refresh(var)
    
    # Broadcast change
    await broadcast_variable_change("created", var, db)
    
    return serialize_variable(var, include_paths=True)


# Update the update_variable endpoint
@app.put("/api/variables/{variable_name}", response_model=SystemVariableResponse)
async def update_variable(variable_name: str, request: VariableUpdateRequest, background_tasks: BackgroundTasks, db: Session = Depends(get_db)):
    """Update an existing variable"""
    var = db.query(SystemVariable).filter_by(name=variable_name).first()
    if not var:
        raise HTTPException(status_code=404, detail=f"Variable '{variable_name}' not found")
    
    # Track if anything changed
    changed = False
    
    if request.display_name is not None and request.display_name != var.display_name:
        var.display_name = request.display_name
        changed = True
    
    if request.units is not None and request.units != var.units:
        var.units = request.units
        changed = True
    
    if request.description is not None and request.description != var.description:
        var.description = request.description
        changed = True
    
    if request.category is not None and request.category != var.category:
        var.category = request.category
        changed = True
    
    if request.locator is not None and request.locator != var.locator:
        var.locator = request.locator
        changed = True
    
    if changed:
        new_sys_ver = increment_system_version(db)
        var.variable_version += 1
        var.system_version = new_sys_ver
        var.last_updated = datetime.utcnow()
        db.commit()
        db.refresh(var)
        
        # Broadcast change
        await broadcast_variable_change("updated", var, db)
    
    return serialize_variable(var, include_paths=True)


# Update the delete_variable endpoint
@app.delete("/api/variables/{variable_name}")
async def delete_variable(variable_name: str, background_tasks: BackgroundTasks, db: Session = Depends(get_db)):
    """Delete a variable (and all its access paths)"""
    var = db.query(SystemVariable).filter_by(name=variable_name).first()
    if not var:
        raise HTTPException(status_code=404, detail=f"Variable '{variable_name}' not found")
    
    # Increment system version
    increment_system_version(db)
    
    # Broadcast before deleting
    await broadcast_variable_change("deleted", var, db)
    
    db.delete(var)
    db.commit()
    
    return {"message": f"Variable '{variable_name}' deleted successfully"}

# ============================================================================
# API Endpoints
# ============================================================================

@app.get("/")
def root():
    """API root"""
    return {
        "service": "Variable Registry API",
        "version": "1.0.0",
        "endpoints": {
            "variables": "/api/variables",
            "system_version": "/api/system/version",
            "health": "/api/health"
        }
    }


@app.get("/api/health")
def health_check(db: Session = Depends(get_db)):
    """Health check endpoint"""
    try:
        sys_ver = get_system_version(db)
        var_count = db.query(SystemVariable).count()
        return {
            "status": "healthy",
            "system_version": sys_ver,
            "variable_count": var_count
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Database error: {str(e)}")


@app.get("/api/system/version", response_model=SystemVersionResponse)
def get_version(db: Session = Depends(get_db)):
    """Get current system version"""
    sys_ver = db.query(SystemVersion).filter_by(id=1).first()
    if not sys_ver:
        raise HTTPException(status_code=404, detail="System version not found")
    return sys_ver


@app.get("/api/variables", response_model=List[SystemVariableResponse])
def list_variables(
    category: Optional[str] = Query(None, description="Filter by category (config, prot, oper)"),
    locator: Optional[str] = Query(None, description="Filter by locator"),
    name_pattern: Optional[str] = Query(None, description="Filter by name pattern (SQL LIKE)"),
    include_paths: bool = Query(True, description="Include access paths"),
    since_version: Optional[int] = Query(None, description="Only return variables updated since this version"),
    db: Session = Depends(get_db)
):
    """List all variables with optional filtering"""
    query = db.query(SystemVariable)
    
    # Apply filters
    if category:
        query = query.filter(SystemVariable.category == category)
    
    if locator:
        if "[*]" in locator:
            pattern = locator.replace("[*]", "[%]")
            query = query.filter(SystemVariable.locator.like(pattern))
        else:
            query = query.filter(SystemVariable.locator == locator)
    
    if name_pattern:
        query = query.filter(SystemVariable.name.like(f"%{name_pattern}%"))
    
    if since_version is not None:
        query = query.filter(SystemVariable.system_version > since_version)
    
    vars_ = query.order_by(SystemVariable.name).all()
    
    return [serialize_variable(v, include_paths=include_paths) for v in vars_]


@app.get("/api/variables/{variable_name}", response_model=SystemVariableResponse)
def get_variable(variable_name: str, db: Session = Depends(get_db)):
    """Get a specific variable by name"""
    var = db.query(SystemVariable).filter_by(name=variable_name).first()
    if not var:
        raise HTTPException(status_code=404, detail=f"Variable '{variable_name}' not found")
    
    return serialize_variable(var, include_paths=True)


@app.post("/api/variables/bulk", response_model=BulkReadResponse)
def bulk_read_variables(request: BulkReadRequest, db: Session = Depends(get_db)):
    """Read multiple variables by name in a single request"""
    vars_ = db.query(SystemVariable).filter(SystemVariable.name.in_(request.variable_names)).all()
    
    # Check for missing variables
    found_names = {v.name for v in vars_}
    missing = set(request.variable_names) - found_names
    if missing:
        raise HTTPException(status_code=404, detail=f"Variables not found: {', '.join(missing)}")
    
    sys_ver = get_system_version(db)
    
    return BulkReadResponse(
        variables=[serialize_variable(v, include_paths=True) for v in vars_],
        system_version=sys_ver
    )


@app.post("/api/variables", response_model=SystemVariableResponse, status_code=201)
def create_variable(request: VariableCreateRequest, db: Session = Depends(get_db)):
    """Create a new variable"""
    # Check if variable already exists
    existing = db.query(SystemVariable).filter_by(name=request.name).first()
    if existing:
        raise HTTPException(status_code=409, detail=f"Variable '{request.name}' already exists")
    
    # Increment system version
    new_sys_ver = increment_system_version(db)
    
    # Create variable
    var = SystemVariable(
        name=request.name,
        display_name=request.display_name,
        units=request.units,
        description=request.description,
        category=request.category,
        locator=request.locator,
        variable_version=1,
        system_version=new_sys_ver,
        source_system=request.source_system
    )
    
    db.add(var)
    db.commit()
    db.refresh(var)
    
    return serialize_variable(var, include_paths=True)


@app.put("/api/variables/{variable_name}", response_model=SystemVariableResponse)
def update_variable(variable_name: str, request: VariableUpdateRequest, db: Session = Depends(get_db)):
    """Update an existing variable"""
    var = db.query(SystemVariable).filter_by(name=variable_name).first()
    if not var:
        raise HTTPException(status_code=404, detail=f"Variable '{variable_name}' not found")
    
    # Track if anything changed
    changed = False
    
    if request.display_name is not None and request.display_name != var.display_name:
        var.display_name = request.display_name
        changed = True
    
    if request.units is not None and request.units != var.units:
        var.units = request.units
        changed = True
    
    if request.description is not None and request.description != var.description:
        var.description = request.description
        changed = True
    
    if request.category is not None and request.category != var.category:
        var.category = request.category
        changed = True
    
    if request.locator is not None and request.locator != var.locator:
        var.locator = request.locator
        changed = True
    
    if changed:
        new_sys_ver = increment_system_version(db)
        var.variable_version += 1
        var.system_version = new_sys_ver
        var.last_updated = datetime.utcnow()
        db.commit()
        db.refresh(var)
    
    return serialize_variable(var, include_paths=True)


@app.delete("/api/variables/{variable_name}")
def delete_variable(variable_name: str, db: Session = Depends(get_db)):
    """Delete a variable (and all its access paths)"""
    var = db.query(SystemVariable).filter_by(name=variable_name).first()
    if not var:
        raise HTTPException(status_code=404, detail=f"Variable '{variable_name}' not found")
    
    # Increment system version
    increment_system_version(db)
    
    db.delete(var)
    db.commit()
    
    return {"message": f"Variable '{variable_name}' deleted successfully"}


@app.post("/api/access-paths", response_model=VariableAccessPathResponse, status_code=201)
def create_access_path(request: AccessPathCreateRequest, db: Session = Depends(get_db)):
    """Create a new access path for a variable"""
    # Find variable
    var = db.query(SystemVariable).filter_by(name=request.variable_name).first()
    if not var:
        raise HTTPException(status_code=404, detail=f"Variable '{request.variable_name}' not found")
    
    # Increment system version
    new_sys_ver = increment_system_version(db)
    
    # Create access path
    path = VariableAccessPath(
        variable_id=var.id,
        access_type=request.access_type,
        reference=json.dumps(request.reference),
        priority=request.priority,
        read_only=request.read_only,
        rack_number=request.rack_number,
        device_id=request.device_id,
        path_version=1,
        system_version=new_sys_ver,
        active=True
    )
    
    db.add(path)
    db.commit()
    db.refresh(path)
    
    return VariableAccessPathResponse(
        id=path.id,
        access_type=path.access_type,
        reference=json.loads(path.reference),
        priority=path.priority,
        read_only=path.read_only,
        rack_number=path.rack_number,
        device_id=path.device_id,
        path_version=path.path_version,
        system_version=path.system_version,
        active=path.active,
        last_updated=path.last_updated
    )


@app.delete("/api/access-paths/{path_id}")
def delete_access_path(path_id: int, db: Session = Depends(get_db)):
    """Delete (deactivate) an access path"""
    path = db.query(VariableAccessPath).filter_by(id=path_id).first()
    if not path:
        raise HTTPException(status_code=404, detail=f"Access path {path_id} not found")
    
    # Increment system version
    new_sys_ver = increment_system_version(db)
    
    # Deactivate instead of delete
    path.active = False
    path.system_version = new_sys_ver
    path.last_updated = datetime.utcnow()
    
    db.commit()
    
    return {"message": f"Access path {path_id} deactivated successfully"}


@app.get("/api/categories")
def list_categories(db: Session = Depends(get_db)):
    """List all categories with counts"""
    categories = {}
    for cat in ["config", "prot", "oper"]:
        count = db.query(SystemVariable).filter(SystemVariable.category == cat).count()
        categories[cat] = count
    
    return {
        "categories": categories,
        "total": sum(categories.values())
    }


@app.get("/api/locators")
def list_locators(db: Session = Depends(get_db)):
    """List all unique locators with counts"""
    locators = db.query(
        SystemVariable.locator,
        db.func.count(SystemVariable.id)
    ).group_by(SystemVariable.locator).all()
    
    result = {}
    for loc, count in locators:
        if loc:
            result[loc] = count
    
    return {
        "locators": result,
        "total": len(result)
    }
@app.post("/api/sync/version")
def sync_version(request: SyncVersionRequest, db: Session = Depends(get_db)):
    """Sync system version from primary server"""
    #from db_variables import sync_system_version
    
    updated = sync_system_version(db, request.system_version)
    current = get_system_version(db)
    
    return {
        "updated": updated,
        "system_version": current,
        "target_version": request.system_version
    }


@app.post("/api/sync/variable")
async def sync_variable_from_primary(
    var_data: dict,
    background_tasks: BackgroundTasks,
    db: Session = Depends(get_db)
):
    """
    Sync a variable from primary server with exact version numbers.
    This endpoint is used by sync clients to replicate variables with their versions.
    """
    var_name = var_data.get('name')
    if not var_name:
        raise HTTPException(status_code=400, detail="Variable name is required")
    
    # Check if variable exists
    var = db.query(SystemVariable).filter_by(name=var_name).first()
    
    if not var:
        # Create new variable with exact version from primary
        var = SystemVariable(
            name=var_data['name'],
            display_name=var_data['display_name'],
            units=var_data.get('units', ''),
            description=var_data.get('description', ''),
            category=var_data.get('category', 'oper'),
            locator=var_data.get('locator'),
            variable_version=var_data['variable_version'],
            system_version=var_data['system_version'],
            source_system=f"sync:{var_data.get('source_system', 'unknown')}"
        )
        db.add(var)

        # IMPORTANT: Sync system version BEFORE commit
        sync_system_version(db, var_data['system_version'])

        
        db.commit()
        db.refresh(var)
        
        # # Sync system version
        # from db_variables import sync_system_version
        # sync_system_version(db, var_data['system_version'])
        
        # Broadcast to local listeners (but not back to primary)
        await broadcast_variable_change("created", var, db)
        
        return {
            "action": "created",
            "variable": serialize_variable(var, include_paths=False)
        }
    else:
        # Update existing variable if primary version is newer
        if var_data['variable_version'] > var.variable_version:
            var.display_name = var_data['display_name']
            var.units = var_data.get('units', '')
            var.description = var_data.get('description', '')
            var.category = var_data.get('category', 'oper')
            var.locator = var_data.get('locator')
            var.variable_version = var_data['variable_version']
            var.system_version = var_data['system_version']
            var.last_updated = datetime.utcnow()
            
            
            # Sync system version
            #from db_variables import sync_system_version
            sync_system_version(db, var_data['system_version'])

            db.commit()
            db.refresh(var)

            # Broadcast to local listeners
            await broadcast_variable_change("updated", var, db)
            
            return {
                "action": "updated",
                "variable": serialize_variable(var, include_paths=False)
            }
        else:
            return {
                "action": "skipped",
                "reason": "secondary version is equal or newer",
                "variable": serialize_variable(var, include_paths=False)
            }


@app.delete("/api/sync/variable/{variable_name}")
async def sync_delete_variable(
    variable_name: str,
    system_version: int = Query(..., description="System version from primary"),
    background_tasks: BackgroundTasks = None,
    db: Session = Depends(get_db)
):
    """Sync variable deletion from primary server"""
    var = db.query(SystemVariable).filter_by(name=variable_name).first()
    if not var:
        return {"action": "skipped", "reason": "variable not found"}
    
    # Delete variable
    db.delete(var)
    db.commit()
    
    # # Sync system version
    # from db_variables import sync_system_version
    sync_system_version(db, system_version)
    
    # Broadcast to local listeners
    await broadcast_variable_change("deleted", var, db)
    
    return {"action": "deleted", "variable_name": variable_name}



# ============================================================================
# Main entry point
# ============================================================================

if __name__ == "__main__":
    args = parse_args()
    apply_config(args)
    
    print("=" * 60)
    print("Variable Registry API Server")
    print("=" * 60)
    print(f"Database:        {config.DATABASE_URL}")
    print(f"Server port:     {config.SERVER_PORT}")
    print("=" * 60)
    
    # Initialize database
    init_db()
    
    # Load config file if provided
    if config.CONFIG_FILE:
        from db_variables import load_config_file
        try:
            load_config_file(config.CONFIG_FILE, config.METADATA_FILE)
        except Exception as e:
            print(f"✗ Error loading config file: {e}")
    
    print(f"\n✓ Starting API server on port {config.SERVER_PORT}...")
    print(f"✓ API docs: http://localhost:{config.SERVER_PORT}/docs")
    print(f"✓ Health check: http://localhost:{config.SERVER_PORT}/api/health")
    
    # Run server
    uvicorn.run(app, host="0.0.0.0", port=config.SERVER_PORT)