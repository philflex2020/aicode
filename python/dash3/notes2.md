Perfect — great progress 🎉  

Now let’s add a **“Profile Control”** panel to your dashboard that talks to the new `/api/profiles`, `/api/active_profile`, `/api/profiles/select`, and `/api/profiles/clone` endpoints.

We’ll build it so it fits neatly into your current two‑tab layout (`controls` & `values`), generated from `dash.json`, but keep it totally stand‑alone if you want to test it right now.

---

## 1️⃣ HTML — new Profile Control card

Add this to your `templates/index.html` (or wherever your controls tab container lives).  
If you’re using a tabbed interface, drop it in the *Controls* tab section, ideally above your knob/sliders.

```html
<div class="card" id="profile-card">
  <h3>Profile Control</h3>

  <div class="form-row">
    <label for="profile-name">Profile Name</label>
    <input type="text" id="profile-name" placeholder="default" />
  </div>

  <div class="form-row">
    <label for="build">Build</label>
    <select id="build">
      <option value="hatteras">Hatteras</option>
      <option value="warwick">Warwick</option>
      <option value="factory">Factory</option>
    </select>
  </div>

  <div class="form-row">
    <label for="version">Version</label>
    <input type="text" id="version" value="0.3.34" />
  </div>

  <div class="form-row">
    <label for="target">Target</label>
    <input type="text" id="target" placeholder="(optional)" />
  </div>

  <div class="button-row">
    <button id="save-profile">💾 Save Profile</button>
    <button id="select-profile">✅ Set Active</button>
    <button id="clone-profile">🧬 Clone</button>
  </div>

  <div class="form-row">
    <label for="active-profile">Active Profile:</label>
    <span id="active-profile-name">(loading...)</span>
  </div>

  <div class="form-row">
    <label for="profile-list">Existing Profiles:</label>
    <select id="profile-list"></select>
  </div>

  <p id="profile-status" class="status"></p>
</div>
```

A bit of CSS (in `static/style.css` if you have it):

```css
#profile-card {
  padding: 1em;
  margin-bottom: 1.5em;
}
.form-row {
  margin-bottom: 0.5em;
}
.form-row label {
  display: inline-block;
  width: 120px;
}
.button-row button {
  margin-right: 0.5em;
}
.status {
  margin-top: 0.5em;
  font-size: 0.9em;
  color: #4caf50;
}
```

---

## 2️⃣ JavaScript logic (add to `static/main.js` or a new JS file)

This JS calls your backend (proxied via `main.py`), uses the same `/api/...` routes, and updates the UI.

```js
async function getJSON(url) {
  const resp = await fetch(url);
  return resp.json();
}
async function postJSON(url, data) {
  const resp = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(data)
  });
  return resp.json();
}

async function updateProfileList() {
  const list = document.getElementById("profile-list");
  list.innerHTML = "";
  try {
    const profiles = await getJSON("/api/profiles");
    profiles.forEach(p => {
      const opt = document.createElement("option");
      opt.value = p.name;
      opt.textContent = `${p.name} (${p.build}, ${p.version})`;
      list.appendChild(opt);
    });
  } catch (e) {
    console.error(e);
  }
}

async function updateActiveProfile() {
  try {
    const ap = await getJSON("/api/active_profile");
    document.getElementById("active-profile-name").textContent = ap.name;
  } catch (e) {
    document.getElementById("active-profile-name").textContent = "(none)";
  }
}

async function saveProfileClicked() {
  const data = {
    name: document.getElementById("profile-name").value || "default",
    build: document.getElementById("build").value,
    version: document.getElementById("version").value,
    target: document.getElementById("target").value || null
  };
  const res = await postJSON("/api/profiles", data);
  document.getElementById("profile-status").textContent = "Profile saved.";
  console.log("saveProfile", res);
  await updateProfileList();
  await updateActiveProfile();
}

async function selectProfileClicked() {
  const name =
    document.getElementById("profile-name").value ||
    document.getElementById("profile-list").value;
  const res = await postJSON("/api/profiles/select", { name });
  document.getElementById(
    "profile-status"
  ).textContent = `Active profile: ${res.active}`;
  await updateActiveProfile();
}

async function cloneProfileClicked() {
  const source_name =
    document.getElementById("profile-name").value ||
    document.getElementById("profile-list").value;
  const new_name = prompt("New profile name:");
  if (!new_name) return;
  const res = await postJSON("/api/profiles/clone", {
    source_name,
    new_name,
    new_version: document.getElementById("version").value,
    new_target: document.getElementById("target").value
  });
  document.getElementById(
    "profile-status"
  ).textContent = `Cloned to ${res.name}.`;
  await updateProfileList();
  await updateActiveProfile();
}

// Wire up on load
window.addEventListener("DOMContentLoaded", () => {
  document
    .getElementById("save-profile")
    .addEventListener("click", () => saveProfileClicked());
  document
    .getElementById("select-profile")
    .addEventListener("click", () => selectProfileClicked());
  document
    .getElementById("clone-profile")
    .addEventListener("click", () => cloneProfileClicked());

  updateProfileList();
  updateActiveProfile();
});
```

