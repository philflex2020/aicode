You’ve got a nice foundation here; you’re basically walking into a green‑field dashboard system with a clear spec. Let me give you a focused “attack plan” for tomorrow morning, grounded in the `dash.json` and `CONTEXT.md` you pasted.

---

### 1. Clarify the Goal for Tomorrow

Given the context, a realistic target for a first working slice is:

- Backend:
  - FastAPI app with:
    - `/series` (GET) returning mock time‑series for `mbps` and `cpu`.
    - `/api/sync_dash` (POST) that parses `dash.json` and seeds `Variable` + `TelemetryMeta`.
  - SQLite DB with the models listed.
- Frontend:
  - Simple page that:
    - Loads `dash.json`.
    - Renders the four tabs: Profile, Overview, Controls, Values.
    - Renders *at least*:
      - The overview `line` widget (“Throughput”) wired to `/series?names=mbps`.
      - The overview `dial` widget (“CPU %”) wired to `/series?names=cpu`.
      - The `vartable` (“Variables”) wired to `/vars`.
    - Implements polling for the active tab.

You don’t need all metrics/alarms on day 1; you just need end‑to‑end wiring: `dash.json → DB → /series → frontend widget`.

---

### 2. Map `dash.json` to Backend Models

Your `dash.json`:

```json
{
  "tabs": [
    {"id": "profile", "title": "Profile"},
    {"id": "overview", "title": "Overview"},
    {"id": "controls", "title": "Controls"},
    {"id": "values",   "title": "Values"}
  ],
  "grid": {"cols": 12, "gap": 10, "rowHeight": 90},
  "widgets": [
    { "type": "profile_control", "title": "Build Profiles", "tab": "profile"},
    {"id": "metrics1", "type": "table", "title": "Links", "tab": "overview", "pos": {"row":1,"col":1,"w":12,"h":4}},
    {"id": "line1",    "type": "line",  "title": "Throughput", "tab": "overview", "pos": {"row":5,"col":1,"w":8,"h":4},
      "series": [{"name":"mbps","windowSec":60}]},
    {"id": "dial1",    "type": "dial",  "title": "CPU %", "tab": "overview", "pos": {"row":5,"col":9,"w":4,"h":4},
      "series": [{"name":"cpu","windowSec":60}], "opts": {"min":0, "max":100}},
    {"id": "vars_edit", "type": "vartable", "title": "Variables", "tab": "overview",
      "pos": {"row":9,"col":1,"w":12,"h":4},
      "names": ["username","mode","gain","threshold","phase","enabled","status"],
      "rack": 0}
  ],
  "controls": { ... },
  "values": {
    "columns": ["name","value","readback"],
    "fields": ["username","mode","gain","threshold","phase","enabled","status"]
  }
}
```

Backend responsibilities derived from this:

- `sync_dash_variables()`:
  - From `controls.cards[*].elements[*].id`
  - From `controls.cards[*].readbacks[*].id`
  - From `values.fields[*]`
  - From `widgets` of type `vartable` (`names[*]`)
  - For each variable name: ensure a `Variable` row exists for active profile:
    - `Variable(name, mem_area=..., mem_type=..., offset=..., value=...)` — you can stub mem_* fields on day 1.
- `sync_dash_metrics()`:
  - From `metrics` array in `dash.json` (not shown in the small file, but spec’d in CONTEXT).
  - Upsert `MetricDef`.
  - Precreate alarm variables (you can defer full alarm engine).

For tomorrow, you can:

- Implement `sync_dash_variables()` fully.
- Stub `sync_dash_metrics()` enough to not crash; you can refine later.

---

### 3. Minimum Backend Skeleton (FastAPI)

A lean outline you can build toward:

