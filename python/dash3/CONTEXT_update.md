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
###################################################################### indexgrid widget  ##########################################
# ---

# ✅  You now have:
# - Dynamic CLI and environment‑controlled ports for both frontend and backend  
# - Newly proxied `/api/profiles*` endpoints so the dashboard can call your DB server  

# Next step would be adding the **Profile control UI** in your dashboard page to drive these new endpoints—want me to generate that next?

Perfect, that simplifies things.

- Backend gives you:  
  `cell_volts: [3.21, 3.22, ...]` (one array per metric)  
- You’ll also have “modules” (or other indexed things), so this shouldn’t be cell‑specific.

Let’s rename the widget to something generic like **IndexGridWidget** (or **ArrayGridWidget**). I’ll call it `IndexGridWidget` and use `type: "indexgrid"` in `dash.json`.

Below is a ready‑to‑drop‑in implementation for `widgets.js` that matches your code style and data shape.

---

### 1. `dash.json` config example

Add a widget like:

```json
{
  "id": "cells1",
  "type": "indexgrid",
  "title": "Cell / Module Data",
  "tab": "values",
  "pos": {"row": 1, "col": 1, "w": 12, "h": 5},
  "opts": {
    "totalItems": 480,
    "itemsPerBlock": 100,
    "indexLabel": "Cell #"
  },
  "series": [
    {"name": "cell_volts", "label": "Volts", "unit": "V"},
    {"name": "cell_temp",  "label": "Temp",  "unit": "°C"},
    {"name": "cell_soc",   "label": "SOC",   "unit": "%"},
    {"name": "cell_soh",   "label": "SOH",   "unit": "%"}
  ]
}
```