---

## 3️⃣ Flow you’ll see

When you refresh the dashboard:
1.  It lists profiles from your `db_server` DB.
2.  Displays the active one.
3.  You can:
    - Change fields and click **💾 Save Profile**.
    - Pick one from the dropdown and click **✅ Set Active**.
    - Click **🧬 Clone** to create a new one.

---

## 4️⃣ Optional: Save in `dash.json`

Later, if you prefer this card to be dynamically created from `dash.json`, we can add a `type: "profile_control"` widget describing this configuration — but for now it’ll just appear statically on the page.

---

With this in place you now have a working **Profile Management UI** fully wired into your FastAPI + SQLite backend.

Would you like me to adjust it so that the **clone** action opens a small pop‑up form (with new version/target fields) instead of a basic `prompt()` ?


===============================================================

Exactly — since your dashboard already renders UI dynamically through its modular JS (`utils.js`, `widgets.js`, `controls.js`, `app.js`), the cleanest way is to treat **Profile Control** as a new reusable _widget type_ instead of hard‑coding HTML in `index.html`.

Let’s integrate it just like your other control widgets.

---

## 🌿 Plan (clean integration)
1. Extend `widgets.js` with a `renderProfileControl(container, config)` function.  
2. Register it under `WidgetTypes.profile_control`.  
3. Describe it in `dash.json` so it appears automatically in your Controls tab.  
4. Add a small `profile.js` (if you want to keep logic separate) or embed the async functions directly in `widgets.js`.

This way `index.html` stays untouched ⭐  
The main page loader builds it dynamically from `dash.json` as it already does for sliders/inputs/buttons.

---

## 1️⃣ Add widget code (update `widgets.js`)
Append this near existing widget renderers:

```js
/* ---- Profile Control Widget ---- */
WidgetTypes.profile_control = function renderProfileControl(parent, cfg) {
  const card = document.createElement("div");
  card.className = "card";
  card.innerHTML = `
    <h3>${cfg.title || "Profile Control"}</h3>
    <div class="form-row"><label>Name</label>
      <input id="profile-name" type="text" placeholder="default"></div>
    <div class="form-row"><label>Build</label>
      <select id="profile-build">
        <option value="hatteras">Hatteras</option>
        <option value="warwick">Warwick</option>
        <option value="factory">Factory</option>
      </select></div>
    <div class="form-row"><label>Version</label>
      <input id="profile-version" value="0.3.34"></div>
    <div class="form-row"><label>Target</label>
      <input id="profile-target" placeholder="optional"></div>

    <div class="btn-row">
      <button id="save-profile">💾 Save</button>
      <button id="select-profile">✅ Active</button>
      <button id="clone-profile">🧬 Clone</button>
    </div>

    <div class="form-row">
      <label>Active:</label> <span id="active-profile">(…)</span>
    </div>
    <div class="form-row">
      <label>Profiles:</label> <select id="profile-list"></select>
    </div>
    <p id="profile-msg" class="status"></p>
  `;
  parent.appendChild(card);

  // --- helper functions (can move to utils.js if reused) ---
  async function getJSON(url) { return (await fetch(url)).json(); }
  async function postJSON(url, data) {
    return (await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(data)
    })).json();
  }

  const els = {
    name: card.querySelector("#profile-name"),
    build: card.querySelector("#profile-build"),
    version: card.querySelector("#profile-version"),
    target: card.querySelector("#profile-target"),
    list: card.querySelector("#profile-list"),
    active: card.querySelector("#active-profile"),
    msg: card.querySelector("#profile-msg")
  };

  async function refreshList() {
    els.list.innerHTML = "";
    try {
      const profiles = await getJSON("/api/profiles");
      profiles.forEach(p => {
        const opt = document.createElement("option");
        opt.value = p.name;
        opt.textContent = `${p.name} (${p.build}, ${p.version})`;
        els.list.appendChild(opt);
      });
    } catch {}
  }
  async function refreshActive() {
    try { els.active.textContent = (await getJSON("/api/active_profile")).name; }
    catch { els.active.textContent = "(none)"; }
  }

  async function save() {
    const body = {
      name: els.name.value || "default",
      build: els.build.value,
      version: els.version.value,
      target: els.target.value || null
    };
    await postJSON("/api/profiles", body);
    els.msg.textContent = "Profile saved";
    refreshList();
    refreshActive();
  }
  async function select() {
    const name = els.name.value || els.list.value;
    const r = await postJSON("/api/profiles/select", { name });
    els.msg.textContent = `Active: ${r.active}`;
    refreshActive();
  }
  async function clone() {
    const src = els.name.value || els.list.value;
    const newName = prompt("New profile name?");
    if (!newName) return;
    await postJSON("/api/profiles/clone", {
      source_name: src,
      new_name: newName,
      new_version: els.version.value,
      new_target: els.target.value
    });
    els.msg.textContent = `Cloned → ${newName}`;
    refreshList();
  }

  card.querySelector("#save-profile").onclick = save;
  card.querySelector("#select-profile").onclick = select;
  card.querySelector("#clone-profile").onclick = clone;

  refreshList();
  refreshActive();
};
```

