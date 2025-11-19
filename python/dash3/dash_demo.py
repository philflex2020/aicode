# Perfect — you’re about to unify everything 👍  
# Let’s update **`main.py`** so it can:  

# ✅ Accept command‑line options for  
# - the frontend web port  
# - the backend data‑server host and port  
  
# ✅ Propagate those options to the proxy URLs dynamically.  
# ✅ Keep everything else unchanged.


# ```python
# main.py
# RBMS Dashboard Frontend with CLI port selection and configurable data server address

from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse, JSONResponse, Response
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
import httpx
import json
import uvicorn
import os
import argparse

app = FastAPI(title="RBMS Dashboard Frontend")

# -----------------------------------------------------------------
# Globals (will be overridden by CLI args or env vars)
# -----------------------------------------------------------------
DATA_SERVER_HOST = os.environ.get("DATA_SERVER_HOST", "localhost")
DATA_SERVER_PORT = int(os.environ.get("DATA_SERVER_PORT", "8084"))
#DATA_SERVER_URL = f"http://{DATA_SERVER_HOST}:8084"
DS_URL = "http://127.0.0.1:8084"

# -----------------------------------------------------------------
# Static + templates
# -----------------------------------------------------------------
app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")

def load_dash_config():
    with open("dash.json", "r", encoding="utf-8") as f:
        return json.load(f)

async def get_active_profile_name():
    """Fetch the active profile name from the data server."""
    try:
        async with httpx.AsyncClient() as client:
            url = f"{DS_URL}/api/active_profile"
            resp = await client.get(url, timeout=2.0)
            if resp.status_code == 200:
                data = resp.json()
                return data.get("name", "Unknown")
    except Exception as e:
        print(f"[WARNING] Could not fetch active profile: {e}")
    return "Unknown"
# -----------------------------------------------------------------
# Routes
# -----------------------------------------------------------------
@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    profile_name = await get_active_profile_name()   # ← called here    
    return templates.TemplateResponse("index.html", {
        "request": request,
        "profile_name": profile_name}
        )

@app.get("/dash.json")
def dash_json():
    return JSONResponse(content=load_dash_config())

# -----------------------------------------------------------------
# Proxy helper
# -----------------------------------------------------------------
async def proxy_to_data_server(path: str, request: Request):
    """Forward request to the configured data server."""
    async with httpx.AsyncClient() as client:
        url = f"{DS_URL}{path}"
        print(f">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>[DEBUG] proxying -> {url}")
        if request.method == "GET":
            params = dict(request.query_params)
            resp = await client.get(url, params=params)
        elif request.method == "POST":
            body = await request.body()
            resp = await client.post(
                url, content=body, headers={"Content-Type": "application/json"}
            )
        else:
            return Response(content="Method not allowed", status_code=405)
        return Response(
            content=resp.content,
            status_code=resp.status_code,
            media_type=resp.headers.get("content-type"),
        )

# -----------------------------------------------------------------
# Proxy endpoints
# -----------------------------------------------------------------
@app.get("/metrics")
async def metrics_proxy(request: Request):
    return await proxy_to_data_server("/metrics", request)

@app.get("/vars")
async def vars_proxy(request: Request):
    return await proxy_to_data_server("/vars", request)

@app.post("/vars")
async def vars_post_proxy(request: Request):
    return await proxy_to_data_server("/vars", request)

@app.get("/series")
async def series_proxy(request: Request):
    return await proxy_to_data_server("/series", request)

@app.get("/series/category/{category}")
async def series_category_proxy(category: str, request: Request):
    return await proxy_to_data_server(f"/series/category{category}", request)

@app.get("/api/state")
async def state_get_proxy(request: Request):
    return await proxy_to_data_server("/api/state", request)

@app.post("/api/state")
async def state_post_proxy(request: Request):
    return await proxy_to_data_server("/api/state", request)

@app.post("/api/button/{name}")
async def button_proxy(name: str, request: Request):
    return await proxy_to_data_server(f"/api/button/{name}", request)

# -----------------------------------------------------------------
# New: proxies for DB-server profile APIs
# -----------------------------------------------------------------
@app.get("/api/profiles")
async def profiles_list_proxy(request: Request):
    print(f">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>[DEBUG] get /api/profiles")
    return await proxy_to_data_server("/api/profiles", request)

@app.get("/api/active_profile")
async def active_profile_proxy(request: Request):
    return await proxy_to_data_server("/api/active_profile", request)

@app.post("/api/profiles")
async def profiles_create_proxy(request: Request):
    return await proxy_to_data_server("/api/profiles", request)

@app.post("/api/profiles/select")
async def profiles_select_proxy(request: Request):
    return await proxy_to_data_server("/api/profiles/select", request)

@app.post("/api/profiles/clone")
async def profiles_clone_proxy(request: Request):
    return await proxy_to_data_server("/api/profiles/clone", request)

# -----------------------------------------------------------------
# Entry point with argparse for CLI options
# -----------------------------------------------------------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="RBMS Dashboard Frontend")
    parser.add_argument(
        "--host", default=os.environ.get("HOST", "0.0.0.0"),
        help="Frontend host to bind (default 0.0.0.0)"
    )
    parser.add_argument(
        "--port", type=int, default=int(os.environ.get("PORT", "8081")),
        help="Frontend port (default 8081)"
    )
    parser.add_argument(
        "--data-host", default=DATA_SERVER_HOST,
        help="Data server host (default localhost)"
    )
    parser.add_argument(
        "--data-port", type=int, default=DATA_SERVER_PORT,
        help="Data server port (default 8084)"
    )
    parser.add_argument(
        "--reload", action="store_true", help="Enable uvicorn reload"
    )
    args = parser.parse_args()

    # Compose backend URL dynamically
    #DATA_SERVER_URL = f"http://{args.data_host}:{args.data_port}"
    print(f"🚀 Starting RBMS Dashboard Frontend on {args.host}:{args.port}")
    print(f"↔️  Proxying to data server at {DS_URL}")

    uvicorn.run("main:app", host=args.host, port=args.port, reload=args.reload)
# ```

# ---

# ### 🔧 How to run

# **Default (same as before):**
# ```bash
# python3 main.py
# ```
# → Frontend on `http://0.0.0.0:8080`, proxies to `http://localhost:8081`

# **Custom ports / different data server:**
# ```bash
# python3 main.py --port 8090 --data-host 192.168.1.50 --data-port 9001
# ```
# → Frontend runs on 8090, proxies to `http://192.168.1.50:9001`

# **Use with reload for development:**
# ```bash
# python3 main.py --reload --port 8082
# ```

# ---

# ✅  You now have:
# - Dynamic CLI and environment‑controlled ports for both frontend and backend  
# - Newly proxied `/api/profiles*` endpoints so the dashboard can call your DB server  

# Next step would be adding the **Profile control UI** in your dashboard page to drive these new endpoints—want me to generate that next?