For modules later, you can reuse the same widget with different `indexLabel` (“Module #”) and different series names.

---

### 2. New `IndexGridWidget` class (add to `widgets.js`)

Put this after your other widget classes:

```js
// ------------------------------------------------------------------
// IndexGridWidget - generic array metrics viewer (cells, modules, etc.)
// Displays items in blocks, e.g., 100 per block
// seriesCache expected shape: { metricName: [v0, v1, v2, ...] }
// ------------------------------------------------------------------
class IndexGridWidget extends BaseWidget {
  constructor(node, cfg) {
    super(node, cfg);

    const opts = cfg.opts || {};
    this.totalItems    = opts.totalItems    || 480;    // e.g., number of cells
    this.itemsPerBlock = opts.itemsPerBlock || 100;    // e.g., 100 per block
    this.indexLabel    = opts.indexLabel    || "Index";

    // Metrics/series configuration
    // Each item: { name: "cell_volts", label: "Volts", unit: "V" }
    this.metrics = (cfg.series || []).map(s => ({
      name:  s.name,
      label: s.label || s.name,
      unit:  s.unit  || ""
    }));

    // Visible metrics (for selector menu)
    this.storeKey = `ui2.indexgrid.visible.${cfg.id || "default"}`;
    this.visible  = this.loadVisible() || this.metrics.map(m => m.name);

    this.buildTools();
    this.container = null;
    this.buildLayout();
  }

  loadVisible() {
    try {
      return JSON.parse(localStorage.getItem(this.storeKey) || "null");
    } catch (_) {
      return null;
    }
  }

  saveVisible() {
    localStorage.setItem(this.storeKey, JSON.stringify(this.visible));
  }

  buildTools() {
    // "Metrics" button and menu (similar to TableWidget)
    const btn = document.createElement("button");
    btn.className = "wbtn";
    btn.textContent = "Metrics";

    const menu = document.createElement("div");
    menu.className = "menu";

    const h4 = document.createElement("h4");
    h4.textContent = "Show metrics";
    menu.appendChild(h4);

    const metricSet = document.createElement("div");
    for (const m of this.metrics) {
      const lab = document.createElement("label");
      const cb  = document.createElement("input");
      cb.type    = "checkbox";
      cb.checked = this.visible.includes(m.name);
      cb.onchange = () => {
        const i = this.visible.indexOf(m.name);
        if (cb.checked) {
          if (i < 0) this.visible.push(m.name);
        } else {
          if (i >= 0) this.visible.splice(i, 1);
        }
        this.saveVisible();
        if (this.lastData) this.renderBlocks(this.lastData);
      };
      lab.appendChild(cb);
      lab.appendChild(document.createTextNode(m.label));
      metricSet.appendChild(lab);
    }
    menu.appendChild(metricSet);

    const row = document.createElement("div");
    row.className = "row";

    const allBtn = document.createElement("button");
    allBtn.className = "wbtn";
    allBtn.textContent = "All";
    allBtn.onclick = () => {
      this.visible = this.metrics.map(m => m.name);
      this.saveVisible();
      const labels = metricSet.children;
      for (let i = 0; i < labels.length; i++) {
        labels[i].querySelector("input").checked = true;
      }
      if (this.lastData) this.renderBlocks(this.lastData);
    };

    const noneBtn = document.createElement("button");
    noneBtn.className = "wbtn";
    noneBtn.textContent = "None";
    noneBtn.onclick = () => {
      this.visible = [];
      this.saveVisible();
      const labels = metricSet.children;
      for (let i = 0; i < labels.length; i++) {
        labels[i].querySelector("input").checked = false;
      }
      if (this.lastData) this.renderBlocks(this.lastData);
    };

    row.appendChild(allBtn);
    row.appendChild(noneBtn);
    menu.appendChild(row);

    btn.onclick = (e) => {
      e.stopPropagation();
      menu.classList.toggle("show");
    };
    document.addEventListener("click", () => menu.classList.remove("show"));

    this.tools.appendChild(btn);
    this.head.appendChild(menu);
  }

  buildLayout() {
    this.container = document.createElement("div");
    this.container.className = "indexgrid-container";
    this.body.innerHTML = "";
    this.body.appendChild(this.container);
  }

  // seriesCache: e.g., { cell_volts: [3.21,3.22,...], cell_temp: [25,26,...], ... }
  async tick(seriesCache) {
    if (!seriesCache) return;

    // Normalize: we already have arrays per metric (your format), so just clone.
    const data = {};
    for (const m of this.metrics) {
      const arr = seriesCache[m.name] || [];
      // Ensure we have at least totalItems slots, pad with null
      const vals = new Array(this.totalItems);
      for (let i = 0; i < this.totalItems; i++) {
        vals[i] = (i < arr.length) ? arr[i] : null;
      }
      data[m.name] = vals;
    }

    this.lastData = data;
    this.renderBlocks(data);
  }

  renderBlocks(data) {
    if (!this.container) return;
    this.container.innerHTML = "";

    const total = this.totalItems;
    const per   = this.itemsPerBlock;
    const blocks = Math.ceil(total / per);

    for (let b = 0; b < blocks; b++) {
      const startIdx = b * per;           // 0-based
      const endIdx   = Math.min(startIdx + per, total);

      const block = document.createElement("div");
      block.className = "indexgrid-block";

      // 1) Index number row (Cell # / Module # / etc.)
      const rowIndex = document.createElement("div");
      rowIndex.className = "indexgrid-row indexgrid-row-header";

      const labelIndex = document.createElement("div");
      labelIndex.className = "indexgrid-row-label";
      labelIndex.textContent = `${this.indexLabel} ${startIdx + 1}..${endIdx}`;
      rowIndex.appendChild(labelIndex);

      const valuesIndex = document.createElement("div");
      valuesIndex.className = "indexgrid-row-values";
      for (let idx = startIdx; idx < endIdx; idx++) {
        const span = document.createElement("span");
        span.className = "indexgrid-val indexgrid-val-header";
        span.textContent = String(idx + 1);
        valuesIndex.appendChild(span);
      }
      rowIndex.appendChild(valuesIndex);
      block.appendChild(rowIndex);

      // 2) Metric rows
      for (const m of this.metrics) {
        if (!this.visible.includes(m.name)) continue;
        const vals = data[m.name] || [];
        const row = document.createElement("div");
        row.className = "indexgrid-row";

        const label = document.createElement("div");
        label.className = "indexgrid-row-label";
        label.textContent = m.unit ? `${m.label} (${m.unit})` : m.label;
        row.appendChild(label);

        const valsDiv = document.createElement("div");
        valsDiv.className = "indexgrid-row-values";

        for (let idx = startIdx; idx < endIdx; idx++) {
          const span = document.createElement("span");
          span.className = "indexgrid-val";
          const v = vals[idx];
          span.textContent = (v == null ? "—" : String(v));
          valsDiv.appendChild(span);
        }

        row.appendChild(valsDiv);
        block.appendChild(row);
      }

      this.container.appendChild(block);
    }
  }
}
```

---

### 3. Register the widget

Update your registry at the bottom of `widgets.js`:

```js
const WidgetTypes = {
  table:    (node,cfg)=>new TableWidget(node,cfg),
  line:     (node,cfg)=>new LineWidget(node,cfg),
  dial:     (node,cfg)=>new DialWidget(node,cfg),
  vartable: (node,cfg)=>new VarTableWidget(node,cfg),
  indexgrid:(node,cfg)=>new IndexGridWidget(node,cfg)   // 👈 new
};
```

---

### 4. CSS for the grid (add to your main CSS)

```css
.indexgrid-container {
  display: flex;
  flex-direction: column;
  gap: 8px;
  width: 100%;
  height: 100%;
  overflow-y: auto;      /* scroll if many blocks */
  padding-right: 4px;
}

.indexgrid-block {
  border: 1px solid #1e2a43;
  border-radius: 4px;
  padding: 4px;
  background: #020617;
}

.indexgrid-row {
  display: flex;
  align-items: center;
  margin-bottom: 2px;
  font-size: 11px;
}

.indexgrid-row-header {
  font-weight: 600;
  color: #e5e7eb;
}

.indexgrid-row-label {
  flex: 0 0 120px;
  padding-right: 6px;
  text-align: right;
  color: #9ca3af;
}

.indexgrid-row-values {
  flex: 1 1 auto;
  overflow-x: auto;       /* horizontal scroll across many items */
  white-space: nowrap;
}

.indexgrid-val {
  display: inline-block;
  min-width: 40px;
  text-align: right;
  padding: 1px 4px;
  color: #e5e7eb;
}

.indexgrid-val-header {
  font-weight: 600;
  color: #fef9c3;
}
```

---

### 5. Wiring it to `/series`

You already have polling that builds `seriesCache` per widget. For this widget to work, `seriesCache` must look like:

```js
{
  cell_volts: [3.21, 3.22, ...],
  cell_temp:  [25.0, 25.2, ...],
  cell_soc:   [92, 91, ...],
  // etc...
}
```

If right now `/series` returns time‑series like `{name: [[t,v], ...]}`, you can either:

- Add a special handler on the backend for cell data (just give arrays), or  
- Transform inside your poller before calling `tick()`.

If you want, paste your `/series` response for a cell metric and I can show the exact transform to get the shape this widget expects.


Here's your updated `test_db_server.sh` with tests for the new cell data series endpoints:

```bash
#!/bin/bash
# test_db_server.sh
# Comprehensive test script for db_server.py endpoints

# Configuration
SERVER_HOST="localhost"
SERVER_PORT="8085"
BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"

echo "=========================================="
echo "Testing db_server.py endpoints"
echo "Base URL: ${BASE_URL}"
echo "=========================================="
echo ""

# ==========================================
# Profile Management Tests
# ==========================================
echo "### 1. Listing all profiles ###"
curl -s -X GET "${BASE_URL}/api/profiles" | jq .
echo ""

echo "### 2. Getting active profile ###"
curl -s -X GET "${BASE_URL}/api/active_profile" | jq .
echo ""

echo "### 3. Creating a new profile: 'test-hatteras-0.3.35' ###"
curl -s -X POST "${BASE_URL}/api/profiles" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test-hatteras-0.3.35",
           "build": "hatteras",
           "version": "0.3.35",
           "target": "dev-install"
         }' | jq .
echo ""

echo "### 4. Selecting 'test-hatteras-0.3.35' as active profile ###"
curl -s -X POST "${BASE_URL}/api/profiles/select" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test-hatteras-0.3.35"
         }' | jq .
echo ""

echo "### 4b. Verifying active profile is now 'test-hatteras-0.3.35' ###"
curl -s -X GET "${BASE_URL}/api/active_profile" | jq .
echo ""

# ==========================================
# Variable Management Tests
# ==========================================
echo "### 5. Updating variables for active profile ('test-hatteras-0.3.35') ###"
curl -s -X POST "${BASE_URL}/vars" \
     -H "Content-Type: application/json" \
     -d '{
           "gain": "100",
           "phase": "90",
           "link_state": "UP"
         }' | jq .
echo ""

echo "### 6. Retrieving variables for active profile ('test-hatteras-0.3.35') ###"
curl -s -X GET "${BASE_URL}/vars?names=gain,phase,link_state,errors" | jq .
echo ""

# ==========================================
# Profile Cloning Tests
# ==========================================
echo "### 7. Cloning 'test-hatteras-0.3.35' to 'test-warwick-0.3.36' ###"
curl -s -X POST "${BASE_URL}/api/profiles/clone" \
     -H "Content-Type: application/json" \
     -d '{
           "source_name": "test-hatteras-0.3.35",
           "new_name": "test-warwick-0.3.36",
           "new_version": "0.3.36",
           "new_target": "prod-install"
         }' | jq .
echo ""

echo "### 8. Listing all profiles again (should include cloned profile) ###"
curl -s -X GET "${BASE_URL}/api/profiles" | jq .
echo ""

echo "### 9. Selecting 'default' as active profile again ###"
curl -s -X POST "${BASE_URL}/api/profiles/select" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "default"
         }' | jq .
echo ""

# ==========================================
# Telemetry Series Discovery Tests
# ==========================================
echo "=========================================="
echo "Telemetry Series Tests"
echo "=========================================="
echo ""

echo "### 10. Discovery: List all available telemetry series ###"
curl -s "${BASE_URL}/series" | jq .
echo ""

echo "### 11. Discovery: Filter by category 'network' ###"
curl -s "${BASE_URL}/series?category=network" | jq .
echo ""

echo "### 12. Discovery: Filter by multiple categories 'network,system' ###"
curl -s "${BASE_URL}/series?category=network,system" | jq .
echo ""

echo "### 13. Discovery: Filter by category 'power' (cell data) ###"
curl -s "${BASE_URL}/series?category=power" | jq .
echo ""

echo "### 14. Discovery: Filter by category 'thermal' ###"
curl -s "${BASE_URL}/series?category=thermal" | jq .
echo ""

# ==========================================
# Telemetry Data Retrieval Tests
# ==========================================
echo "### 15. Data: Get scalar time-series (mbps, cpu) ###"
curl -s "${BASE_URL}/series?names=mbps,cpu&window=20" | jq .
echo ""

echo "### 16. Data: Get scalar time-series with category filter ###"
curl -s "${BASE_URL}/series?category=network&names=mbps,rtt_ms&window=10" | jq .
echo ""

# ==========================================
# Cell/Module Array Data Tests (NEW)
# ==========================================
echo "=========================================="
echo "Cell/Module Array Data Tests"
echo "=========================================="
echo ""

echo "### 17. Data: Get cell_volts array (480 values) ###"
curl -s "${BASE_URL}/series?names=cell_volts&window=10" | jq '.series.cell_volts[0][1] | length'
echo "Expected: 480"
echo ""

echo "### 18. Data: Get cell_volts array - first 10 values ###"
curl -s "${BASE_URL}/series?names=cell_volts&window=10" | jq '.series.cell_volts[0][1][0:10]'
echo ""

echo "### 19. Data: Get cell_temp array - first 10 values ###"
curl -s "${BASE_URL}/series?names=cell_temp&window=10" | jq '.series.cell_temp[0][1][0:10]'
echo ""

echo "### 20. Data: Get cell_soc array - first 10 values ###"
curl -s "${BASE_URL}/series?names=cell_soc&window=10" | jq '.series.cell_soc[0][1][0:10]'
echo ""

echo "### 21. Data: Get cell_soh array - first 10 values ###"
curl -s "${BASE_URL}/series?names=cell_soh&window=10" | jq '.series.cell_soh[0][1][0:10]'
echo ""

echo "### 22. Data: Get all cell metrics at once ###"
curl -s "${BASE_URL}/series?names=cell_volts,cell_temp,cell_soc,cell_soh&window=5" | jq 'keys'
echo ""

echo "### 23. Data: Verify cell_volts structure (timestamp + array) ###"
curl -s "${BASE_URL}/series?names=cell_volts&window=5" | jq '.series.cell_volts[0] | [.[0], (.[1] | type), (.[1] | length)]'
echo "Expected: [timestamp, \"array\", 480]"
echo ""

echo "### 24. Data: Get mixed scalar + array series ###"
curl -s "${BASE_URL}/series?names=mbps,cell_volts,temp_c,cell_temp&window=10" | jq 'keys'
echo ""

echo "### 25. Data: Cell volts with category filter (power) ###"
curl -s "${BASE_URL}/series?category=power&names=cell_volts&window=5" | jq '.series.cell_volts[0][1][0:5]'
echo ""

# ==========================================
# Telemetry Metadata Management Tests
# ==========================================
echo "=========================================="
echo "Telemetry Metadata Management Tests"
echo "=========================================="
echo ""

echo "### 26. Create new telemetry series metadata ###"
curl -s -X POST "${BASE_URL}/series" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test_voltage",
           "label": "Test Voltage",
           "unit": "V",
           "category": "power"
         }' | jq .
echo ""

echo "### 27. Verify new series appears in discovery ###"
curl -s "${BASE_URL}/series?category=power" | jq '.available[] | select(.name == "test_voltage")'
echo ""

echo "### 28. Update existing telemetry series metadata ###"
curl -s -X POST "${BASE_URL}/series" \
     -H "Content-Type: application/json" \
     -d '{
           "name": "test_voltage",
           "label": "Test Voltage (Updated)",
           "unit": "mV",
           "category": "power"
         }' | jq .
echo ""

# ==========================================
# Edge Cases and Error Handling
# ==========================================
echo "=========================================="
echo "Edge Cases and Error Handling"
echo "=========================================="
echo ""

echo "### 29. Request unknown series name ###"
curl -s "${BASE_URL}/series?names=unknown_metric&window=5" | jq .
echo "Expected: empty series object"
echo ""

echo "### 30. Request cell data with category mismatch ###"
curl -s "${BASE_URL}/series?category=network&names=cell_volts&window=5" | jq .
echo "Expected: empty series (cell_volts is in 'power' category)"
echo ""

echo "### 31. Mixed valid and invalid series names ###"
curl -s "${BASE_URL}/series?names=mbps,invalid_name,cell_volts&window=5" | jq 'keys'
echo "Expected: only mbps and cell_volts"
echo ""

# ==========================================
# Performance/Scale Tests
# ==========================================
echo "=========================================="
echo "Performance Tests"
echo "=========================================="
echo ""

echo "### 32. Request large window for cell data ###"
echo "Timing: cell_volts with window=300"
time curl -s "${BASE_URL}/series?names=cell_volts&window=300" > /dev/null
echo ""

echo "### 33. Request all cell metrics with large window ###"
echo "Timing: all cell metrics with window=100"
time curl -s "${BASE_URL}/series?names=cell_volts,cell_temp,cell_soc,cell_soh&window=100" > /dev/null
echo ""

# ==========================================
# Cleanup and Summary
# ==========================================
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo ""

echo "### 34. Final profile list ###"
curl -s -X GET "${BASE_URL}/api/profiles" | jq 'length'
echo "profiles created during test"
echo ""

echo "### 35. Final telemetry catalog count ###"
curl -s "${BASE_URL}/series" | jq '.available | length'
echo "telemetry series registered"
echo ""

echo "=========================================="
echo "All tests completed!"
echo "=========================================="
```

### Key additions for cell data testing:

**Tests 17-25**: Cell/module array data
- Verify array length (480 items)
- Sample first 10 values from each metric
- Test all cell metrics together
- Verify data structure (timestamp + array)
- Test mixed scalar + array queries
- Test category filtering with cell data

**Tests 26-28**: Metadata management
- Create new series
- Verify it appears in discovery
- Update existing series

**Tests 29-31**: Edge cases
- Unknown series names
- Category mismatches
- Mixed valid/invalid names

**Tests 32-33**: Performance
- Large window requests
- Multiple array metrics at once

### To run:

```bash
chmod +x test_db_server.sh
./test_db_server.sh
```

### Expected output highlights:

- Test 17 should print `480`
- Test 18-21 should show arrays of 10 numeric values
- Test 23 should show `[<timestamp>, "array", 480]`
- Test 30 should return empty (category filter blocks it)

This gives you comprehensive coverage of both the existing endpoints and your new cell data array functionality.

Yes, that’s built in to the `IndexGridWidget` I gave you.

Here’s how it works and what to check.

### 1. Dash config drives which metrics are available

In `dash.json` you define the metrics for the widget:

```json
{
  "id": "cells1",
  "type": "indexgrid",
  "title": "Cell Data",
  "tab": "values",
  "pos": {"row": 1, "col": 1, "w": 12, "h": 5},
  "opts": {
    "totalItems": 480,
    "itemsPerBlock": 100,
    "indexLabel": "Cell #"
  },
  "series": [
    {"name": "cell_volts", "label": "Volts", "unit": "V"},
    {"name": "cell_temp",  "label": "Temp",  "unit": "°C"},
    {"name": "cell_soc",   "label": "SOC",   "unit": "%"},
    {"name": "cell_soh",   "label": "SOH",   "unit": "%"}
  ]
}
```

Those entries become the **available fields** (rows) in the widget.

### 2. The selector box (“Metrics” menu)

In the widget’s header tools, there is a “Metrics” button with a menu like your TableWidget’s “Columns” button.

Relevant code (this is in the `IndexGridWidget` I provided):

```js
buildTools() {
  const btn = document.createElement("button");
  btn.className = "wbtn";
  btn.textContent = "Metrics";

  const menu = document.createElement("div");
  menu.className = "menu";

  const h4 = document.createElement("h4");
  h4.textContent = "Show metrics";
  menu.appendChild(h4);

  const metricSet = document.createElement("div");
  for (const m of this.metrics) {
    const lab = document.createElement("label");
    const cb  = document.createElement("input");
    cb.type    = "checkbox";
    cb.checked = this.visible.includes(m.name);
    cb.onchange = () => {
      const i = this.visible.indexOf(m.name);
      if (cb.checked) {
        if (i < 0) this.visible.push(m.name);
      } else {
        if (i >= 0) this.visible.splice(i, 1);
      }
      this.saveVisible();
      if (this.lastData) this.renderBlocks(this.lastData);
    };
    lab.appendChild(cb);
    lab.appendChild(document.createTextNode(m.label));
    metricSet.appendChild(lab);
  }
  menu.appendChild(metricSet);

  const row = document.createElement("div");
  row.className = "row";

  const allBtn = document.createElement("button");
  allBtn.className = "wbtn";
  allBtn.textContent = "All";
  allBtn.onclick = () => {
    this.visible = this.metrics.map(m => m.name);
    this.saveVisible();
    const labels = metricSet.children;
    for (let i = 0; i < labels.length; i++) {
      labels[i].querySelector("input").checked = true;
    }
    if (this.lastData) this.renderBlocks(this.lastData);
  };

  const noneBtn = document.createElement("button");
  noneBtn.className = "wbtn";
  noneBtn.textContent = "None";
  noneBtn.onclick = () => {
    this.visible = [];
    this.saveVisible();
    const labels = metricSet.children;
    for (let i = 0; i < labels.length; i++) {
      labels[i].querySelector("input").checked = false;
    }
    if (this.lastData) this.renderBlocks(this.lastData);
  };

  row.appendChild(allBtn);
  row.appendChild(noneBtn);
  menu.appendChild(row);

  btn.onclick = (e) => {
    e.stopPropagation();
    menu.classList.toggle("show");
  };
  document.addEventListener("click", () => menu.classList.remove("show"));

  this.tools.appendChild(btn);
  this.head.appendChild(menu);
}
```

This gives you:

- A **“Metrics”** button in the widget header.
- A menu with a checkbox per metric (Volts, Temp, SOC, SOH, …).
- “All” / “None” buttons.
- Selected metrics are stored in `this.visible` and persisted in `localStorage` so your choices survive reloads.

### 3. Only visible metrics are rendered as rows

In `renderBlocks` only metrics in `this.visible` are shown:

```js
for (const m of this.metrics) {
  if (!this.visible.includes(m.name)) continue;  // ← hide unselected metrics
  const vals = data[m.name] || [];
  // build row...
}
```

So, if you uncheck everything except `cell_volts`, you’ll see just:

- `Cell #` row
- `Volts (V)` row

If you then check `Temp` and `SOC`, you’ll get additional rows for those.

### 4. Make sure the widget type is registered

At the bottom of `widgets.js`:

```js
const WidgetTypes = {
  table:    (node,cfg)=>new TableWidget(node,cfg),
  line:     (node,cfg)=>new LineWidget(node,cfg),
  dial:     (node,cfg)=>new DialWidget(node,cfg),
  vartable: (node,cfg)=>new VarTableWidget(node,cfg),
  indexgrid:(node,cfg)=>new IndexGridWidget(node,cfg)   // 👈 this
};
```

As long as:

- `type: "indexgrid"` is in `dash.json` widget,
- `IndexGridWidget` is present in `widgets.js`,
- the registry maps `"indexgrid"` to `new IndexGridWidget`,

you’ll get exactly what you described: a selector box to choose which fields (volts, temp, SOC, SOH, …) are shown per cell/module.

If you want the selector label to say something different (e.g. “Fields” instead of “Metrics”), you can just change:

```js
btn.textContent = "Metrics";
h4.textContent  = "Show metrics";
```

to:

```js
btn.textContent = "Fields";
h4.textContent  = "Show fields";
```