That’s about 60 lines—fits with your other widget styles and uses the same `WidgetTypes` registration pattern you already have.

---

## 2️⃣ Style (in `static/css/styles.css`)
Optional—the same simple rules:

```css
.card .form-row { margin-bottom: 6px; }
.card .form-row label { display:inline-block; width:100px; }
.card .btn-row button { margin-right:6px; }
.card .status { margin-top:4px; font-size:0.9em; color:#4caf50; }
```

---

## 3️⃣ dash.json snippet

Add this card at the top (or any slot) of your **controls tab** in `dash.json`:

```json
{
  "type": "profile_control",
  "title": "Profile Control"
}
```

Example inserted into the existing structure:

```json
{
  "controls": {
    "cards": [
      { "type": "profile_control", "title": "Profiles" },
      { "type": "slider", "id": "gain", "label": "Gain", "min": 0, "max": 100 },
      { "type": "input", "id": "phase", "label": "Phase (deg)", "default": 90 }
    ]
  }
}
```

When `app.js` iterates through `dash.controls.cards`, it will encounter `type:"profile_control"` and call your new `WidgetTypes.profile_control()` automatically.

---

✅ **Result**

- You still just open `index.html` → front‑end builds tabs dynamically.  
- Nothing hard‑coded; the new control is data‑driven from `dash.json`.  
- It automatically talks to the profile endpoints proxied by `main.py`.

---

Would you like me to include a **small modal dialog clone‑form** (for version/target edits when cloning) next, instead of the simple `prompt()`?


============================================
Here's the complete `widgets.js` with everything integrated cleanly:

