```markdown
# FractalBMS Power System 3D Visualization ("Space Balls")

**Version:** 0.1  
**Date:** 2025-11-21  
**Status:** Experimental Demo

---

## 1. Overview

The "space_balls" visualization is an experimental 3D front-end for FractalBMS-style power systems. It presents a hierarchical battery system as a set of spatial objects, rendered in a browser with Three.js and driven by live (or simulated) data from a Python WebSocket server.

The current implementation is a **self-contained demo** that can later be integrated into the broader FractalBMS Configuration UI. It focuses on:

- Representing each power system as a **sphere** connected to a central **Point of Connection (POC)**
- Visualizing **charge/discharge** and **capacity** through sphere size, position, and line colors
- Allowing **drill-down navigation** through hierarchy levels: System → Cluster → Rack → Module → Cell
- Providing **interactive picking** (hover/click) and a details panel

---

## 2. Architecture

### 2.1. Components

The demo consists of two main pieces:

#### WebSocket Data Server (Python)
**File:** `power_viz_server.py`

- Runs on port `9999` (configurable)
- Uses `asyncio` and `websockets` to:
  - Maintain a **hierarchical model** of systems, clusters, racks, modules, and cells
  - Simulate realistic **power flow** and **SOC** changes over time
  - Serve view-specific snapshots to connected clients based on their current navigation path
- Tracks each client's current view path independently
- Sends updates at 1-second intervals (configurable via `UPDATE_INTERVAL`)

#### 3D Viewer (HTML + Three.js)
**File:** `power_viz_viewer.html`

- Runs in a browser and connects to the server via WebSocket
- Renders:
  - A central **parent node** (the current "POC") as a colored sphere
  - Child items as **spheres arranged in a circle** around that parent
  - **Lines** from parent to each item, colored and shaded by power flow
- Provides:
  - **Left-click**: select object, highlight it, and show details panel
  - **Right-click**: drill down into that object (navigate to its children)
  - **Breadcrumbs + Back button**: navigate back up the hierarchy
  - **Camera orbit + zoom** via mouse drag + wheel

---

## 3. Data Model & Hierarchy

### 3.1. Hierarchy Levels

There are 5 levels in the hierarchy:

1. **Systems** - Top-level power systems (e.g., battery installations)
2. **Clusters** - Groups of racks within a system
3. **Racks** - Physical rack enclosures containing modules
4. **Modules** - Battery modules containing cells
5. **Cells** - Individual battery cells

### 3.2. Navigation Path

The navigation **view path** is represented as a list of IDs:

| Path | View |
|------|------|
| `[]` | Top-level: show all systems |
| `[system_id]` | Show clusters within that system |
| `[system_id, cluster_id]` | Show racks within that cluster |
| `[system_id, cluster_id, rack_id]` | Show modules within that rack |
| `[system_id, cluster_id, rack_id, module_id]` | Show cells within that module |

**ID Format:** Each ID is a string composed from parent IDs:
- `system_0`
- `system_0_cluster_1`
- `system_0_cluster_1_rack_0`
- `system_0_cluster_1_rack_0_module_2`
- `system_0_cluster_1_rack_0_module_2_cell_15`

### 3.3. Hierarchy Construction

On startup, the server creates a fixed hierarchy:

- `NUM_SYSTEMS` systems (currently 6), each with:
  - 2–4 clusters
  - Each cluster: 2–3 racks
  - Each rack: 3–5 modules
  - Each module: 10–20 cells

For each element, a **capacity** is assigned such that:
- System capacity is defined randomly (100-400 kWh)
- Cluster capacities sum to system capacity
- Rack capacities sum to cluster capacity
- Module capacities sum to rack capacity
- Cell capacities sum to module capacity

This ensures consistent proportional distribution for power calculations.

---

## 4. Simulation Model

### 4.1. System State

Each **system** (top level) maintains persistent state:

```python
{
    "capacity": float,      # kWh
    "soc": float,          # State of Charge, %
    "power_flow": float,   # kW (current simulated power)
    "target_power": float, # kW (slowly approached over time)
    "mode": str,           # 'idle' | 'charging' | 'discharging'
    "mode_timer": float    # seconds until next potential mode change
}
```

### 4.2. Simulation Behavior

**Mode Changes:**
- Modes change every **10–30 seconds**, biased by SOC:
  - Low SOC (< 30%): biased toward **charging** (negative power)
  - High SOC (> 85%): biased toward **discharging** (positive power)
  - Mid-range: mix of all three modes

**Power Flow:**
- `target_power` is chosen based on `mode`:
  - Charging: -30% to -80% of (capacity/2)
  - Discharging: +30% to +80% of (capacity/2)
  - Idle: -10% to +10% of (capacity/2)
- `power_flow` approaches `target_power` with a **slew rate**:
  - Max change ≈ 5% of capacity per second
  - Prevents sudden jumps
- Small random noise (±2%) is added to `power_flow` for realism

**SOC Updates:**
- SOC is updated each tick based on power flow:
  - Negative power (charging) → SOC increases
  - Positive power (discharging) → SOC decreases
- Formula: `soc_change = (energy_change / capacity) * 100`
- Clamped to 5% - 95% range

**Lower Level Distribution:**
All lower levels derive their power from the system's `power_flow` by **splitting proportionally by capacity**:

```
cluster_power = system_power × (cluster_capacity / system_capacity)
rack_power = cluster_power × (rack_capacity / cluster_capacity)
module_power = rack_power × (module_capacity / rack_capacity)
cell_power = module_power × (cell_capacity / module_capacity)
```

SOC at lower levels is approximated as system SOC ± small random offsets.

---

## 5. WebSocket Protocol

### 5.1. Client → Server Messages

**Navigate to a path:**
```json
{
  "type": "navigate",
  "path": ["system_0", "system_0_cluster_1"]
}
```

**Back one level** (implemented as navigate to shorter path):
```json
{
  "type": "navigate",
  "path": ["system_0"]
}
```

### 5.2. Server → Client Messages

Every `UPDATE_INTERVAL` seconds (1.0s), the server pushes:

```json
{
  "timestamp": "2025-11-21T12:34:56.789012",
  "view_path": ["system_0", "system_0_cluster_1"],
  "poc": {
    "id": "poc_main",
    "name": "Grid POC",
    "total_power": 123.4
  },
  "view": {
    "level": "racks",
    "parent_id": "system_0_cluster_1",
    "items": [
      {
        "id": "system_0_cluster_1_rack_0",
        "name": "Rack 0",
        "capacity": 42.0,
        "power_flow": 5.3,
        "soc": 67.8,
        "mode": "discharging"
      }
    ]
  }
}
```

**Fields:**
- `view.level`: One of `systems | clusters | racks | modules | cells`
- `view.parent_id`: ID of the current parent (null at top level)
- `view.items`: Array of child elements to display

---

## 6. 3D Visualization

### 6.1. Scene Layout

**Central Parent Node ("POC"):**
- Rendered as a sphere at `(0, 0, 0)`
- Represents the "current parent" of the view (grid, system, cluster, rack, or module)
- Labeled with a sprite just below the sphere
- **Color-coded by depth:**
  - Depth 0 (top) – Yellow (`Grid POC`)
  - Depth 1 – Cyan (system)
  - Depth 2 – Blue (cluster)
  - Depth 3 – Purple (rack)
  - Depth 4 – Magenta (module)

**Child Items:**
- Rendered as **spheres arranged in a circle** around the parent
- Circle radius varies by level:
  - Systems/clusters/racks: `distance = 60 + capacity * 0.25` (scaled)
  - Modules: `distance ≈ 100` (fixed)
  - Cells: `distance ≈ 80` (tighter circle)

### 6.2. Sphere Sizing

| Level | Radius Calculation | Purpose |
|-------|-------------------|---------|
| Systems / Clusters / Racks | `sqrt(capacity) * 0.6` (min 6) | Scale by capacity |
| Modules | Fixed: 10 units | Easy to see & click |
| Cells | Fixed: 8 units | Easy to see & click despite tiny capacity |

This avoids tiny, unclickable spheres at deep levels while keeping higher-level scaling meaningful.

### 6.3. Connection Lines & Colors

Each child has a line from the central parent.

**Color encoding (direction and strength):**

| Power Flow | Color | Intensity |
|------------|-------|-----------|
| Negative (charging) | Green gradient | Dark green (low) → Bright green (high) |
| Positive (discharging) | Red gradient | Dark red (low) → Bright red (high) |
| Near-zero (idle) | Dim gray | Low opacity |

**Line opacity** also varies with power magnitude (since `linewidth` support is limited in WebGL).

### 6.4. Labels

**Central parent label:**
- Three.js sprite (text on semi-transparent background)
- Updated dynamically based on `currentViewPath`:
  - `Grid POC`, `System 0`, `Cluster 1`, `Rack 0`, `Module 2`, etc.

**Item labels:**
- For systems / clusters / racks: text sprites above each sphere
- For modules / cells: labels can be hidden to avoid clutter (configurable)

### 6.5. User Interaction

**Camera Controls:**
- **Left mouse drag**: Orbit camera around origin (spherical coordinates)
- **Mouse wheel**: Zoom in/out
- Camera always looks at origin

**Selection (Left-click):**
- Uses Three.js `Raycaster` for picking
- On click:
  - If a child sphere is intersected:
    - Item becomes "selected"
    - Sphere glows (emissive color change to yellow)
    - **Details panel** (left side) shows:
      - ID, name
      - Capacity
      - State (Charging / Discharging / Idle)
      - Power magnitude
      - SOC
      - Voltage (for cells)
  - Clicking empty space:
    - Clears selection
    - Hides details panel and removes highlighting

**Drill-down (Right-click):**
- Right-click on a sphere:
  - Sends a `navigate` command to add that item's ID to the view path
  - Server responds with the children of that item
  - Scene updates to show new level
- At deepest level (cells), drill-down is disabled

**Navigation:**
- **Breadcrumb bar** (top center) displays current path:
  - Example: `Systems › System 0 › Cluster 1 › Rack 0 › Module 2`
  - Each segment is clickable to jump to that level
- **Back button** (below breadcrumb):
  - Brings you up one level (`path[:-1]`)
  - Hidden at top level

**Connection Status:**
- Small status box (top-right) shows:
  - Connected (green) vs Disconnected (red)
- Auto-reconnect on disconnect (2-second delay)

---

## 7. UI Elements

### 7.1. Info Panel (Top-Left)
```
Power System Visualization
─────────────────────────
Grid POC
Total Power: 123.4 kW
Level: clusters
Items: 3
Last update: 12:34:56
```

### 7.2. Breadcrumb (Top-Center)
```
Systems › System 0 › Cluster 1 › Rack 0
```
- Each segment clickable
- Current level highlighted in green

### 7.3. Back Button (Below Breadcrumb)
```
← Back
```
- Only visible when not at top level

### 7.4. Details Panel (Left Side)
```
Rack 0
─────────────────────────
ID: system_0_cluster_1_rack_0
Capacity: 42.0 kWh
State: Discharging
Power: 5.3 kW
SOC: 67.8%

