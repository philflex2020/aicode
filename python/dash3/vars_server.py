# vars_server.py
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from db_variables import init_db, get_session, SystemVariable, VariableAccessPath, get_system_version, config
from contextlib import asynccontextmanager
import json

import argparse

app = FastAPI(title="Variable Registry API")

# CORS for dashboard access
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ============================================================================
# Startup
# ============================================================================
# ============================================================================
# Lifespan event handler (replaces on_event)
# ============================================================================

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    init_db()
    print(f"✓ Variable Registry API ready on port {config.SERVER_PORT}")
    yield
    # Shutdown (if needed)

app = FastAPI(title="Variable Registry API", lifespan=lifespan)
# @app.on_event("startup")
# def startup():
#     init_db()
#     print(f"✓ Variable Registry API ready on port {config.SERVER_PORT}")

# ============================================================================
# Endpoints
# ============================================================================

@app.get("/")
def root():
    SessionLocal = get_session()
    with SessionLocal() as db:
        var_count = db.query(SystemVariable).count()
        sys_ver = get_system_version(db)
    
    return {
        "service": "Variable Registry API",
        "variables": var_count,
        "system_version": sys_ver,
        "database": config.DATABASE_URL
    }


@app.get("/api/vars")
def list_variables(limit: int = 100, offset: int = 0):
    """List all variables with pagination"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        total = db.query(SystemVariable).count()
        vars_ = db.query(SystemVariable).order_by(SystemVariable.name).limit(limit).offset(offset).all()
        
        result = []
        for v in vars_:
            result.append({
                "id": v.id,
                "name": v.name,
                "display_name": v.display_name,
                "units": v.units,
                "description": v.description,
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


@app.get("/api/vars/{var_name}")
def get_variable(var_name: str):
    """Get a single variable with all access paths"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        var = db.query(SystemVariable).filter_by(name=var_name).first()
        
        if not var:
            raise HTTPException(status_code=404, detail=f"Variable '{var_name}' not found")
        
        paths = []
        for p in var.access_paths:
            if p.active:
                paths.append({
                    "id": p.id,
                    "access_type": p.access_type,
                    "reference": json.loads(p.reference),
                    "priority": p.priority,
                    "read_only": p.read_only,
                    "rack_number": p.rack_number,
                    "device_id": p.device_id,
                    "path_version": p.path_version,
                    "system_version": p.system_version
                })
        
        return {
            "id": var.id,
            "name": var.name,
            "display_name": var.display_name,
            "units": var.units,
            "description": var.description,
            "variable_version": var.variable_version,
            "system_version": var.system_version,
            "source_system": var.source_system,
            "last_updated": var.last_updated.isoformat() if var.last_updated else None,
            "access_paths": paths
        }


@app.get("/api/system/version")
def system_version():
    """Get current system version"""
    SessionLocal = get_session()
    with SessionLocal() as db:
        return {
            "system_version": get_system_version(db)
        }


# ============================================================================
# Main
# ============================================================================

if __name__ == "__main__":
    import uvicorn
    
    parser = argparse.ArgumentParser(description='Variable Registry API Server')
    parser.add_argument('--db', type=str, default='variables.db', help='Database file')
    parser.add_argument('--port', type=int, default=8902, help='Server port')
    parser.add_argument('--data-source-ip', type=str, default='127.0.0.1', help='Data source IP')
    parser.add_argument('--data-source-port', type=int, default=8901, help='Data source port')
    args = parser.parse_args()
    
    # # Apply config
    # from db_variables import apply_config
    # apply_config(args)
    # Manually apply config (don't call apply_config since we don't have all fields)
    config.DATABASE_URL = f"sqlite:///./{args.db}"
    config.DATA_SOURCE_IP = args.data_source_ip
    config.DATA_SOURCE_PORT = args.data_source_port
    config.SERVER_PORT = args.port

    print("=" * 60)
    print("Variable Registry API Server")
    print("=" * 60)
    print(f"Database:        {config.DATABASE_URL}")
    print(f"Data source:     {config.DATA_SOURCE_IP}:{config.DATA_SOURCE_PORT}")
    print(f"Server port:     {config.SERVER_PORT}")
    print("=" * 60)

    uvicorn.run(app, host="0.0.0.0", port=args.port)