#!/usr/bin/env python3
"""
Frontend server for RBMS Dashboard
Serves static files and proxies API requests to backend services
"""

import os
import argparse
import json
import mimetypes
from typing import Optional
from fastapi import FastAPI, Request, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse, FileResponse
import httpx
import uvicorn

app = FastAPI(title="RBMS Frontend Server")

# Global variables for backend URLs
DB_SERVER_URL = "http://localhost:8084"
VAR_SERVER_URL = "http://localhost:8902"

# HTTP clients with timeout (initialized on startup)
timeout = httpx.Timeout(10.0)
db_client = None
var_client = None

# MIME types
mimetypes.init()


@app.on_event("startup")
async def startup_event():
    """Initialize HTTP clients on startup"""
    global db_client, var_client
    db_client = httpx.AsyncClient(timeout=timeout)
    var_client = httpx.AsyncClient(timeout=timeout)
    print(f"✅ HTTP clients initialized")


@app.on_event("shutdown")
async def shutdown_event():
    """Close HTTP clients on shutdown"""
    global db_client, var_client
    if db_client:
        await db_client.aclose()
    if var_client:
        await var_client.aclose()
    print(f"✅ HTTP clients closed")


@app.get("/", response_class=HTMLResponse)
async def index():
    """Serve the main dashboard page"""
    try:
        with open("index.html", "r") as f:
            content = f.read()
        return HTMLResponse(content=content)
    except FileNotFoundError:
        return HTMLResponse(content="<h1>Welcome to RBMS Dashboard</h1><p>index.html not found</p>")


@app.get("/dash.json")
async def get_dash_config():
    """Serve dashboard configuration"""
    try:
        with open("dash.json", "r") as f:
            return json.load(f)
    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="dash.json not found")


# ------------------------------
# Proxy routes for DB Server
# ------------------------------
@app.get("/api/profiles")
async def proxy_get_profiles():
    response = await db_client.get(f"{DB_SERVER_URL}/api/profiles")
    return response.json()


@app.get("/api/active_profile")
async def proxy_get_active_profile():
    response = await db_client.get(f"{DB_SERVER_URL}/api/active_profile")
    return response.json()