Right-click to drill down
```
- Only visible when item is selected

### 7.5. Legend (Bottom-Left)
```
Legend:
━━━━━━ Discharging (to grid)
━━━━━━ Charging (from grid)
━━━━━━ Idle / Low power

Right-click to drill down
Left-click to select
```

### 7.6. Status (Top-Right)
```
● Connected
```
or
```
● Disconnected
```

---

## 8. How to Run

### 8.1. Prerequisites

**Server:**
```bash
pip install websockets
```

**Client:**
- Modern browser (Chrome, Firefox, Edge)
- No additional dependencies (Three.js loaded from CDN)

### 8.2. Start the Server

```bash
python3 power_viz_server.py
```

Output:
```
Starting Power Visualization WebSocket Server on port 9999...
Hierarchy initialized:
  - 6 systems
  - Each system has 2-4 clusters
  - Each cluster has 2-3 racks
  - Each rack has 3-5 modules
  - Each module has 10-20 cells
```

### 8.3. Configure the Client

Edit `power_viz_viewer.html` and update the WebSocket URL (around line 450):

```javascript
ws = new WebSocket('ws://YOUR_SERVER_IP:9999');
```

Replace `YOUR_SERVER_IP` with:
- `localhost` if running on same machine
- Server's IP address if running remotely

### 8.4. Open the Viewer

Open `power_viz_viewer.html` in your browser.

You should see:
- 6 blue spheres arranged in a circle around a yellow central sphere
- Green/red lines showing power flow
- "Connected" status in top-right

### 8.5. Interact

1. **Rotate view**: Left-click and drag
2. **Zoom**: Mouse wheel
3. **Select item**: Left-click on a sphere
4. **Drill down**: Right-click on a sphere
5. **Navigate back**: Click "Back" button or breadcrumb segments

---

## 9. Current Status

### 9.1. What's Working

✅ Hierarchical simulation with realistic power flows and SOC  
✅ Full 3D interactive view with drill-down navigation  
✅ Selection, highlighting, and details panel  
✅ Visual encoding of capacity and power  
✅ Dynamic central parent label and color by depth  
✅ Cells and modules sized for easy clicking  
✅ Smooth power transitions (no "bouncing")  
✅ Multi-client support with independent navigation  
✅ Auto-reconnect on disconnect  

### 9.2. Future Work / Open Items

**Integration:**
- [ ] Hook into real FractalBMS telemetry data
- [ ] Connect to actual configuration models
- [ ] Integrate with existing dashboard UI

**Visualization Enhancements:**
- [ ] 3D orbits / gravitation-style layouts
- [ ] Grouping by side/row/phase
- [ ] Time history playback / trails
- [ ] Heatmaps for SOC/temperature
- [ ] Alarm/fault indicators

**Features:**
- [ ] Filtering by thresholds
- [ ] Aggregations at each level
- [ ] Export/screenshot capability
- [ ] Touch device support
- [ ] VR/AR mode

**Polish:**
- [ ] UI/UX improvements
- [ ] Responsive design
- [ ] Loading states
- [ ] Error handling
- [ ] Performance optimization for large hierarchies

---

## 10. Technical Notes

### 10.1. Performance Considerations

**Current limits:**
- Tested with 6 systems, ~500 total cells
- Rendering performance depends on:
  - Number of visible items (currently max ~20 per view)
  - Browser WebGL capabilities
  - Update rate (currently 1 Hz)

**Optimization strategies for larger systems:**
- Level-of-detail (LOD) rendering
- Instanced rendering for cells
- Culling off-screen objects
- Reduce update rate at deeper levels

### 10.2. Browser Compatibility

**Tested on:**
- Chrome 120+
- Firefox 121+
- Edge 120+

**Known issues:**
- Safari: Limited WebGL line width support (lines appear thin)
- Mobile: Touch controls not yet implemented

### 10.3. WebSocket Considerations

**Connection handling:**
- Auto-reconnect with 2-second delay
- Client state (view path) is lost on disconnect
- Server does not persist client state

**Scalability:**
- Current implementation: one update per client per second
- For many clients: consider pub/sub pattern or rate limiting

---

## 11. File Structure

```
space_balls/
├── power_viz_server.py      # WebSocket server with simulation
├── power_viz_viewer.html    # 3D viewer (standalone HTML)
└── CONTEXT_SPACE_BALLS.md   # This document
```

---

## 12. Integration with FractalBMS

### 12.1. Current Status

The "space_balls" visualization is currently a **standalone demo** and is **not integrated** with the main FractalBMS Configuration UI.

### 12.2. Future Integration Points

**Data Sources:**
- Replace simulated data with real telemetry from `db_server.py`
- Use actual hierarchy from configuration profiles
- Subscribe to live updates via existing WebSocket infrastructure

**UI Integration:**
- Add as a new tab in the main dashboard (`dash.json`)
- Share authentication/session management
- Coordinate with existing widgets (e.g., clicking a cell in 3D view updates IndexGridWidget)

**Configuration:**
- Allow users to customize:
  - Color schemes
  - Layout algorithms
  - Visible metrics
  - Update rates

### 12.3. Proposed Architecture

```
┌─────────────────────────────────────────┐
│         FractalBMS Web UI               │
│  ┌────────────┐  ┌──────────────────┐  │
│  │ Dashboard  │  │  Space Balls 3D  │  │
│  │  (2D)      │  │     Viewer       │  │
│  └────────────┘  └──────────────────┘  │
└─────────────────────────────────────────┘
              │
              ▼
    ┌──────────────────┐
    │   db_server.py   │
    │  (FastAPI + WS)  │
    └──────────────────┘
              │
              ▼
    ┌──────────────────┐
    │  Telemetry Data  │
    │   (profiles,     │
    │   series, etc.)  │
    └──────────────────┘