```js
// widgets.js
// Widget rendering and management for RBMS Dashboard

// ------------------------------------------------------------------
// Canvas drawing utilities
// ------------------------------------------------------------------
function drawLine(canvas, points, yMin=0, yMax=1, color='#9bd') {
  const dpr = window.devicePixelRatio || 1;
  const w = canvas.clientWidth, h = canvas.clientHeight;
  if (!w || !h) return;
  canvas.width = Math.floor(w*dpr); canvas.height = Math.floor(h*dpr);
  const ctx = canvas.getContext('2d'); ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,w,h);
  if (!points || points.length < 2) return;
  let minT = points[0][0], maxT = points[points.length-1][0];
  if (maxT <= minT) maxT = minT + 1;
  const sx = t => (t-minT)/(maxT-minT) * (w-16) + 8;
  const sy = v => h-8 - (v-yMin)/(yMax-yMin) * (h-24);
  ctx.strokeStyle = color; ctx.lineWidth = 2;
  ctx.beginPath();
  for (let i=0;i<points.length;i++){
    const [t,v] = points[i];
    const x=sx(t), y=sy(v);
    if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  }
  ctx.stroke();
  ctx.strokeStyle = '#2a3b5c'; ctx.lineWidth = 1;
  ctx.strokeRect(0.5,0.5,w-1,h-1);
}

function drawDial(canvas, value, min=0, max=100, label='') {
  const dpr = window.devicePixelRatio||1;
  const w=canvas.clientWidth, h=canvas.clientHeight;
  if (!w || !h) return;
  canvas.width=Math.floor(w*dpr); canvas.height=Math.floor(h*dpr);
  const ctx=canvas.getContext('2d'); ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,w,h);
  const cx=w/2, cy=h*0.75, R=Math.min(w,h)*0.4;
  const a0=Math.PI, a1=0;
  ctx.strokeStyle='#2a3b5c'; ctx.lineWidth=8;
  ctx.beginPath(); ctx.arc(cx,cy,R,a0,a1); ctx.stroke();
  const p=(Math.max(min,Math.min(max,value))-min)/(max-min || 1);
  const ang=a0 + (a1-a0)*p;
  ctx.strokeStyle='#6bd'; ctx.beginPath(); ctx.arc(cx,cy,R,a0,ang); ctx.stroke();
  ctx.fillStyle='#e8eef6'; ctx.font='bold 14px system-ui';
  ctx.textAlign='center'; ctx.fillText(label||'', cx, cy-R-4);
  ctx.font='600 16px system-ui'; ctx.fillText(String((+value).toFixed(2)), cx, cy+12);
}

// ------------------------------------------------------------------
// Base Widget class
// ------------------------------------------------------------------
class BaseWidget {
  constructor(node, cfg){ 
    this.node=node; 
    this.cfg=cfg; 
    this.body=null; 
    this.head=null; 
    this.initFrame(); 
  }
  initFrame(){
    this.node.className='widget';
    const head=document.createElement('div'); 
    head.className='whead';
    const ttl=document.createElement('div');
    ttl.className='wtitle';
    ttl.textContent=this.cfg.title||this.cfg.id;
    const tools=document.createElement('div');
    tools.className='wtools';
    head.appendChild(ttl);
    head.appendChild(tools);
    const body=document.createElement('div');
    body.className='wbody';
    this.node.appendChild(head);
    this.node.appendChild(body);
    this.body = body; 
    this.head = head; 
    this.tools = tools;
  }
  onShow(){} 
  onHide(){}
}

// ------------------------------------------------------------------
// TableWidget - displays metrics in a table with column selection
// ------------------------------------------------------------------
class TableWidget extends BaseWidget {
  constructor(node,cfg){
    super(node,cfg);
    this.columns = [
      {id:'name',      label:'name',      get:m=>m.name||''},
      {id:'host',      label:'host',      get:m=>m.host||''},
      {id:'port',      label:'port',      get:m=>m.port||''},
      {id:'tx_msgs',   label:'tx_msgs',   get:m=>m.tx_msgs||0},
      {id:'rx_msgs',   label:'rx_msgs',   get:m=>m.rx_msgs||0},
      {id:'tx_bytes',  label:'tx_bytes',  get:m=>m.tx_bytes||0},
      {id:'rx_bytes',  label:'rx_bytes',  get:m=>m.rx_bytes||0},
      {id:'ema_mbps',  label:'mbps (ema)',get:m=>(m.ema_mbps||0).toFixed(3)},
      {id:'ewma_rtt_s',label:'rtt_ewma_ms',get:m=>((m.ewma_rtt_s||0)*1000).toFixed(2)},
    ];
    const defaultCompact = ['name','port','tx_msgs','rx_msgs','ema_mbps','ewma_rtt_s'];
    this.storeKey = 'ui2.table.visible';
    this.visible = this.loadVisible() || defaultCompact.slice();
    this.buildTools(defaultCompact);
    this.tbl=null; this.tbody=null;
  }
  loadVisible(){ 
    try { return JSON.parse(localStorage.getItem(this.storeKey)||'null'); } 
    catch(_) { return null; } 
  }
  saveVisible(){ 
    localStorage.setItem(this.storeKey, JSON.stringify(this.visible)); 
  }
  buildTools(defaultCompact){
    const btn = document.createElement('button'); btn.className='wbtn'; btn.textContent='Columns';
    const menu = document.createElement('div'); menu.className='menu';
    const h4 = document.createElement('h4'); h4.textContent='Show columns'; menu.appendChild(h4);
    const colset = document.createElement('div');
    for (const c of this.columns){
      const lab = document.createElement('label');
      const cb = document.createElement('input'); cb.type='checkbox'; cb.checked = this.visible.includes(c.id);
      cb.onchange = ()=>{
        const i=this.visible.indexOf(c.id);
        if (cb.checked){ if (i<0) this.visible.push(c.id); }
        else { if (i>=0) this.visible.splice(i,1); }
        this.saveVisible(); this.rebuildHeader(); this.renderRows(this.lastData||[]);
      };
      lab.appendChild(cb); lab.appendChild(document.createTextNode(c.label));
      colset.appendChild(lab);
    }
    menu.appendChild(colset);
    const row = document.createElement('div'); row.className='row';
    const compactBtn = document.createElement('button'); compactBtn.className='wbtn'; compactBtn.textContent='Compact';
    compactBtn.onclick=()=>{
      this.visible = defaultCompact.slice(); this.saveVisible();
      for (let i=0;i<colset.children.length;i++){
        const id=this.columns[i].id;
        colset.children[i].querySelector('input').checked = this.visible.includes(id);
      }
      this.rebuildHeader(); this.renderRows(this.lastData||[]);
    };
    const allBtn = document.createElement('button'); allBtn.className='wbtn'; allBtn.textContent='All';
    allBtn.onclick=()=>{
      this.visible = this.columns.map(c=>c.id); this.saveVisible();
      for (let i=0;i<colset.children.length;i++) colset.children[i].querySelector('input').checked = true;
      this.rebuildHeader(); this.renderRows(this.lastData||[]);
    };
    row.appendChild(compactBtn); row.appendChild(allBtn);
    menu.appendChild(row);
    btn.onclick = (e)=>{ e.stopPropagation(); menu.classList.toggle('show'); };
    document.addEventListener('click', ()=>menu.classList.remove('show'));
    this.tools.appendChild(btn);
    this.head.appendChild(menu);
  }
  ensureTable(){
    if (this.tbl) return;
    this.tbl=document.createElement('table');
    this.thead=document.createElement('thead');
    this.tbody=document.createElement('tbody');
    this.tbl.appendChild(this.thead); this.tbl.appendChild(this.tbody);
    this.body.innerHTML=''; this.body.appendChild(this.tbl);
    this.rebuildHeader();
  }
  rebuildHeader(){
    this.thead.innerHTML='';
    const tr=document.createElement('tr');
    for (const c of this.columns) if (this.visible.includes(c.id)){
      const th=document.createElement('th'); th.textContent=c.label; tr.appendChild(th);
    }
    this.thead.appendChild(tr);
  }
  renderRows(rows){
    this.lastData = rows;
    if (!this.tbody) return;
    this.tbody.innerHTML='';
    for (const m of rows){
      const tr=document.createElement('tr');
      let html='';
      for (const c of this.columns) if (this.visible.includes(c.id)){
        html += `<td>${c.get(m)}</td>`;
      }
      tr.innerHTML = html;
      this.tbody.appendChild(tr);
    }
  }
  async tick(){
    const a = await getJSON('/metrics');
    this.ensureTable();
    this.renderRows(a);
  }
}

// ------------------------------------------------------------------
// LineWidget - displays time-series line chart
// ------------------------------------------------------------------
class LineWidget extends BaseWidget {
  constructor(node, cfg){ 
    super(node,cfg); 
    this.canvas=document.createElement('canvas'); 
    this.body.appendChild(this.canvas); 
    this.last=[]; 
  }
  async tick(seriesCache){
    const name = (this.cfg.series&&this.cfg.series[0]&&this.cfg.series[0].name) || '';
    const pts = (seriesCache && seriesCache[name]) || [];
    this.last = pts;
    let ymin=0, ymax=1;
    for (const p of pts){ ymin=Math.min(ymin,p[1]); ymax=Math.max(ymax,p[1]); }
    if (ymax<=ymin) { ymax=ymin+1; }
    drawLine(this.canvas, pts, ymin, ymax);
  }
}

// ------------------------------------------------------------------
// DialWidget - displays circular gauge
// ------------------------------------------------------------------
class DialWidget extends BaseWidget {
  constructor(node, cfg){ 
    super(node,cfg); 
    this.canvas=document.createElement('canvas'); 
    this.body.appendChild(this.canvas); 
  }
  async tick(seriesCache){
    const name = (this.cfg.series&&this.cfg.series[0]&&this.cfg.series[0].name) || '';
    const pts = (seriesCache && seriesCache[name]) || [];
    const v = pts.length ? pts[pts.length-1][1] : 0;
    const min = (this.cfg.opts&&this.cfg.opts.min)||0;
    const max = (this.cfg.opts&&this.cfg.opts.max)||100;
    drawDial(this.canvas, v, min, max, this.cfg.title||name);
  }
}

// ------------------------------------------------------------------
// VarTableWidget - displays and edits variables
// ------------------------------------------------------------------
class VarTableWidget extends BaseWidget {
  constructor(node, cfg){
    super(node,cfg);
    this.defaults = {}; // Store defaults for reset
    this.buildUI();
  }
  
  buildUI(){
    // Global action buttons in the widget header tools
    const refreshBtn = document.createElement('button');
    refreshBtn.className = 'wbtn';
    refreshBtn.textContent = 'Refresh All';
    refreshBtn.onclick = () => this.tick();
    
    const sendAllBtn = document.createElement('button');
    sendAllBtn.className = 'wbtn';
    sendAllBtn.textContent = 'Send All';
    sendAllBtn.onclick = () => this.sendAll();
    
    const resetBtn = document.createElement('button');
    resetBtn.className = 'wbtn';
    resetBtn.textContent = 'Reset';
    resetBtn.onclick = () => this.reset();
    
    this.tools.appendChild(refreshBtn);
    this.tools.appendChild(sendAllBtn);
    this.tools.appendChild(resetBtn);
    
    // Table structure
    this.tbl = document.createElement('table');
    const thead = document.createElement('thead');
    const tr = document.createElement('tr');
    ['Name', 'New Value', 'Current Value', 'Action'].forEach(h => {
      const th = document.createElement('th');
      th.textContent = h;
      tr.appendChild(th);
    });
    thead.appendChild(tr);
    this.tbl.appendChild(thead);
    
    this.tbody = document.createElement('tbody');
    this.tbl.appendChild(this.tbody);
    this.body.innerHTML = '';
    this.body.appendChild(this.tbl);
  }
  
  async tick(){
    const names = (this.cfg.names || []).join(',');
    const rack  = (this.cfg.rack ?? 0);
    if (!names) return;
  
    const rsp = await getJSON(`/vars?names=${encodeURIComponent(names)}&rack=${rack}`);
  
    // If the table already exists, just refresh current-value cells
    if (this.tbody && this.tbody.children.length) {
      for (const nm of (this.cfg.names || [])) {
        const td = document.getElementById(`var_curr_${nm}`);
        if (td) {
          const v = rsp[nm];
          td.textContent = Array.isArray(v) ? v.join(', ') : (v == null ? '—' : v);
        }
      }
      return;
    }
  
    // First-time build (no rows yet)
    this.tbody.innerHTML = '';
    for (const nm of (this.cfg.names || [])) {
      const currVal = rsp[nm];
      const displayVal = Array.isArray(currVal) ? currVal.join(', ') :
                         (currVal == null ? '—' : currVal);
      if (!(nm in this.defaults)) this.defaults[nm] = currVal;
  
      const tr = document.createElement('tr');
  
      // Name
      const tdName = document.createElement('td');
      tdName.textContent = nm;
      tr.appendChild(tdName);
  
      // New value input
      const tdNew = document.createElement('td');
      const input = document.createElement('input');
      input.type = 'text';
      input.className = 'var-input';
      input.id = `var_new_${nm}`;
      input.placeholder = 'Enter new value';
      input.style.cssText = 'width:100%;background:#0b1126;color:#e8eef6;border:1px solid #1e2a43;border-radius:4px;padding:4px 6px;font-size:12px';
      tdNew.appendChild(input);
      tr.appendChild(tdNew);
  
      // Current value
      const tdCurr = document.createElement('td');
      tdCurr.id = `var_curr_${nm}`;
      tdCurr.textContent = displayVal;
      tr.appendChild(tdCurr);
  
      // Action button
      const tdAction = document.createElement('td');
      const sendBtn = document.createElement('button');
      sendBtn.className = 'wbtn';
      sendBtn.textContent = 'Send';
      sendBtn.onclick = () => this.sendOne(nm);
      tdAction.appendChild(sendBtn);
      tr.appendChild(tdAction);
  
      this.tbody.appendChild(tr);
    }
  }
  
  async sendOne(name){
    const input = document.getElementById(`var_new_${name}`);
    if (!input || !input.value.trim()) {
      alert(`No new value entered for ${name}`);
      return;
    }
    
    const newValue = input.value.trim();
    
    try {
      const payload = { [name]: newValue };
      await postJSON('/vars', payload);
      await this.tick();
      input.value = '';
    } catch (e) {
      alert(`Failed to send ${name}: ${e.message}`);
    }
  }
  
  async sendAll(){
    const updates = {};
    let hasUpdates = false;
    
    for (const nm of (this.cfg.names || [])) {
      const input = document.getElementById(`var_new_${nm}`);
      if (input && input.value.trim()) {
        updates[nm] = input.value.trim();
        hasUpdates = true;
      }
    }
    
    if (!hasUpdates) {
      alert('No new values to send');
      return;
    }
    
    try {
      await postJSON('/vars', updates);
      await this.tick();
      
      // Clear all inputs after successful send
      for (const nm of (this.cfg.names || [])) {
        const input = document.getElementById(`var_new_${nm}`);
        if (input) input.value = '';
      }
    } catch (e) {
      alert(`Failed to send all: ${e.message}`);
    }
  }
  
  reset(){
    // Reset all "new value" inputs to their defaults
    for (const nm of (this.cfg.names || [])) {
      const input = document.getElementById(`var_new_${nm}`);
      if (input) {
        const defaultVal = this.defaults[nm];
        input.value = Array.isArray(defaultVal) ? defaultVal.join(', ') : (defaultVal == null ? '' : String(defaultVal));
      }
    }
  }
}

// ------------------------------------------------------------------
// Widget registry
// ------------------------------------------------------------------
const WidgetTypes = {
  table: (node,cfg)=>new TableWidget(node,cfg),
  line:  (node,cfg)=>new LineWidget(node,cfg),
  dial:  (node,cfg)=>new DialWidget(node,cfg),
  vartable: (node,cfg)=>new VarTableWidget(node,cfg)
};

// ------------------------------------------------------------------
// Profile Control Widget
// ------------------------------------------------------------------
WidgetTypes.profile_control = function renderProfileControl(parent, cfg) {
  const card = document.createElement("div");
  card.className = "widget";
  
  // Build frame similar to BaseWidget
  const head = document.createElement('div'); 
  head.className = 'whead';
  const ttl = document.createElement('div'); 
  ttl.className = 'wtitle'; 
  ttl.textContent = cfg.title || "Profile Control";
  const tools = document.createElement('div'); 
  tools.className = 'wtools';
  head.appendChild(ttl); 
  head.appendChild(tools);
  
  const body = document.createElement('div'); 
  body.className = 'wbody';
  
  body.innerHTML = `
    <div class="form-row">
      <label>Name</label>
      <input id="profile-name" type="text" placeholder="default">
    </div>
    <div class="form-row">
      <label>Build</label>
      <select id="profile-build">
        <option value="hatteras">Hatteras</option>
        <option value="warwick">Warwick</option>
        <option value="factory">Factory</option>
      </select>
    </div>
    <div class="form-row">
      <label>Version</label>
      <input id="profile-version" value="0.3.34">
    </div>
    <div class="form-row">
      <label>Target</label>
      <input id="profile-target" placeholder="optional">
    </div>

    <div class="btn-row">
      <button class="wbtn" id="save-profile">💾 Save</button>
      <button class="wbtn" id="select-profile">✅ Active</button>
      <button class="wbtn" id="clone-profile">🧬 Clone</button>
    </div>

    <div class="form-row">
      <label>Active:</label> <span id="active-profile">(…)</span>
    </div>
    <div class="form-row">
      <label>Profiles:</label> <select id="profile-list"></select>
    </div>
    <p id="profile-msg" class="status"></p>
  `;
  
  card.appendChild(head);
  card.appendChild(body);
  parent.appendChild(card);

  // Helper functions
  async function getJSON(url) { return (await fetch(url)).json(); }
  async function postJSON(url, data) {
    return (await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(data)
    })).json();
  }

  const els = {
    name: body.querySelector("#profile-name"),
    build: body.querySelector("#profile-build"),
    version: body.querySelector("#profile-version"),
    target: body.querySelector("#profile-target"),
    list: body.querySelector("#profile-list"),
    active: body.querySelector("#active-profile"),
    msg: body.querySelector("#profile-msg")
  };

  async function refreshList() {
    els.list.innerHTML = "";
    try {
      const profiles = await getJSON("/api/profiles");
      profiles.forEach(p => {
        const opt = document.createElement("option");
        opt.value = p.name;
        opt.textContent = `${p.name} (${p.build}, ${p.version})`;
        els.list.appendChild(opt);
      });
    } catch(e) {
      console.error("Failed to load profiles:", e);
    }
  }
  
  async function refreshActive() {
    try { 
      const ap = await getJSON("/api/active_profile");
      els.active.textContent = ap.name;
    } catch(e) { 
      els.active.textContent = "(none)"; 
    }
  }

  async function save() {
    const bodyData = {
      name: els.name.value || "default",
      build: els.build.value,
      version: els.version.value,
      target: els.target.value || null
    };
    try {
      await postJSON("/api/profiles", bodyData);
      els.msg.textContent = "Profile saved ✓";
      setTimeout(() => els.msg.textContent = "", 3000);
      refreshList();
      refreshActive();
    } catch(e) {
      els.msg.textContent = "Save failed: " + e.message;
    }
  }

  async function select() {
    const name = els.name.value || els.list.value;
    try {
      const r = await postJSON("/api/profiles/select", { name });
      els.msg.textContent = `Active: ${r.active} ✓`;
      setTimeout(() => els.msg.textContent = "", 3000);
      refreshActive();
    } catch(e) {
      els.msg.textContent = "Select failed: " + e.message;
    }
  }

  async function clone() {
    const src = els.name.value || els.list.value;
    const newName = prompt("New profile name?");
    if (!newName) return;
    try {
      await postJSON("/api/profiles/clone", {
        source_name: src,
        new_name: newName,
        new_version: els.version.value,
        new_target: els.target.value
      });
      els.msg.textContent = `Cloned → ${newName} ✓`;
      setTimeout(() => els.msg.textContent = "", 3000);
      refreshList();
    } catch(e) {
      els.msg.textContent = "Clone failed: " + e.message;
    }
  }

  body.querySelector("#save-profile").onclick = save;
  body.querySelector("#select-profile").onclick = select;
  body.querySelector("#clone-profile").onclick = clone;

  refreshList();
  refreshActive();
};
```