@app.post("/api/profiles")
async def proxy_upsert_profile(request: Request):
    body = await request.body()
    response = await db_client.post(
        f"{DB_SERVER_URL}/api/profiles",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.post("/api/profiles/select")
async def proxy_select_profile(request: Request):
    body = await request.body()
    response = await db_client.post(
        f"{DB_SERVER_URL}/api/profiles/select",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.post("/api/profiles/clone")
async def proxy_clone_profile(request: Request):
    body = await request.body()
    response = await db_client.post(
        f"{DB_SERVER_URL}/api/profiles/clone",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.post("/api/sync_dash")
async def proxy_sync_dash():
    response = await db_client.post(f"{DB_SERVER_URL}/api/sync_dash")
    return response.json()


@app.get("/api/protection_meta")
async def proxy_get_protection_meta():
    response = await db_client.get(f"{DB_SERVER_URL}/api/protection_meta")
    return response.json()


@app.get("/api/protections")
async def proxy_get_protections(source: str = "current", profile: Optional[str] = None):
    url = f"{DB_SERVER_URL}/api/protections?source={source}"
    if profile:
        url += f"&profile={profile}"
    response = await db_client.get(url)
    return response.json()


@app.post("/api/protections")
async def proxy_save_protections(request: Request):
    body = await request.body()
    response = await db_client.post(
        f"{DB_SERVER_URL}/api/protections",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.post("/api/protections/deploy")
async def proxy_deploy_protections(request: Request):
    body = await request.body()
    response = await db_client.post(
        f"{DB_SERVER_URL}/api/protections/deploy",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.get("/api/state")
async def proxy_get_state():
    response = await db_client.get(f"{DB_SERVER_URL}/api/state")
    return response.json()


@app.post("/api/state")
async def proxy_set_state(request: Request):
    body = await request.body()
    response = await db_client.post(
        f"{DB_SERVER_URL}/api/state",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.post("/api/button/{name}")
async def proxy_button_action(name: str):
    response = await db_client.post(f"{DB_SERVER_URL}/api/button/{name}")
    return response.json()


@app.post("/vars")
async def proxy_update_vars(request: Request, profile: Optional[str] = None):
    body = await request.body()
    url = f"{DB_SERVER_URL}/vars"
    if profile:
        url += f"?profile={profile}"
    response = await db_client.post(
        url,
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.get("/vars")
async def proxy_get_vars(names: str, rack: int = 0, profile: Optional[str] = None):
    url = f"{DB_SERVER_URL}/vars?names={names}&rack={rack}"
    if profile:
        url += f"&profile={profile}"
    response = await db_client.get(url)
    return response.json()


@app.get("/metrics")
async def proxy_metrics(profile: Optional[str] = None):
    url = f"{DB_SERVER_URL}/metrics"
    if profile:
        url += f"?profile={profile}"
    response = await db_client.get(url)
    return response.json()


@app.get("/series")
async def proxy_get_series(
    names: Optional[str] = None,
    category: Optional[str] = None,
    window: int = 300,
    profile: Optional[str] = None
):
    url = f"{DB_SERVER_URL}/series?"
    params = []
    if names:
        params.append(f"names={names}")
    if category:
        params.append(f"category={category}")
    params.append(f"window={window}")
    if profile:
        params.append(f"profile={profile}")
    url += "&".join(params)
    
    response = await db_client.get(url)
    return response.json()


@app.post("/series")
async def proxy_create_series(request: Request):
    body = await request.body()
    response = await db_client.post(
        f"{DB_SERVER_URL}/series",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


# ------------------------------
# Proxy routes for Variable Registry Server
# ------------------------------
@app.get("/api/variables")
async def proxy_list_variables(
    category: Optional[str] = None,
    locator: Optional[str] = None,
    name_pattern: Optional[str] = None,
    include_paths: Optional[bool] = True,
    since_version: Optional[int] = None
):
    url = f"{VAR_SERVER_URL}/api/variables?"
    params = []
    if category:
        params.append(f"category={category}")
    if locator:
        params.append(f"locator={locator}")
    if name_pattern:
        params.append(f"name_pattern={name_pattern}")
    if not include_paths:
        params.append("include_paths=false")
    if since_version is not None:
        params.append(f"since_version={since_version}")
    url += "&".join(params)
    
    response = await var_client.get(url)
    return response.json()


@app.get("/api/variables/{variable_name}")
async def proxy_get_variable(variable_name: str):
    response = await var_client.get(f"{VAR_SERVER_URL}/api/variables/{variable_name}")
    return response.json()


@app.post("/api/variables")
async def proxy_create_variable(request: Request):
    body = await request.body()
    response = await var_client.post(
        f"{VAR_SERVER_URL}/api/variables",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.put("/api/variables/{variable_name}")
async def proxy_update_variable(variable_name: str, request: Request):
    body = await request.body()
    response = await var_client.put(
        f"{VAR_SERVER_URL}/api/variables/{variable_name}",
        content=body,
        headers={"Content-Type": "application/json"}
    )
    return response.json()


@app.delete("/api/variables/{variable_name}")
async def proxy_delete_variable(variable_name: str):
    response = await var_client.delete(f"{VAR_SERVER_URL}/api/variables/{variable_name}")
    return response.json()


@app.get("/api/system/version")
async def proxy_get_system_version():
    response = await var_client.get(f"{VAR_SERVER_URL}/api/system/version")
    return response.json()


@app.get("/api/categories")
async def proxy_list_categories():
    response = await var_client.get(f"{VAR_SERVER_URL}/api/categories")
    return response.json()


@app.get("/api/locators")
async def proxy_list_locators():
    response = await var_client.get(f"{VAR_SERVER_URL}/api/locators")
    return response.json()


# ------------------------------
# Static file serving
# ------------------------------
@app.get("/{filepath:path}")
async def serve_static(filepath: str):
    """Serve static files"""
    if not filepath:
        filepath = "index.html"
    
    # Security check - prevent directory traversal
    if ".." in filepath:
        raise HTTPException(status_code=404, detail="File not found")
    
    # Determine file path
    if filepath == "index.html" or filepath.endswith(".html"):
        file_path = filepath
    else:
        file_path = os.path.join(".", filepath)
    
    # Check if file exists
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="File not found")
    
    # Guess content type
    content_type, _ = mimetypes.guess_type(file_path)
    if content_type is None:
        content_type = "application/octet-stream"
    
    return FileResponse(file_path, media_type=content_type)


def main():
    global DB_SERVER_URL, VAR_SERVER_URL
    
    parser = argparse.ArgumentParser(description="RBMS Frontend Server")
    parser.add_argument("--host", default=os.environ.get("HOST", "0.0.0.0"), help="Host to bind")
    parser.add_argument("--port", type=int, default=int(os.environ.get("PORT", "8081")), help="Port to bind")
    parser.add_argument("--data-host", default=os.environ.get("DATA_HOST", "localhost"), help="DB server host")
    parser.add_argument("--data-port", type=int, default=int(os.environ.get("DATA_PORT", "8084")), help="DB server port")
    parser.add_argument("--var-host", default=os.environ.get("VAR_HOST", "localhost"), help="Variable registry server host")
    parser.add_argument("--var-port", type=int, default=int(os.environ.get("VAR_PORT", "8085")), help="Variable registry server port")
    parser.add_argument("--reload", action="store_true", help="Enable uvicorn reload")
    
    args = parser.parse_args()
    
    # # Set backend URLs
    # DB_SERVER_URL = f"http://{args.data_host}:{args.data_port}"
    # VAR_SERVER_URL = f"http://{args.var_host}:{args.var_port}"
    
    print(f"🚀 Frontend server starting on http://{args.host}:{args.port}")
    print(f"📊 DB server at {DB_SERVER_URL}")
    print(f"📋 Variable registry at {VAR_SERVER_URL}")
    
    # Start server
    uvicorn.run("main_fixed:app", host=args.host, port=args.port, reload=args.reload)


if __name__ == "__main__":
    main()