```

---

## 13. Development Notes

### 13.1. Adding New Hierarchy Levels

To add a new level (e.g., "Strings" between Racks and Modules):

1. Update `initialize_hierarchy()` in server
2. Add level name to `levelNames` array in client
3. Update `generate_view_data()` to handle new level
4. Add color to `updatePocColor()` colors array
5. Test navigation through new level

### 13.2. Customizing Visualization

**Sphere colors:**
- Edit `updatePocColor()` for central sphere
- Edit `updateItemObject()` for child spheres

**Layout algorithm:**
- Edit `updateItemObject()` distance and angle calculations
- Current: simple circle layout
- Alternatives: grid, spiral, force-directed, etc.

**Line styles:**
- Edit `updateItemObject()` line color/opacity logic
- Consider adding line thickness (limited WebGL support)

### 13.3. Debugging

**Server-side:**
```python
# Add logging
import logging
logging.basicConfig(level=logging.DEBUG)
```

**Client-side:**
- Open browser console (F12)
- Check for WebSocket messages
- Inspect `currentViewPath` and `currentViewData` globals

---

## 14. Known Issues

1. **Line width**: WebGL has limited support for line width > 1. Lines may appear thin on some systems.
   - **Workaround**: Using opacity to indicate power magnitude

2. **Label overlap**: At deep levels with many items, labels can overlap.
   - **Workaround**: Hide labels at module/cell levels

3. **Mobile support**: Touch controls not implemented.
   - **Status**: Desktop-only for now

4. **Large hierarchies**: Performance degrades with >1000 visible items.
   - **Status**: Current design limits visible items to ~20 per view

---

## 15. References

**Technologies:**
- [Three.js](https://threejs.org/) - 3D rendering library
- [WebSockets](https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API) - Real-time communication
- [Python asyncio](https://docs.python.org/3/library/asyncio.html) - Async server framework

**Related FractalBMS Documents:**
- `CONTEXT.md` - Main project context
- `dash.json` - Dashboard configuration
- `db_server.py` - Data server implementation

---

## 16. License & Credits

Part of the FractalBMS Configuration UI project.

**Author:** Phil Wilshire + Friends
**Date:** 2025-11-21  
**Version:** 0.1 (Experimental)

---

## Appendix A: Message Examples

### A.1. Top-Level View (Systems)

**Server → Client:**
```json
{
  "timestamp": "2025-11-21T14:23:45.123456",
  "view_path": [],
  "poc": {
    "id": "poc_main",
    "name": "Grid POC",
    "total_power": 234.5
  },
  "view": {
    "level": "systems",
    "parent_id": null,
    "items": [
      {
        "id": "system_0",
        "name": "Power System 0",
        "capacity": 287.3,
        "power_flow": 45.2,
        "soc": 67.8,
        "mode": "discharging"
      },
      {
        "id": "system_1",
        "name": "Power System 1",
        "capacity": 312.1,
        "power_flow": -23.4,
        "soc": 45.2,
        "mode": "charging"
      }
    ]
  }
}
```

### A.2. Drill-Down Request

**Client → Server:**
```json
{
  "type": "navigate",
  "path": ["system_0"]
}
```

### A.3. Cluster View

**Server → Client:**
```json
{
  "timestamp": "2025-11-21T14:23:46.234567",
  "view_path": ["system_0"],
  "poc": {
    "id": "poc_main",
    "name": "Grid POC",
    "total_power": 234.5
  },
  "view": {
    "level": "clusters",
    "parent_id": "system_0",
    "items": [
      {
        "id": "system_0_cluster_0",
        "name": "Cluster 0",
        "capacity": 95.8,
        "power_flow": 15.1,
        "soc": 68.3,
        "mode": "discharging"
      },
      {
        "id": "system_0_cluster_1",
        "name": "Cluster 1",
        "capacity": 95.8,
        "power_flow": 15.1,
        "soc": 67.2,
        "mode": "discharging"
      },
      {
        "id": "system_0_cluster_2",
        "name": "Cluster 2",
        "capacity": 95.7,
        "power_flow": 15.0,
        "soc": 68.0,
        "mode": "discharging"
      }
    ]
  }
}
```

---

**End of Document**
```

This markdown document is ready to save as `CONTEXT_SPACE_BALLS.md` in your project. It provides comprehensive documentation of the current state of the 3D visualization system, making it easy to resume work later.