---

### ✅ What changed

1. **Moved `WidgetTypes` definition before the profile control** so it exists when we attach to it.
2. **Profile control now builds a proper widget frame** (with `whead`, `wbody`, `wtitle`, `wtools`) matching your other widgets.
3. **All helper functions scoped inside** the profile control factory.
4. **Error handling added** for fetch failures with user feedback.
5. **Removed duplicate/conflicting `const WidgetTypes` declarations**.

Now you can add this to your `dash.json`:

```json
{
  "type": "profile_control",
  "title": "Build Profiles"
}
```

And it will render cleanly alongside your other widgets. 🎉


===============================================
Perfect — love that approach 👌  
Focus on locking in stability before adding layers.  

Here’s how we can lock in and test **what you have right now** (the current frontend + API setup):

---

### ✅ Step 1: Run your stack

```bash
# Start your data server first (the one exposing /vars and /metrics)
python data_server.py --port 8090

# Then start the main dashboard frontend
python main.py --port 8080 --data-port 8090
```

Confirm both print startup messages.  
Then open:  
**http://localhost:8080**

---

### ✅ Step 2: Verify dashboard UI loads correctly
You should see:
- All widgets (tables, dials, variable table, and **Profile Control**) visible.
- The knob drawing correctly.
- Inputs in the VarTable staying persistent while polling.