```python
# main.py
from fastapi import FastAPI
from pydantic import BaseModel
from sqlalchemy.orm import Session
from sqlalchemy import create_engine
from models import Base, Variable, TelemetryMeta  # you’ll define these
import json
from pathlib import Path

app = FastAPI()
DB_PATH = "data/database.sqlite"
engine = create_engine(f"sqlite:///{DB_PATH}", connect_args={"check_same_thread": False})
Base.metadata.create_all(engine)

DASH_PATH = Path("dash.json")

class SeriesQuery(BaseModel):
    name: str
    window: int = 60

@app.on_event("startup")
def startup():
    with engine.begin() as conn:
        init_db(conn)  # create EnvProfile, MetaKV, etc.
        sync_dash_variables(conn)
        sync_dash_metrics(conn)

@app.get("/series")
def get_series(names: str = "", window: int = 60, category: str = ""):
    # For day 1: return mock data
    # names: "mbps,cpu"
    import time
    now = int(time.time())
    result = {}
    if not names:
        # Discovery mode
        result["series"] = [
            {"name": "mbps", "category": "network"},
            {"name": "cpu",  "category": "system"},
        ]
        return result

    name_list = [n.strip() for n in names.split(",") if n.strip()]
    for name in name_list:
        # mock: last N samples
        points = []
        for i in range(window):
            t = now - (window - i)
            if name == "mbps":
                v = 100 + i * 2  # fake ramp
            elif name == "cpu":
                v = 40 + ((i * 7) % 60)
            else:
                v = 0
            points.append({"t": t, "v": v})
        result[name] = points
    return result

@app.post("/api/sync_dash")
def api_sync_dash():
    with engine.begin() as conn:
        sync_dash_variables(conn)
        sync_dash_metrics(conn)
    return {"status": "ok"}
```

You can refine models and DB flows later; this gets `/series` working so your widgets actually show something.

If you want, I can draft the SQLAlchemy models matching your `CONTEXT.md` next.

---

### 4. Frontend Plan for Tomorrow

Your `CONTEXT.md` says:

- `templates/index.html`
- `app.js` polls `/series` and updates widgets.
- Dash config is in `dash.json`.

Concrete tasks:

1. **Basic index.html**:
   - Load `dash.json` via fetch.
   - Render tabs from `tabs` array.
   - Render widget containers for active tab using positions (`pos.row/col/w/h`) in a CSS grid.

2. **Widgets to support Day 1**:
   - `line`:
     - Fetch `/series?names=mbps&window=60`.
     - Draw a very basic SVG or canvas line, or even rely on a lightweight chart library if allowed (if not, simple SVG polyline is enough for demo).
   - `dial`:
     - Fetch `/series?names=cpu&window=60`.
     - Take the last value and display:
       - A textual `%` value and a simple pseudo‑gauge (e.g., a colored bar).
   - `vartable`:
     - Fetch `/vars` (you’ll add this endpoint) and list `name/value/readback` columns per `values.columns`.

3. **Polling skeleton**:

```js
// app.js
let POLL_T = null;
let ACTIVE_TAB = "overview";

function collectSeriesNamesForActiveTab(dash) {
  const names = new Set();
  for (const w of dash.widgets || []) {
    if (w.tab !== ACTIVE_TAB) continue;
    if (!w.series) continue;
    for (const s of w.series) {
      if (s.name) names.add(s.name);
    }
  }
  return Array.from(names);
}

async function pollOnce(dash) {
  const names = collectSeriesNamesForActiveTab(dash);
  if (names.length === 0) return;
  const url = `/series?names=${encodeURIComponent(names.join(","))}&window=60`;
  const resp = await fetch(url);
  const data = await resp.json();
  // Now route data to widgets
  for (const w of dash.widgets) {
    if (w.tab !== ACTIVE_TAB) continue;
    if (!window.WIDGETS[w.id]) continue;
    window.WIDGETS[w.id].tick(data);
  }
}

function startPolling(dash) {
  if (POLL_T) clearInterval(POLL_T);
  POLL_T = setInterval(() => pollOnce(dash), 2000);
}
```

You then define `WIDGETS[line1] = { tick(data) { ... } }` etc.

---

### 5. How to Use `CONTEXT.md` as Your Guardrails

