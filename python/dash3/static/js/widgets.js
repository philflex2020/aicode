

// widgets.js
// Widget rendering and management for RBMS Dashboard

// ------------------------------------------------------------------
// Canvas drawing utilities
// ------------------------------------------------------------------
function drawLine(canvas, points, yMin=0, yMax=1, color='#9bd') {
  const dpr = window.devicePixelRatio || 1;
  //const 
  w = canvas.clientWidth, h = canvas.clientHeight;
  if (!w || !h) return;
  //console.log(" line orig height ", h); //h -= 40;
  //console.log(" line new height ", h);
  canvas.width = Math.floor(w*dpr);
  canvas.height = Math.floor(h*dpr);
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
  //ctx.strokeRect(0.5,0.5,w,h);
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
    const head=document.createElement('div'); head.className='whead';
    const ttl=document.createElement('div'); ttl.className='wtitle'; ttl.textContent=this.cfg.title||this.cfg.id;
    const tools=document.createElement('div'); tools.className='wtools';
    head.appendChild(ttl); head.appendChild(tools);
    const body=document.createElement('div'); body.className='wbody';
    this.node.appendChild(head); this.node.appendChild(body);
    this.body = body; this.head = head; this.tools = tools;
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
  
    const rsp = await getJSON(
      withProfileURL(`/vars?names=${encodeURIComponent(names)}&rack=${rack}`)
    );
//    const rsp = await getJSON(`/vars?names=${encodeURIComponent(names)}&rack=${rack}`);
  
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
// IndexGridWidget - generic array metrics viewer (cells, modules, etc.)
// Displays items in blocks, e.g., 100 per block
// seriesCache expected shape: { metricName: [v0, v1, v2, ...] }
// ------------------------------------------------------------------
// class IndexGridWidget extends BaseWidget {
//   constructor(node, cfg) {
//     super(node, cfg);

//     const opts = cfg.opts || {};
//     this.totalItems    = opts.totalItems    || 480;    // e.g., number of cells
//     this.itemsPerBlock = opts.itemsPerBlock || 100;    // e.g., 100 per block
//     this.indexLabel    = opts.indexLabel    || "Index";

//     // Metrics/series configuration
//     // Each item: { name: "cell_volts", label: "Volts", unit: "V" }
//     this.metrics = (cfg.series || []).map(s => ({
//       name:  s.name,
//       label: s.label || s.name,
//       unit:  s.unit  || ""
//     }));

//     // Visible metrics (for selector menu)
//     this.storeKey = `ui2.indexgrid.visible.${cfg.id || "default"}`;
//     this.visible  = this.loadVisible() || this.metrics.map(m => m.name);

//     this.buildTools();
//     this.container = null;
//     this.buildLayout();
//   }

//   loadVisible() {
//     try {
//       return JSON.parse(localStorage.getItem(this.storeKey) || "null");
//     } catch (_) {
//       return null;
//     }
//   }

//   saveVisible() {
//     localStorage.setItem(this.storeKey, JSON.stringify(this.visible));
//   }

//   buildTools() {
//     // "Metrics" button and menu (similar to TableWidget)
//     const btn = document.createElement("button");
//     btn.className = "wbtn";
//     btn.textContent = "Metrics";

//     const menu = document.createElement("div");
//     menu.className = "menu";

//     const h4 = document.createElement("h4");
//     h4.textContent = "Show metrics";
//     menu.appendChild(h4);

//     const metricSet = document.createElement("div");
//     for (const m of this.metrics) {
//       const lab = document.createElement("label");
//       const cb  = document.createElement("input");
//       cb.type    = "checkbox";
//       cb.checked = this.visible.includes(m.name);
//       cb.onchange = () => {
//         const i = this.visible.indexOf(m.name);
//         if (cb.checked) {
//           if (i < 0) this.visible.push(m.name);
//         } else {
//           if (i >= 0) this.visible.splice(i, 1);
//         }
//         this.saveVisible();
//         if (this.lastData) this.renderBlocks(this.lastData);
//       };
//       lab.appendChild(cb);
//       lab.appendChild(document.createTextNode(m.label));
//       metricSet.appendChild(lab);
//     }
//     menu.appendChild(metricSet);

//     const row = document.createElement("div");
//     row.className = "row";

//     const allBtn = document.createElement("button");
//     allBtn.className = "wbtn";
//     allBtn.textContent = "All";
//     allBtn.onclick = () => {
//       this.visible = this.metrics.map(m => m.name);
//       this.saveVisible();
//       const labels = metricSet.children;
//       for (let i = 0; i < labels.length; i++) {
//         labels[i].querySelector("input").checked = true;
//       }
//       if (this.lastData) this.renderBlocks(this.lastData);
//     };

//     const noneBtn = document.createElement("button");
//     noneBtn.className = "wbtn";
//     noneBtn.textContent = "None";
//     noneBtn.onclick = () => {
//       this.visible = [];
//       this.saveVisible();
//       const labels = metricSet.children;
//       for (let i = 0; i < labels.length; i++) {
//         labels[i].querySelector("input").checked = false;
//       }
//       if (this.lastData) this.renderBlocks(this.lastData);
//     };

//     row.appendChild(allBtn);
//     row.appendChild(noneBtn);
//     menu.appendChild(row);

//     btn.onclick = (e) => {
//       e.stopPropagation();
//       menu.classList.toggle("show");
//     };
//     document.addEventListener("click", () => menu.classList.remove("show"));

//     this.tools.appendChild(btn);
//     this.head.appendChild(menu);
//   }

//   buildLayout() {
//     this.container = document.createElement("div");
//     this.container.className = "indexgrid-container";
//     this.body.innerHTML = "";
//     this.body.appendChild(this.container);
//   }

//   // seriesCache: e.g., { cell_volts: [[t, [v0,v1,...]]], cell_temp: [[t, [...]]], ... }
// async tick(seriesCache) {
//   if (!seriesCache) return;

//   const data = {};
//   for (const m of this.metrics) {
//     const pts = seriesCache[m.name] || [];
//     if (!pts.length) {
//       // No data for this metric
//       data[m.name] = new Array(this.totalItems).fill(null);
//       continue;
//     }

//     // Extract the last sample: [timestamp, arrayOfValues]
//     const last = pts[pts.length - 1][1];
//     let arr;

//     if (Array.isArray(last)) {
//       // Expected case: backend sent [[t, [v0, v1, ...]]]
//       arr = last.slice();
//     } else {
//       // Fallback: if backend sent scalar time-series instead
//       arr = pts.map(p => p[1]);
//     }

//     // Pad or truncate to totalItems
//     const vals = new Array(this.totalItems);
//     for (let i = 0; i < this.totalItems; i++) {
//       vals[i] = (i < arr.length) ? arr[i] : null;
//     }
//     data[m.name] = vals;
//   }

//   this.lastData = data;
//   this.renderBlocks(data);
// }

// ------------------------------------------------------------------
// IndexGridWidget - generic array metrics viewer (cells, modules, etc.)
// seriesCache expected shape: { metricName: [[t, [v0, v1, v2, ...]]] }
// ------------------------------------------------------------------
class IndexGridWidget extends BaseWidget {
  constructor(node, cfg) {
    super(node, cfg);

    const opts = cfg.opts || {};
    this.totalItems    = opts.totalItems    || 480;
    this.itemsPerBlock = opts.itemsPerBlock || 100;
    this.indexLabel    = opts.indexLabel    || "Index";

    this.metrics = (cfg.series || []).map(s => ({
      name:  s.name,
      label: s.label || s.name,
      unit:  s.unit  || ""
    }));

    this.storeKey = `ui2.indexgrid.visible.${cfg.id || "default"}`;
    this.visible  = this.loadVisible() || this.metrics.map(m => m.name);

    this.buildTools();
    // Don't create a separate container; renderBlocks will build directly in this.body
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

  // seriesCache: e.g., { cell_volts: [[t, [v0,v1,...]]], cell_temp: [[t, [...]]], ... }
  async tick(seriesCache) {
    console.log("IndexGrid tick seriesCache:", seriesCache);  // DEBUG
    if (!seriesCache) return;

    const data = {};
    for (const m of this.metrics) {
      const pts = seriesCache[m.name] || [];
      if (!pts.length) {
        data[m.name] = new Array(this.totalItems).fill(null);
        continue;
      }

      const lastSample = pts[pts.length - 1];
      const payload = lastSample[1];

      let arr;
      if (Array.isArray(payload)) {
        // Expected: [[t, [v0, v1, ...]]]
        arr = payload.slice();
      } else {
        // Fallback: scalar series
        arr = pts.map(p => p[1]);
      }

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
    // Ensure we have a place to store per-block scroll positions
    if (!this.blockScroll) this.blockScroll = {};
  
    // Clear existing content
    this.body.innerHTML = "";
  
    const root = document.createElement("div");
    root.className = "ixg-root";
  
    const blocksContainer = document.createElement("div");
    blocksContainer.className = "ixg-blocks-container";
  
    const totalBlocks = Math.ceil(this.totalItems / this.itemsPerBlock);
  
    for (let b = 0; b < totalBlocks; b++) {
      const start = b * this.itemsPerBlock;
      const end   = Math.min((b + 1) * this.itemsPerBlock, this.totalItems);
      const count = end - start;
  
      // Outer block "card"
      const block = document.createElement("div");
      block.className = "ixg-block";
  
      const blockTitle = document.createElement("div");
      blockTitle.className = "ixg-block-title";
      blockTitle.textContent = `Cells ${start + 1}–${end}`;
      block.appendChild(blockTitle);
  
      // Two-column layout in the block
      const layout = document.createElement("div");
      layout.className = "ixg-layout";
  
      const leftCol = document.createElement("div");
      leftCol.className = "ixg-legend";
  
      const rightCol = document.createElement("div");
      rightCol.className = "ixg-scroll";
  
      // Build row data for this block
      const rows = [];
  
      // Index row (Cell #)
      const indexVals = [];
      for (let i = 0; i < count; i++) indexVals.push(start + i + 1);
      rows.push({
        label:   this.indexLabel,
        unit:    "",
        values:  indexVals,
        isIndex: true,
      });
  
      // Metric rows (sliced to this block)
      for (const m of this.metrics) {
        if (!this.visible.includes(m.name)) continue;
        const fullVals = data[m.name] || [];
        const slice = [];
        for (let i = start; i < end; i++) {
          slice.push(i < fullVals.length ? fullVals[i] : null);
        }
        rows.push({
          label:   m.label,
          unit:    m.unit || "",
          values:  slice,
          isIndex: false,
        });
      }
  
      // Create DOM rows: left legend + right values
      for (const rowData of rows) {
        // Left legend row
        const lrow = document.createElement("div");
        lrow.className = "ixg-row-left";
        const lhead = document.createElement("div");
        lhead.className = "ixg-row-head";
        lhead.textContent = rowData.unit
          ? `${rowData.label} (${rowData.unit})`
          : rowData.label;
        lrow.appendChild(lhead);
        leftCol.appendChild(lrow);
  
        // Right values row
        const rrow = document.createElement("div");
        rrow.className = "ixg-row";
        const cellsWrap = document.createElement("div");
        cellsWrap.className = "ixg-row-cells";
  
        for (let i = 0; i < rowData.values.length; i++) {
          const v = rowData.values[i];
          const cell = document.createElement("div");
          cell.className = "ixg-cell";
  
          if (v == null || Number.isNaN(v)) {
            cell.classList.add("ixg-cell-empty");
            cell.textContent = "—";
          } else if (rowData.isIndex) {
            // integer cell number
            cell.textContent = parseInt(v, 10);
          } else {
            // metric values
            cell.textContent =
              typeof v === "number" ? v.toFixed(2) : v;
          }
  
          cellsWrap.appendChild(cell);
        }
  
        rrow.appendChild(cellsWrap);
        rightCol.appendChild(rrow);
      }
  
      // Attach scroll listener ONCE per render to keep scroll pos
      rightCol.addEventListener("scroll", () => {
        this.blockScroll[b] = rightCol.scrollLeft;
      });
  
      // Restore previous scrollLeft for this block (if any)
      if (this.blockScroll[b] != null) {
        // Use requestAnimationFrame to ensure layout is done
        requestAnimationFrame(() => {
          rightCol.scrollLeft = this.blockScroll[b];
        });
      }
  
      layout.appendChild(leftCol);
      layout.appendChild(rightCol);
      block.appendChild(layout);
      blocksContainer.appendChild(block);
    }
  
    root.appendChild(blocksContainer);
    this.body.appendChild(root);
  }
  yrenderBlocks(data) {
    this.body.innerHTML = "";
  
    const root = document.createElement("div");
    root.className = "ixg-root";
  
    const blocksContainer = document.createElement("div");
    blocksContainer.className = "ixg-blocks-container";
  
    const totalBlocks = Math.ceil(this.totalItems / this.itemsPerBlock);
  
    // We keep scroll positions per block by index
    if (!this.blockScroll) this.blockScroll = {};
  
    for (let b = 0; b < totalBlocks; b++) {
      const start = b * this.itemsPerBlock;
      const end   = Math.min((b + 1) * this.itemsPerBlock, this.totalItems);
      const count = end - start;
  
      // Outer block card
      const block = document.createElement("div");
      block.className = "ixg-block";
  
      const blockTitle = document.createElement("div");
      blockTitle.className = "ixg-block-title";
      blockTitle.textContent = `Cells ${start + 1}–${end}`;
      block.appendChild(blockTitle);
  
      // Two-column layout inside block
      const layout = document.createElement("div");
      layout.className = "ixg-layout";
  
      const leftCol = document.createElement("div");
      leftCol.className = "ixg-legend";
  
      const rightCol = document.createElement("div");
      rightCol.className = "ixg-scroll";
  
      // Restore previous scroll position for this block
      if (this.blockScroll[b] != null) {
        rightCol.scrollLeft = this.blockScroll[b];
      }
  
      const rows = [];
  
      // Index row
      const indexVals = [];
      for (let i = 0; i < count; i++) indexVals.push(start + i + 1);
      rows.push({
        label: this.indexLabel,
        unit: "",
        values: indexVals,
        isIndex: true,
      });
  
      // Metric rows
      for (const m of this.metrics) {
        if (!this.visible.includes(m.name)) continue;
        const fullVals = data[m.name] || [];
        const slice = [];
        for (let i = start; i < end; i++) {
          slice.push(i < fullVals.length ? fullVals[i] : null);
        }
        rows.push({
          label: m.label,
          unit: m.unit || "",
          values: slice,
          isIndex: false,
        });
      }
  
      for (const rowData of rows) {
        // Left legend row
        const lrow = document.createElement("div");
        lrow.className = "ixg-row-left";
        const lhead = document.createElement("div");
        lhead.className = "ixg-row-head";
        lhead.textContent = rowData.unit
          ? `${rowData.label} (${rowData.unit})`
          : rowData.label;
        lrow.appendChild(lhead);
        leftCol.appendChild(lrow);
  
        // Right values row
        const rrow = document.createElement("div");
        rrow.className = "ixg-row";
        const cellsWrap = document.createElement("div");
        cellsWrap.className = "ixg-row-cells";
  
        for (let i = 0; i < rowData.values.length; i++) {
          const v = rowData.values[i];
          const cell = document.createElement("div");
          cell.className = "ixg-cell";
  
          if (v == null || Number.isNaN(v)) {
            cell.classList.add("ixg-cell-empty");
            cell.textContent = "—";
          } else if (rowData.isIndex) {
            cell.textContent = parseInt(v, 10);
          } else {
            cell.textContent =
              typeof v === "number" ? v.toFixed(2) : v;
          }
  
          cellsWrap.appendChild(cell);
        }
  
        rrow.appendChild(cellsWrap);
        rightCol.appendChild(rrow);
      }
  
      // Track scroll changes for this block
      rightCol.addEventListener("scroll", () => {
        this.blockScroll[b] = rightCol.scrollLeft;
      });
  
      layout.appendChild(leftCol);
      layout.appendChild(rightCol);
      block.appendChild(layout);
      blocksContainer.appendChild(block);
    }
  
    root.appendChild(blocksContainer);
    this.body.appendChild(root);
  }
  xrenderBlocks(data) {
    // Preserve scroll position
    let prevScrollLeft = 0;
    const existingRoot = this.body.querySelector(".ixg-root");
    if (existingRoot) {
      const existingScroll = existingRoot.querySelector(".ixg-scroll");
      if (existingScroll) prevScrollLeft = existingScroll.scrollLeft;
    }
  
    this.body.innerHTML = "";
  
    const root = document.createElement("div");
    root.className = "ixg-root";
  
    // Two-column layout: left fixed legend, right scrolling values
    const layout = document.createElement("div");
    layout.className = "ixg-layout";
  
    const leftCol = document.createElement("div");
    leftCol.className = "ixg-legend";
  
    const rightCol = document.createElement("div");
    rightCol.className = "ixg-scroll";
  
    // Build rows in parallel for alignment
    const rows = [];
  
    const indexVals = [];
    for (let i = 0; i < this.totalItems; i++) indexVals.push(i + 1);
    rows.push({
      label: this.indexLabel,
      unit: "",
      values: indexVals,
      isIndex: true,
    });
  
    for (const m of this.metrics) {
      if (!this.visible.includes(m.name)) continue;
      const vals = data[m.name] || [];
      rows.push({
        label: m.label,
        unit: m.unit || "",
        values: vals,
        isIndex: false,
      });
    }
  
    for (const rowData of rows) {
      // Left legend cell (fixed)
      const lrow = document.createElement("div");
      lrow.className = "ixg-row-left";
      const lhead = document.createElement("div");
      lhead.className = "ixg-row-head";
      lhead.textContent = rowData.unit
        ? `${rowData.label} (${rowData.unit})`
        : rowData.label;
      lrow.appendChild(lhead);
      leftCol.appendChild(lrow);
  
      // Right values cells (scrollable)
      const rrow = document.createElement("div");
      rrow.className = "ixg-row";
      const cellsWrap = document.createElement("div");
      cellsWrap.className = "ixg-row-cells";
  
      for (let i = 0; i < this.totalItems; i++) {
        const v = rowData.values[i];
        const cell = document.createElement("div");
        cell.className = "ixg-cell";
  
        if (v == null || Number.isNaN(v)) {
          cell.classList.add("ixg-cell-empty");
          cell.textContent = "—";
        } else if (rowData.isIndex) {
          // Index row: integer cell number
          cell.textContent = parseInt(v, 10);
        } else {
          cell.textContent =
            typeof v === "number" ? v.toFixed(2) : v;
        }
  
        cellsWrap.appendChild(cell);
      }
  
      rrow.appendChild(cellsWrap);
      rightCol.appendChild(rrow);
    }
  
    layout.appendChild(leftCol);
    layout.appendChild(rightCol);
    root.appendChild(layout);
    this.body.appendChild(root);
  
    // Restore scroll position
    rightCol.scrollLeft = prevScrollLeft;
  }
}

// ------------------------------------------------------------------
// Widget registry
// ------------------------------------------------------------------
const WidgetTypes = {
  table: (node,cfg)=>new TableWidget(node,cfg),
  line:  (node,cfg)=>new LineWidget(node,cfg),
  dial:  (node,cfg)=>new DialWidget(node,cfg),
  vartable: (node,cfg)=>new VarTableWidget(node,cfg),
  indexgrid:(node,cfg)=>new IndexGridWidget(node,cfg)   // 👈 new
};



// ------------------------------------------------------------------
// Profile Control Widget (with activeProfile propagation)
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

  // Helper: append profile to query if window.activeProfile is set
  function withProfileURL(baseURL) {
    const url = new URL(baseURL, window.location.origin);
    if (window.activeProfile) url.searchParams.set('profile', window.activeProfile);
    return url.toString();
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
    console.log("refresh active");
    try { 
      const ap = await getJSON("/api/active_profile");
      window.activeProfile = ap.name; // ✅ global context
      els.active.textContent = ap.name || "(none)";
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
    console.log(" select  profile name ", name);
    try {
      const r = await postJSON("/api/profiles/select", { name });
      window.activeProfile = name;                 // ✅ Store globally for polling
      window.profile_name = name;  // ✅ if you want this global as well
      els.active.textContent = name;
      els.msg.textContent = `Active: ${name} ✓`;
      setTimeout(() => els.msg.textContent = "", 3000);
      // 🔄 Update the header title without reloading
      const label = document.getElementById("profile-name-label");
      if (label) {
        label.textContent = name;
        console.log(" set profile name ", name);
      }
      // optional: immediate refresh of other widgets if global pollOnce exists
      if (typeof refreshActive === "function") refreshActive();
      if (typeof pollOnce === "function") pollOnce();

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



// // ------------------------------------------------------------------
// // Profile Control Widget
// // ------------------------------------------------------------------
// WidgetTypes.profile_control = function renderProfileControl(parent, cfg) {
//   const card = document.createElement("div");
//   card.className = "widget";
  
//   // Build frame similar to BaseWidget
//   const head = document.createElement('div'); 
//   head.className = 'whead';
//   const ttl = document.createElement('div'); 
//   ttl.className = 'wtitle'; 
//   ttl.textContent = cfg.title || "Profile Control";
//   const tools = document.createElement('div'); 
//   tools.className = 'wtools';
//   head.appendChild(ttl); 
//   head.appendChild(tools);
  
//   const body = document.createElement('div'); 
//   body.className = 'wbody';
  
//   body.innerHTML = `
//     <div class="form-row">
//       <label>Name</label>
//       <input id="profile-name" type="text" placeholder="default">
//     </div>
//     <div class="form-row">
//       <label>Build</label>
//       <select id="profile-build">
//         <option value="hatteras">Hatteras</option>
//         <option value="warwick">Warwick</option>
//         <option value="factory">Factory</option>
//       </select>
//     </div>
//     <div class="form-row">
//       <label>Version</label>
//       <input id="profile-version" value="0.3.34">
//     </div>
//     <div class="form-row">
//       <label>Target</label>
//       <input id="profile-target" placeholder="optional">
//     </div>

//     <div class="btn-row">
//       <button class="wbtn" id="save-profile">💾 Save</button>
//       <button class="wbtn" id="select-profile">✅ Active</button>
//       <button class="wbtn" id="clone-profile">🧬 Clone</button>
//     </div>

//     <div class="form-row">
//       <label>Active:</label> <span id="active-profile">(…)</span>
//     </div>
//     <div class="form-row">
//       <label>Profiles:</label> <select id="profile-list"></select>
//     </div>
//     <p id="profile-msg" class="status"></p>
//   `;
  
//   card.appendChild(head);
//   card.appendChild(body);
//   parent.appendChild(card);

//   // Helper functions
//   async function getJSON(url) { return (await fetch(url)).json(); }
//   async function postJSON(url, data) {
//     return (await fetch(url, {
//       method: "POST",
//       headers: { "Content-Type": "application/json" },
//       body: JSON.stringify(data)
//     })).json();
//   }

//   const els = {
//     name: body.querySelector("#profile-name"),
//     build: body.querySelector("#profile-build"),
//     version: body.querySelector("#profile-version"),
//     target: body.querySelector("#profile-target"),
//     list: body.querySelector("#profile-list"),
//     active: body.querySelector("#active-profile"),
//     msg: body.querySelector("#profile-msg")
//   };

//   async function refreshList() {
//     els.list.innerHTML = "";
//     try {
//       const profiles = await getJSON("/api/profiles");
//       profiles.forEach(p => {
//         const opt = document.createElement("option");
//         opt.value = p.name;
//         opt.textContent = `${p.name} (${p.build}, ${p.version})`;
//         els.list.appendChild(opt);
//       });
//     } catch(e) {
//       console.error("Failed to load profiles:", e);
//     }
//   }
  
//   async function refreshActive() {
//     try { 
//       const ap = await getJSON("/api/active_profile");
//       els.active.textContent = ap.name;
//     } catch(e) { 
//       els.active.textContent = "(none)"; 
//     }
//   }

//   async function save() {
//     const bodyData = {
//       name: els.name.value || "default",
//       build: els.build.value,
//       version: els.version.value,
//       target: els.target.value || null
//     };
//     try {
//       await postJSON("/api/profiles", bodyData);
//       els.msg.textContent = "Profile saved ✓";
//       setTimeout(() => els.msg.textContent = "", 3000);
//       refreshList();
//       refreshActive();
//     } catch(e) {
//       els.msg.textContent = "Save failed: " + e.message;
//     }
//   }

//   async function select() {
//     const name = els.name.value || els.list.value;
//     try {
//       const r = await postJSON("/api/profiles/select", { name });
//       els.msg.textContent = `Active: ${r.active} ✓`;
//       setTimeout(() => els.msg.textContent = "", 3000);
//       refreshActive();
//     } catch(e) {
//       els.msg.textContent = "Select failed: " + e.message;
//     }
//   }

//   async function clone() {
//     const src = els.name.value || els.list.value;
//     const newName = prompt("New profile name?");
//     if (!newName) return;
//     try {
//       await postJSON("/api/profiles/clone", {
//         source_name: src,
//         new_name: newName,
//         new_version: els.version.value,
//         new_target: els.target.value
//       });
//       els.msg.textContent = `Cloned → ${newName} ✓`;
//       setTimeout(() => els.msg.textContent = "", 3000);
//       refreshList();
//     } catch(e) {
//       els.msg.textContent = "Clone failed: " + e.message;
//     }
//   }

//   body.querySelector("#save-profile").onclick = save;
//   body.querySelector("#select-profile").onclick = select;
//   body.querySelector("#clone-profile").onclick = clone;

//   refreshList();
//   refreshActive();
// };