If you get a blank page:
- Check **browser console (F12 → Console)** for JavaScript errors.
- Ensure `dash.json` is in your `static/` directory.
- Verify `widgets.js` loads (Network tab → watch for 404 errors).

---

### ✅ Step 3: Exercise API endpoints with curl

Use this short test matrix:
```bash
curl -s localhost:8090/metrics | jq
curl -s localhost:8090/vars?names=test_var -G
curl -s localhost:8090/api/profiles | jq
curl -s localhost:8090/api/active_profile | jq

# Create and set a new profile
curl -s -X POST localhost:8090/api/profiles -H "Content-Type: application/json" \
  -d '{"name":"demo_profile","build":"hatteras","version":"0.3.34"}' | jq

curl -s -X POST localhost:8090/api/profiles/select -H "Content-Type: application/json" \
  -d '{"name":"demo_profile"}' | jq
```

---

### ✅ Step 4: Confirm frontend sync
After running the profile POSTs above, refresh your dashboard — your **Profile Control** should now list and mark your new `demo_profile` as active.

---

If you run through those and note:
- No console errors,
- Profile list loads and updates,
- Var table refreshes periodically without clearing inputs,
- Line and dial widgets show mock data,

Then you’re solid — your offline stack is ready for stable use and later deployment expansion.

---

You want me to drop in a **quick smoke test checklist** (a single bash script that runs these curl calls and validates server responses)? It’s nice for confirming your endpoints after restarts.

=================================================