When you’re in the rush tomorrow, you can keep referring back to these anchor points:

- **Endpoints you must not break**:
  - `GET /series`
  - `POST /series`
  - `POST /api/sync_dash`
  - `GET /api/metrics`
  - `GET /api/metrics/{name}/alarms`

- **Data model mental map**:
  - `TelemetryMeta` – “what series exist”.
  - `Variable` – “name/value/readback” for controls and values.
  - `MetricDef` – metric families and their alarm levels.
  - `ProfileHistory` – audit (can be stubbed at first).

- **dash.json is the source of truth**:
  - You never hardcode widget metrics; you always read from `dash.json`.

---

### 6. If You Want a Concrete “First Hour” Checklist

When you sit down tomorrow:

1. `python -m venv venv && source venv/bin/activate`
2. `pip install fastapi uvicorn sqlalchemy aiosqlite pydantic`
3. Create:
   - `models.py` with `Base`, `Variable`, `TelemetryMeta` (EnvProfile, MetaKV later).
   - `main.py` with FastAPI skeleton + `/series` + `/api/sync_dash`.
4. Put your existing `dash.json` and `CONTEXT.md` at root.
5. `uvicorn main:app --reload --port 8085`.
6. `curl http://127.0.0.1:8085/series?names=mbps,cpu&window=10 | jq .` to confirm backend.
7. Add `templates/index.html` + `app.js` with a single `line` widget hardwired to `mbps` using fetch.
8. Once that works, generalize to reading `dash.json` and instantiating widgets dynamically.

If you tell me which part you’re most worried about (DB schema, `/series` semantics, or the JS widget plumbing), I can pre‑write that part in detail so you can mostly paste it in tomorrow.

# Perfect — you’re about to unify everything 👍  
# Let’s update **`main.py`** so it can:  

# ✅ Accept command‑line options for  
# - the frontend web port  
# - the backend data‑server host and port  
  
# ✅ Propagate those options to the proxy URLs dynamically.  
# ✅ Keep everything else unchanged.

# Here’s the updated, ready‑to‑drop‑in replacement for your **`main.py`**:

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
DATA_SERVER_PORT = int(os.environ.get("DATA_SERVER_PORT", "8085"))
DATA_SERVER_URL = f"http://{DATA_SERVER_HOST}:{DATA_SERVER_PORT}"

# -----------------------------------------------------------------
# Static + templates
# -----------------------------------------------------------------
app.mount("/static", StaticFiles(directory="static"), name="static")
templates = Jinja2Templates(directory="templates")

def load_dash_config():
    with open("dash.json", "r", encoding="utf-8") as f:
        return json.load(f)

# -----------------------------------------------------------------
# Routes
# -----------------------------------------------------------------
@app.get("/", response_class=HTMLResponse)
def index(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})

@app.get("/dash.json")
def dash_json():
    return JSONResponse(content=load_dash_config())

# -----------------------------------------------------------------
# Proxy helper
# -----------------------------------------------------------------
async def proxy_to_data_server(path: str, request: Request):
    """Forward request to the configured data server."""
    async with httpx.AsyncClient() as client:
        url = f"{DATA_SERVER_URL}{path}"
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
        "--port", type=int, default=int(os.environ.get("PORT", "8080")),
        help="Frontend port (default 8080)"
    )
    parser.add_argument(
        "--data-host", default=DATA_SERVER_HOST,
        help="Data server host (default localhost)"
    )
    parser.add_argument(
        "--data-port", type=int, default=DATA_SERVER_PORT,
        help="Data server port (default 8081)"
    )
    parser.add_argument(
        "--reload", action="store_true", help="Enable uvicorn reload"
    )
    args = parser.parse_args()

    # Compose backend URL dynamically
    DATA_SERVER_URL = f"http://{args.data_host}:{args.data_port}"
    print(f"🚀 Starting RBMS Dashboard Frontend on {args.host}:{args.port}")
    print(f"↔️  Proxying to data server at {DATA_SERVER_URL}")

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

