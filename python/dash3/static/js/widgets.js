

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
//
//
// ------------------------------------------------------------------
// Protections Widget 
// this is a complex widget , not sure how best to resent it.
// there are number of vats that are montitored for fault /alarm limits.
// For instance cell_voltage
// there are 3 limit levels for max cell voltage which are repeated for min cell voltage
//  //  Warning , Alert , Fault
// each level has a threshold value and a duration
// if the threshold is exceeded for the duration then the alarm is raised
// the widget shows the current value of each variable and allows the user to set the thresholds and durations
// it also shows the current status of each alarm (normal, warning, alert, fault)
// buttons allow save, recall, reset to default and deploy ( we need to add deploy logic later to other widgets)
//  not all variables hve all 3 levels 
// not all variables have max and min limits
// the widget will be presented with a list of variables and a config for exceptions to the defaults
// the system will trigger a function by name when an alarm state changes
// the named variable will have sonme text and a format associated with it
// ProtectionsWidget.js
// ------------------------------------------------------------------
// ProtectionsWidget - Alarm threshold management
// ------------------------------------------------------------------

// ------------------------------------------------------------------
// ProtectionsWidget - Alarm threshold management
// ------------------------------------------------------------------
class ProtectionsWidget extends BaseWidget {
  constructor(node, cfg) {
    super(node, cfg);
    
    // Configuration options with defaults
    this.showDisabled = this.cfg.config?.show_disabled !== false;
    this.highlightActiveAlarms = this.cfg.config?.highlight_active_alarms !== false;
    this.allowEdit = this.cfg.config?.allow_edit !== false;
    
    this.protectionData = null;
    this.isEditMode = false;
    this.expandedVar = null;   // 👈 track which variable is expanded

    this.renderStructure();
  }

  renderStructure() {
    // Create the main widget structure
    this.body.innerHTML = `
      <div class="protections-widget">
        <div class="protections-header">
          ${this.allowEdit ? '<button class="btn-edit" id="btn-edit-protections">Edit</button>' : ''}
        </div>
        <div class="protections-content">
          <div class="protections-loading">Loading protections...</div>
        </div>
      </div>
    `;

    // Attach event listeners
    if (this.allowEdit) {
      const editBtn = this.body.querySelector('#btn-edit-protections');
      if (editBtn) {
        editBtn.addEventListener('click', () => this.toggleEditMode());
      }
    }

    console.log('ProtectionsWidget initialized');
  }

  toggleEditMode() {
    this.isEditMode = !this.isEditMode;
    console.log('Edit mode:', this.isEditMode);
    
    if (this.isEditMode) {
      this.showEditModal();
    }
  }

  openEditModal(name, varData) {
    this.editingVar = name;
    this.editingData = JSON.parse(JSON.stringify(varData)); // deep clone
    
    const modal = document.createElement('div');
    modal.className = 'prot-modal-overlay';
    modal.innerHTML = `
      <div class="prot-modal">
        <div class="prot-modal-header">
          <h3>Edit Protection: ${varData.meta.display_name}</h3>
          <button class="prot-modal-close">&times;</button>
        </div>
        <div class="prot-modal-body">
          ${this.renderEditForm(name, this.editingData)}
        </div>
        <div class="prot-modal-footer">
          <button class="prot-btn prot-btn-secondary" data-action="recall-current">Recall Current</button>
          <button class="prot-btn prot-btn-secondary" data-action="recall-default">Recall Default</button>
          <button class="prot-btn prot-btn-secondary" data-action="reset">Reset</button>
          <button class="prot-btn prot-btn-primary" data-action="save">Save</button>
          <button class="prot-btn prot-btn-danger" data-action="deploy">Deploy</button>
        </div>
      </div>
    `;
    
    document.body.appendChild(modal);
    
    // Event handlers
    modal.querySelector('.prot-modal-close').onclick = () => this.closeEditModal();
    modal.querySelector('[data-action="recall-current"]').onclick = () => this.recallValues('current');
    modal.querySelector('[data-action="recall-default"]').onclick = () => this.recallValues('default');
    modal.querySelector('[data-action="reset"]').onclick = () => this.resetForm();
    modal.querySelector('[data-action="save"]').onclick = () => this.saveProtections(false);
    modal.querySelector('[data-action="deploy"]').onclick = () => this.saveProtections(true);
    
    // Close on overlay click
    modal.onclick = (e) => {
      if (e.target === modal) this.closeEditModal();
    };
  }

  closeEditModal() {
    const modal = document.querySelector('.prot-modal-overlay');
    if (modal) modal.remove();
    this.editingVar = null;
    this.editingData = null;
  }

  renderEditForm(name, varData) {
    const limits = varData.limits || {};
    const sides = ['max', 'min'];
    const levels = ['warning', 'alert', 'fault'];
    
    const renderSide = (side) => {
      const sideLimits = limits[side];
      if (!sideLimits) return '';
      
      const hysteresis = sideLimits.hysteresis || 0.0;
      
      return `
        <div class="prot-edit-side">
          <h4>${side.toUpperCase()} Limits</h4>
          
          <div class="prot-edit-row prot-edit-hyst">
            <label>Hysteresis</label>
            <input type="number" step="0.01" value="${hysteresis}" 
                  data-side="${side}" data-field="hysteresis" class="prot-input">
          </div>
          
          <table class="prot-edit-table">
            <thead>
              <tr>
                <th>Level</th>
                <th>Threshold</th>
                <th>On (s)</th>
                <th>Off (s)</th>
                <th>Enabled</th>
              </tr>
            </thead>
            <tbody>
              ${levels.map(level => {
                const cfg = sideLimits[level];
                if (!cfg) return '';
                
                return `
                  <tr>
                    <td class="prot-level">${level}</td>
                    <td>
                      <input type="number" step="0.01" value="${cfg.threshold}" 
                            data-side="${side}" data-level="${level}" data-field="threshold" 
                            class="prot-input prot-input-sm">
                    </td>
                    <td>
                      <input type="number" step="0.1" value="${cfg.on_duration}" 
                            data-side="${side}" data-level="${level}" data-field="on_duration" 
                            class="prot-input prot-input-sm">
                    </td>
                    <td>
                      <input type="number" step="0.1" value="${cfg.off_duration}" 
                            data-side="${side}" data-level="${level}" data-field="off_duration" 
                            class="prot-input prot-input-sm">
                    </td>
                    <td class="prot-center">
                      <input type="checkbox" ${cfg.enabled ? 'checked' : ''} 
                            data-side="${side}" data-level="${level}" data-field="enabled" 
                            class="prot-checkbox">
                    </td>
                  </tr>
                `;
              }).join('')}
            </tbody>
          </table>
        </div>
      `;
    };
    
    return sides.map(renderSide).join('');
  }

  async recallValues(source) {
    try {

      const data = await getJSON(`/api/db/protections?source=${source}`);
      
      if (data.ok && data.protections[this.editingVar]) {
        this.editingData = JSON.parse(JSON.stringify(data.protections[this.editingVar]));
        
        // Re-render the form
        const modalBody = document.querySelector('.prot-modal-body');
        if (modalBody) {
          modalBody.innerHTML = this.renderEditForm(this.editingVar, this.editingData);
        }
      } else {
        alert('Variable not found in recalled data');
      }
    } catch (err) {
      console.error('Failed to recall values:', err);
      alert('Failed to recall values');
    }
  }

  resetForm() {
    // Re-render with current editingData (undo any unsaved changes)
    const modalBody = document.querySelector('.prot-modal-body');
    if (modalBody) {
      modalBody.innerHTML = this.renderEditForm(this.editingVar, this.editingData);
    }
  }

  async saveProtections(deploy = false) {
    const modal = document.querySelector('.prot-modal');
    if (!modal || !this.editingVar || !this.editingData) {
      console.warn('saveProtections: missing modal/editingVar/editingData');
      return;
    }

    const inputs = modal.querySelectorAll('input');
    // Start from current editingData.limits
    const updatedLimits = JSON.parse(JSON.stringify(this.editingData.limits));

    console.log('saveProtections: starting with limits =', JSON.stringify(updatedLimits, null, 2));

    inputs.forEach(input => {
      const side  = input.dataset.side;    // "max" or "min"
      const level = input.dataset.level;   // "warning" | "alert" | "fault" | undefined
      const field = input.dataset.field;   // "threshold" | "on_duration" | "off_duration" | "enabled" | "hysteresis"

      const value = (input.type === 'checkbox') ? input.checked : input.value;

      console.log('Input:', {
        value,
        side,
        level,
        field
      });

      if (!side || !field) {
        console.warn('Skipping input (missing side/field)', { side, field });
        return;
      }

      if (!updatedLimits[side]) {
        updatedLimits[side] = {};
      }

      if (field === 'hysteresis') {
        updatedLimits[side].hysteresis = parseFloat(input.value) || 0.0;
        console.log(`Set ${side}.hysteresis =`, updatedLimits[side].hysteresis);
        return;
      }

      // For non-hysteresis fields, we must have a level
      if (!level) {
        console.warn('Skipping non-hysteresis input without level', { field });
        return;
      }

      if (!updatedLimits[side][level]) {
        // If the level didn't exist yet, create it
        updatedLimits[side][level] = {
          threshold: 0.0,
          on_duration: 0.0,
          off_duration: 0.0,
          enabled: false,
        };
      }

      if (field === 'enabled') {
        updatedLimits[side][level].enabled = input.checked;
      } else {
        updatedLimits[side][level][field] = parseFloat(input.value) || 0.0;
      }

      console.log(`Set ${side}.${level}.${field} =`,
        field === 'enabled'
          ? updatedLimits[side][level].enabled
          : updatedLimits[side][level][field]);
    });

    console.log('saveProtections: final updatedLimits =', JSON.stringify(updatedLimits, null, 2));

    // Build payload
    const payload = {
      protections: {
        [this.editingVar]: updatedLimits
      }
    };

    const endpoint = deploy ? '/api/protections/deploy' : '/api/protections';

    try {
      console.log('Saving protections to', endpoint, JSON.stringify(payload, null, 2));
      const data = await postJSON(endpoint, payload);
      console.log('Save protections response:', data);

      if (data && data.ok) {
        alert(deploy ? 'Protections deployed!' : 'Protections saved!');
        this.closeEditModal();
        console.log('Protections saved, refreshing...');
        // If you don’t have this yet, either add fetchProtections or remove this line
        if (this.fetchProtections) {
          await this.fetchProtections('current');
        }
      } else {
        const msg = (data && data.error) ? data.error : 'Unknown error';
        console.error('Failed to save protections:', msg);
        alert(`Failed to save protections: ${msg}`);
      }
    } catch (err) {
      console.error('Failed to save protections:', err);
      alert(`Failed to save protections: ${err.message}`);
    }
  }

  async xsaveProtections(deploy = false) {
    // Collect values from form
    const modal = document.querySelector('.prot-modal');
    if (!modal || !this.editingVar || !this.editingData) return;

    const inputs = modal.querySelectorAll('input');
    const updatedLimits = JSON.parse(JSON.stringify(this.editingData.limits));
    
    inputs.forEach(input => {
      const side = input.dataset.side;  // max or min
      const level = input.dataset.level; // warning, alert, fault, or undefined
      const field = input.dataset.field; // threshold, on_duration, off_duration, enabled, hysteresis
      
      if (!side || !field) return;
      
      if (field === 'hysteresis') {
        updatedLimits[side].hysteresis = parseFloat(input.value) || 0.0;
        return;

      if (!level) return
      if (!updatedLimits[side][level]) {
        updatedLimits[side][level] = {
          threshold: 0.0,
          on_duration: 0.0,
          off_duration: 0.0,
          enabled: false
        };

      }
      if (field === 'enabled') {
          updatedLimits[side][level].enabled = input.checked;
        } else {
          updatedLimits[side][level][field] = parseFloat(input.value) || 0.0;
        }
      }
    });
    
    // Build payload
    const payload = {
      protections: {
        [this.editingVar]: updatedLimits
      }
    };
    const endpoint = deploy ? '/api/protections/deploy' : '/api/protections';
    try {
      console.log('Saving protections to', endpoint, JSON.stringify(payload, null, 2));
      const data = await postJSON(endpoint, payload);
      console.log('Save protections response:', data);
      console.log('Save protections response data OK:', data.ok);

      if (data && data.ok) {
        alert(deploy ? 'Protections deployed!' : 'Protections saved!');
        this.closeEditModal();
        console.log('Protections saved, refreshing...');
        await this.fetchProtections('current'); // Refresh widget
      } else {
        const msg =(data && data.error) ? data.error : 'Unknown error';
        console.error('Failed to save protections:', msg);
        alert(`Failed to save protections #1 ${msg}`);
      }
    } catch (err) {
      console.error('Failed to save protections:', err);
      alert('Failed to save protections #2');
    }
  }

  async fetchProtections(source = 'current') {
    try {
      const data = await getJSON(`/api/protections?source=${source}`);
      
      console.log('Fetched protections data:', data );

      if (data && data.ok) {
        this.protections = data.protections;
        this.renderProtections(data);
      } else {
        console.error('Failed to fetch protections #1');
        this.root.innerHTML = '<div class="prot-error">Failed to load protections</div>';
      }
    } catch (err) {
      console.error('Failed to fetch protections #2:', err);
      this.root.innerHTML = `<div class="prot-error">Error: ${err.message}</div>`;
    }
  }
  showEditModal() {
    console.log('Opening edit modal... (to be implemented)');
    alert('Edit modal coming in next step!');
  }

  async tick(seriesCache) {
    // Fetch protection data on each tick
    try {
      // Use relative URL like other widgets - goes through main.py proxy
      const data = await getJSON("/api/protections?source=current");
      
      // if (!response.ok) {
      //   throw new Error(`HTTP error! status: ${response.status}`);
      // }
      
      this.protectionData = data;
      
      if (data && data.protections) {
        this.renderProtections(data);
      }
    } catch (error) {
      console.error('Error fetching protections:', error);
      
      const content = this.body.querySelector('.protections-content');
      if (content) {
        content.innerHTML = `<div class="protections-error">Error: ${error.message}</div>`;
      }
    }
  }

  renderProtections(data) {
    console.log('Rendering protections, data =', data);
    const content = this.body.querySelector('.protections-content');
    if (!content) return;

    const variables = Object.keys(data.protections || {});
    console.log('Rendering protections, variables =', variables);

    if (variables.length === 0) {
      content.innerHTML = '<div class="protections-empty">No protections configured</div>';
      return;
    }

    content.innerHTML = `
      <div class="protections-list">
        ${variables.map(name => {
          const varData = data.protections[name];
          const state = varData.current_state || 'normal';
          const v = varData.current_value;
          const value =
            v === null || v === undefined ? 'N/A'
            : typeof v === 'number' ? v.toFixed(2)
            : String(v);
          const unit = varData.meta?.unit || '';
          const isExpanded = this.expandedVar === name;

          return `
            <div class="protection-item ${this.highlightActiveAlarms && state !== 'normal' ? 'alarm-active' : ''}" data-var="${name}">
              <div class="protection-main">
                <button class="protection-toggle">
                  ${isExpanded ? '▾' : '▸'}
                </button>
                <span class="protection-name">${this.formatVariableName(name)}</span>
                <span class="protection-value">${value} ${unit}</span>
                <span class="protection-state state-${state}">${state.toUpperCase()}</span>
                <button class="protection-edit-btn no-toggle" data-var="${name}">✎ Edit</button>
              </div>
              ${isExpanded ? this.renderDetails(name, varData) : ''}
            </div>
          `;
        }).join('')}
      </div>
    `;

    // Attach click handlers for expand/collapse
    const items = content.querySelectorAll('.protection-item');
    items.forEach(item => {
      item.addEventListener('click', (ev) => {
        // don’t trigger on inner buttons in the future (e.g., edit/save)
        if (ev.target.closest('.no-toggle')) return;

        const varName = item.getAttribute('data-var');
        this.expandedVar = (this.expandedVar === varName) ? null : varName;
        this.renderProtections(this.protectionData);
      });
    });

    // Attach click handlers for edit buttons
    const editBtns = content.querySelectorAll('.protection-edit-btn');
    editBtns.forEach(btn => {
      btn.addEventListener('click', (ev) => {
        ev.stopPropagation();
        const varName = btn.getAttribute('data-var');
        const varData = data.protections[varName];
        this.openEditModal(varName, varData);
      });
    });
  }

  renderDetails(name, varData) {
    const limits = varData.limits || {};
    const sides = ['max', 'min'];
    const levels = ['warning', 'alert', 'fault'];

    const renderSide = (side) => {
      const sideLimits = limits[side];
      if (!sideLimits) return '';

      const hysteresis = sideLimits.hysteresis;

      // Check if this side has any actual level data
      const hasAnyLevel = levels.some(lv => sideLimits[lv]);
      const hasHyst = hysteresis !== undefined && hysteresis !== null;

      if (!hasAnyLevel && !hasHyst) return '';

      return `
        <div class="prot-side">
          <div class="prot-side-title">${side.toUpperCase()}</div>
          <table class="prot-table">
            <thead>
              <tr>
                <th>Level</th>
                <th>Threshold</th>
                <th>On (s)</th>
                <th>Off (s)</th>
                <th>En</th>
              </tr>
            </thead>
            <tbody>
              ${levels.map(level => {
                const cfg = sideLimits[level];
                if (!cfg) return '';

                const clsDisabled = !cfg.enabled ? 'prot-row-disabled' : '';
                return `
                  <tr class="${clsDisabled}">
                    <td class="prot-level">${level}</td>
                    <td class="prot-num">${this.fmtNum(cfg.threshold)}</td>
                    <td class="prot-num">${this.fmtNum(cfg.on_duration)}</td>
                    <td class="prot-num">${this.fmtNum(cfg.off_duration)}</td>
                    <td class="prot-center">${cfg.enabled ? '✓' : ''}</td>
                  </tr>
                `;
              }).join('')}
              ${hasHyst ? `
                <tr class="prot-hyst-row">
                  <td class="prot-level">Hysteresis</td>
                  <td class="prot-num">${this.fmtNum(hysteresis)}</td>
                  <td class="prot-num"></td>
                  <td class="prot-num"></td>
                  <td class="prot-center"></td>
                </tr>
              ` : ''}
            </tbody>
          </table>
        </div>
      `;
    };

    const html = sides.map(renderSide).join('');

    if (!html.trim()) {
      return `
        <div class="protection-details">
          <div class="prot-empty">No limits configured for this variable</div>
        </div>
      `;
    }

    return `
      <div class="protection-details">
        ${html}
      </div>
    `;
  }

  fmtNum(x) {
    if (x === null || x === undefined) return '';
    if (typeof x !== 'number') return String(x);
    if (Math.abs(x) >= 10) return x.toFixed(1);
    return x.toFixed(2);
  }

  xrenderDetails(name, varData) {
    const limits = varData.limits || {};
    const sides = ['max', 'min'];
    const levels = ['warning', 'alert', 'fault'];

    const renderSide = (side) => {
      const sideLimits = limits[side];
      if (!sideLimits) return '';  // no max or min block at all

      // Check if this side has any actual level data
      const hasAnyLevel = levels.some(lv => sideLimits[lv]);
      if (!hasAnyLevel) return '';

      return `
        <div class="prot-side">
          <div class="prot-side-title">${side.toUpperCase()}</div>
          <table class="prot-table">
            <thead>
              <tr>
                <th>Level</th>
                <th>Threshold</th>
                <th>On (s)</th>
                <th>Off (s)</th>
                <th>En</th>
              </tr>
            </thead>
            <tbody>
              ${levels.map(level => {
                const cfg = sideLimits[level];
                if (!cfg) return '';

                const clsDisabled = !cfg.enabled ? 'prot-row-disabled' : '';
                return `
                  <tr class="${clsDisabled}">
                    <td class="prot-level">${level}</td>
                    <td class="prot-num">${this.fmtNum(cfg.threshold)}</td>
                    <td class="prot-num">${this.fmtNum(cfg.on_duration)}</td>
                    <td class="prot-num">${this.fmtNum(cfg.off_duration)}</td>
                    <td class="prot-center">${cfg.enabled ? '✓' : ''}</td>
                  </tr>
                `;
              }).join('')}
            </tbody>
          </table>
        </div>
      `;
    };

    const html = sides.map(renderSide).join('');

    // If no limits at all, show a message
    if (!html.trim()) {
      return `
        <div class="protection-details">
          <div class="prot-empty">No limits configured for this variable</div>
        </div>
      `;
    }

    return `
      <div class="protection-details">
        ${html}
      </div>
    `;
  }

  fmtNum(x) {
    if (x === null || x === undefined) return '';
    if (typeof x !== 'number') return String(x);
    if (Math.abs(x) >= 10) return x.toFixed(1);
    return x.toFixed(2);
  }

xxrenderDetails(name, varData) {
    const limits = varData.limits || {};
    const sides = ['max', 'min'];
    const levels = ['warning', 'alert', 'fault'];

    const renderSide = (side) => {
      const sideLimits = limits[side];
      if (!sideLimits) return '';

      return `
        <div class="prot-side">
          <div class="prot-side-title">${side.toUpperCase()}</div>
          <table class="prot-table">
            <thead>
              <tr>
                <th>Level</th>
                <th>Threshold</th>
                <th>On (s)</th>
                <th>Off (s)</th>
                <th>En</th>
              </tr>
            </thead>
            <tbody>
              ${levels.map(level => {
                const cfg = sideLimits[level];
                if (!cfg) return '';

                const clsDisabled = !cfg.enabled ? 'prot-row-disabled' : '';
                return `
                  <tr class="${clsDisabled}">
                    <td class="prot-level">${level}</td>
                    <td class="prot-num">${this.fmtNum(cfg.threshold)}</td>
                    <td class="prot-num">${this.fmtNum(cfg.on_duration)}</td>
                    <td class="prot-num">${this.fmtNum(cfg.off_duration)}</td>
                    <td class="prot-center">${cfg.enabled ? '✓' : ''}</td>
                  </tr>
                `;
              }).join('')}
            </tbody>
          </table>
        </div>
      `;
    };

    return `
      <div class="protection-details">
        ${sides.map(renderSide).join('')}
      </div>
    `;
  }

  fmtNum(x) {
    if (x === null || x === undefined) return '';
    if (typeof x !== 'number') return String(x);
    if (Math.abs(x) >= 10) return x.toFixed(1);
    return x.toFixed(2);
  }

  formatVariableName(name) {
    // Convert snake_case to Title Case
    return name
      .split('_')
      .map(word => word.charAt(0).toUpperCase() + word.slice(1))
      .join(' ');
  }
}



// ------------------------------------------------------------------
// VariablesWidget  // Widget for displaying and managing system variables
// ------------------------------------------------------------------

class VariablesWidget extends BaseWidget {
    constructor(node,cfg) {
        super(node,cfg);
        this.variables = [];
        this.filteredVariables = [];
        this.systemVersion = 0;
        this.selectedCategory = 'oper';
        this.searchQuery = '';
        this.showPaths = false;
        this.sortBy = 'name';
        this.sortAsc = true;
        this.systemVersion = 'Unknown';
        
        // Load data once on initialization
        this.fetchVariables();
        this.fetchSystemVersion();

    }

    init() {
        console.log(">>>>>>>>>>init; category:", this.selectedCategory);
        this.fetchVariables();
        // Refresh every 5 seconds
        //setInterval(() => this.fetchVariables(), 5000);
    }

    tick(seriesCache) {
        //console.log("tick ; category:", this.selectedCategory);
        //this.fetchVariables();
        // Not using seriesCache for this widget
        // Data comes from variable registry API
    }

    async fetchVariables() {
        console.log("Fetching variables for category:", this.selectedCategory);
        try {
            const url = this.selectedCategory === 'all' 
                ? '/api/variables'
                : `/api/variables?category=${this.selectedCategory}`;


            console.log("Fetching variables for url:", url);
            const data = await getJSON(url);
            console.log("Fetching variables data:", data);

            if (Array.isArray(data)) {
                this.variables = data;
                console.log("before apply filters:");
                this.applyFilters();
                console.log("before apply render:");
                this.render();
            }
        } catch (error) {
            console.error('Error fetching variables:', error);
            this.renderError('Failed to load variables');
        }
    }

    async fetchSystemVersion() {
        try {
            const data = await $.getJSON('/api/system/version');
            this.systemVersion = data.system_version;
        } catch (error) {
            console.error('Error fetching system version:', error);
        }
    }

    applyFilters() {
        this.filteredVariables = this.variables.filter(v => {
            // Search filter
            if (this.searchQuery) {
                const query = this.searchQuery.toLowerCase();
                const matchName = v.name.toLowerCase().includes(query);
                const matchDisplay = v.display_name.toLowerCase().includes(query);
                const matchDesc = v.description.toLowerCase().includes(query);
                if (!matchName && !matchDisplay && !matchDesc) {
                    return false;
                }
            }
            return true;
        });

        // Sort
        this.filteredVariables.sort((a, b) => {
            let aVal, bVal;

            switch (this.sortBy) {
                case 'name':
                    aVal = a.name;
                    bVal = b.name;
                    break;
                case 'category':
                    aVal = a.category;
                    bVal = b.category;
                    break;
                case 'version':
                    aVal = a.variable_version;
                    bVal = b.variable_version;
                    break;
                default:
                    aVal = a.name;
                    bVal = b.name;
            }

            if (typeof aVal === 'string') {
                return this.sortAsc 
                    ? aVal.localeCompare(bVal)
                    : bVal.localeCompare(aVal);
            } else {
                return this.sortAsc ? aVal - bVal : bVal - aVal;
            }
        });
    }

    render() {
    console.log("running render #1");

    if (!this.body) {
        console.error("render: this.body is not set");
        return;
    }

    const html = `
        <div class="variables-widget">
            <div class="variables-header">
                <div class="variables-controls">
                    <div class="control-group">
                        <label>Category:</label>
                        <select class="category-filter">
                            <option value="all">All</option>
                            <option value="config">Config</option>
                            <option value="prot">Protection</option>
                            <option value="oper">Operational</option>
                        </select>
                    </div>

                    <div class="control-group">
                        <label>Search:</label>
                        <input type="text" class="search-input" placeholder="Search variables..." value="${this.searchQuery}">
                    </div>

                    <div class="control-group">
                        <label>Sort:</label>
                        <select class="sort-select">
                            <option value="name">Name</option>
                            <option value="category">Category</option>
                            <option value="version">Version</option>
                        </select>
                        <button class="sort-direction-btn" title="Toggle sort direction">
                            ${this.sortAsc ? '↑' : '↓'}
                        </button>
                    </div>

                    <div class="control-group">
                        <label>
                            <input type="checkbox" class="show-paths-checkbox" ${this.showPaths ? 'checked' : ''}>
                            Show Access Paths
                        </label>
                    </div>

                    <button class="refresh-btn">↻ Refresh</button>
                    <button class="add-variable-btn">+ Add Variable</button>
                </div>
            </div>

            <div class="variables-stats">
                <span>Total: ${this.variables.length}</span>
                <span>Filtered: ${this.filteredVariables.length}</span>
                <span>System Version: ${this.systemVersion}</span>
            </div>

            <div class="variables-list">
                ${this.renderVariablesList()}
            </div>
        </div>
    `;
    
    console.log("running render #2");

    this.body.innerHTML = html;
    this.attachEventHandlers();
}
    xrender() {
        console.log("running render #1");

    if (!this.cfg) {
        console.error("render: this.cfg is not set", this.cfg);
        return;
    }
    console.log("render: this.cfg is ", this.cfg);
    console.log("render: this.cfg.id is ", this.cfg.id);
    if (!this.cfg.id) {
        console.error("render: this.cfg.id is not set", this.cfg);
        return;
    }
    try {
        const container = $(`#${this.cfg.id}`);
        console.log("render container:", container);
    } catch (e) {
        console.error("ERROR creating container:", e);
        console.error("Stack:", e.stack);
        return;
    }
        const container = $(`#${this.cfg.id}`);
        console.log("render container :", container);  
        if (!container.length) {
          //we return here
           console.error("render container length :", container.length);
          return;
        }
        const html = `
            <div class="variables-widget">
                <div class="variables-header">
                    <div class="variables-controls">
                        <div class="control-group">
                            <label>Category:</label>
                            <select class="category-filter">
                                <option value="all">All</option>
                                <option value="config">Config</option>
                                <option value="prot">Protection</option>
                                <option value="oper">Operational</option>
                            </select>
                        </div>

                        <div class="control-group">
                            <label>Search:</label>
                            <input type="text" class="search-input" placeholder="Search variables..." value="${this.searchQuery}">
                        </div>

                        <div class="control-group">
                            <label>Sort:</label>
                            <select class="sort-select">
                                <option value="name">Name</option>
                                <option value="category">Category</option>
                                <option value="version">Version</option>
                            </select>
                            <button class="sort-direction-btn" title="Toggle sort direction">
                                ${this.sortAsc ? '↑' : '↓'}
                            </button>
                        </div>

                        <div class="control-group">
                            <label>
                                <input type="checkbox" class="show-paths-checkbox" ${this.showPaths ? 'checked' : ''}>
                                Show Access Paths
                            </label>
                        </div>

                        <button class="refresh-btn">↻ Refresh</button>
                        <button class="add-variable-btn">+ Add Variable</button>
                    </div>
                </div>

                <div class="variables-stats">
                    <span>Total: ${this.variables.length}</span>
                    <span>Filtered: ${this.filteredVariables.length}</span>
                    <span>System Version: ${this.systemVersion}</span>
                </div>

                <div class="variables-list">
                    ${this.renderVariablesList()}
                </div>
            </div>
        `;
        console.log("running render #2");

        container.html(html);
        this.attachEventHandlers();
    }

    renderVariablesList() {
        if (this.filteredVariables.length === 0) {
            return '<div class="no-variables">No variables found</div>';
        }

        return this.filteredVariables.map(v => this.renderVariableCard(v)).join('');
    }

    renderVariableCard(variable) {
        const categoryClass = `category-${variable.category}`;
        const pathsHtml = this.showPaths ? this.renderAccessPaths(variable.access_paths) : '';

        return `
            <div class="variable-card ${categoryClass}" data-variable-id="${variable.id}">
                <div class="variable-header">
                    <div class="variable-title">
                        <span class="variable-name">${variable.name}</span>
                        <span class="variable-category badge-${variable.category}">${variable.category}</span>
                    </div>
                    <div class="variable-actions">
                        <button class="edit-variable-btn" data-variable-name="${variable.name}">✎ Edit</button>
                        <button class="delete-variable-btn" data-variable-name="${variable.name}">🗑</button>
                    </div>
                </div>

                <div class="variable-body">
                    <div class="variable-info">
                        <div class="info-row">
                            <span class="label">Display Name:</span>
                            <span class="value">${variable.display_name}</span>
                        </div>
                        ${variable.units ? `
                        <div class="info-row">
                            <span class="label">Units:</span>
                            <span class="value">${variable.units}</span>
                        </div>
                        ` : ''}
                        ${variable.description ? `
                        <div class="info-row">
                            <span class="label">Description:</span>
                            <span class="value">${variable.description}</span>
                        </div>
                        ` : ''}
                        ${variable.locator ? `
                        <div class="info-row">
                            <span class="label">Locator:</span>
                            <span class="value">${variable.locator}</span>
                        </div>
                        ` : ''}
                        <div class="info-row">
                            <span class="label">Version:</span>
                            <span class="value">v${variable.variable_version} (sys: ${variable.system_version})</span>
                        </div>
                        <div class="info-row">
                            <span class="label">Source:</span>
                            <span class="value">${variable.source_system}</span>
                        </div>
                    </div>
                    ${pathsHtml}
                </div>
            </div>
        `;
    }

    renderAccessPaths(paths) {
        if (!paths || paths.length === 0) {
            return '<div class="access-paths"><em>No access paths</em></div>';
        }

        const pathsHtml = paths.map(path => {
            const ref = typeof path.reference === 'string' 
                ? JSON.parse(path.reference) 
                : path.reference;

            return `
                <div class="access-path ${path.active ? '' : 'inactive'}">
                    <span class="path-type">${path.access_type}</span>
                    <span class="path-ref">${JSON.stringify(ref)}</span>
                    <span class="path-priority">Priority: ${path.priority}</span>
                    ${path.read_only ? '<span class="read-only-badge">RO</span>' : ''}
                </div>
            `;
        }).join('');

        return `
            <div class="access-paths">
                <div class="access-paths-header">Access Paths:</div>
                ${pathsHtml}
            </div>
        `;
    }
renderError(message) {
    this.body.innerHTML = `
        <div class="variables-widget">
            <div class="error-message">${message}</div>
        </div>
    `;
}

attachEventHandlers() {
    // Use this.body instead of looking up by ID
    const container = this.body;

    // Category filter
    const categoryFilter = container.querySelector('.category-filter');
    if (categoryFilter) {
        categoryFilter.value = this.selectedCategory;
        categoryFilter.addEventListener('change', (e) => {
            this.selectedCategory = e.target.value;
            this.fetchVariables();
        });
    }

    // Search input
    const searchInput = container.querySelector('.search-input');
    if (searchInput) {
        searchInput.addEventListener('input', (e) => {
            this.searchQuery = e.target.value;
            this.applyFilters();
            this.render();
        });
    }

    // Sort controls
    const sortSelect = container.querySelector('.sort-select');
    if (sortSelect) {
        sortSelect.value = this.sortBy;
        sortSelect.addEventListener('change', (e) => {
            this.sortBy = e.target.value;
            this.applyFilters();
            this.render();
        });
    }

    const sortDirBtn = container.querySelector('.sort-direction-btn');
    if (sortDirBtn) {
        sortDirBtn.addEventListener('click', () => {
            this.sortAsc = !this.sortAsc;
            this.applyFilters();
            this.render();
        });
    }

    // Show paths checkbox
    const showPathsCheckbox = container.querySelector('.show-paths-checkbox');
    if (showPathsCheckbox) {
        showPathsCheckbox.addEventListener('change', (e) => {
            this.showPaths = e.target.checked;
            this.render();
        });
    }

    // Refresh button
    const refreshBtn = container.querySelector('.refresh-btn');
    if (refreshBtn) {
        refreshBtn.addEventListener('click', () => {
            this.fetchVariables();
            this.fetchSystemVersion();
        });
    }

    // Add variable button
    const addBtn = container.querySelector('.add-variable-btn');
    if (addBtn) {
        addBtn.addEventListener('click', () => {
            this.showAddVariableModal();
        });
    }

    // Edit variable buttons
    const editBtns = container.querySelectorAll('.edit-variable-btn');
    editBtns.forEach(btn => {
        btn.addEventListener('click', (e) => {
            const variableName = btn.getAttribute('data-variable-name');
            this.showEditVariableModal(variableName);
        });
    });

    // Delete variable buttons
    const deleteBtns = container.querySelectorAll('.delete-variable-btn');
    deleteBtns.forEach(btn => {
        btn.addEventListener('click', (e) => {
            const variableName = btn.getAttribute('data-variable-name');
            this.deleteVariable(variableName);
        });
    });
}
    // renderError(message) {
    //     const container = $(`#${this.cfg.id}`);
    //     container.html(`
    //         <div class="variables-widget">
    //             <div class="error-message">${message}</div>
    //         </div>
    //     `);
    // }

    // attachEventHandlers() {
    //     const container = $(`#${this.cfg.id}`);

    //     // Category filter
    //     container.find('.category-filter').val(this.selectedCategory).on('change', (e) => {
    //         this.selectedCategory = e.target.value;
    //         this.fetchVariables();
    //     });

    //     // Search input
    //     container.find('.search-input').on('input', (e) => {
    //         this.searchQuery = e.target.value;
    //         this.applyFilters();
    //         this.render();
    //     });

    //     // Sort controls
    //     container.find('.sort-select').val(this.sortBy).on('change', (e) => {
    //         this.sortBy = e.target.value;
    //         this.applyFilters();
    //         this.render();
    //     });

    //     container.find('.sort-direction-btn').on('click', () => {
    //         this.sortAsc = !this.sortAsc;
    //         this.applyFilters();
    //         this.render();
    //     });

    //     // Show paths checkbox
    //     container.find('.show-paths-checkbox').on('change', (e) => {
    //         this.showPaths = e.target.checked;
    //         this.render();
    //     });

    //     // Refresh button
    //     container.find('.refresh-btn').on('click', () => {
    //         this.fetchVariables();
    //         this.fetchSystemVersion();
    //     });

    //     // Add variable button
    //     container.find('.add-variable-btn').on('click', () => {
    //         this.showAddVariableModal();
    //     });

    //     // Edit variable buttons
    //     container.find('.edit-variable-btn').on('click', (e) => {
    //         const variableName = $(e.target).data('variable-name');
    //         this.showEditVariableModal(variableName);
    //     });

    //     // Delete variable buttons
    //     container.find('.delete-variable-btn').on('click', (e) => {
    //         const variableName = $(e.target).data('variable-name');
    //         this.deleteVariable(variableName);
    //     });
    // }

    showAddVariableModal() {
        const modalHtml = `
            <div class="modal-overlay" id="add-variable-modal">
                <div class="modal-content">
                    <div class="modal-header">
                        <h3>Add New Variable</h3>
                        <button class="modal-close">&times;</button>
                    </div>
                    <div class="modal-body">
                        <div class="form-group">
                            <label>Name:</label>
                            <input type="text" id="var-name" placeholder="e.g., battery_voltage">
                        </div>
                        <div class="form-group">
                            <label>Display Name:</label>
                            <input type="text" id="var-display-name" placeholder="e.g., Battery Voltage">
                        </div>
                        <div class="form-group">
                            <label>Units:</label>
                            <input type="text" id="var-units" placeholder="e.g., V">
                        </div>
                        <div class="form-group">
                            <label>Description:</label>
                            <textarea id="var-description" rows="3"></textarea>
                        </div>
                        <div class="form-group">
                            <label>Category:</label>
                            <select id="var-category">
                                <option value="oper">Operational</option>
                                <option value="config">Config</option>
                                <option value="prot">Protection</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Locator (optional):</label>
                            <input type="text" id="var-locator" placeholder="e.g., rbms.battery.voltage">
                        </div>
                    </div>
                    <div class="modal-footer">
                        <button class="btn-cancel">Cancel</button>
                        <button class="btn-save">Create Variable</button>
                    </div>
                </div>
            </div>
        `;

        $('body').append(modalHtml);

        $('#add-variable-modal .modal-close, #add-variable-modal .btn-cancel').on('click', () => {
            $('#add-variable-modal').remove();
        });

        $('#add-variable-modal .btn-save').on('click', () => {
            this.createVariable();
        });
    }

    async createVariable() {
        const data = {
            name: $('#var-name').val().trim(),
            display_name: $('#var-display-name').val().trim(),
            units: $('#var-units').val().trim(),
            description: $('#var-description').val().trim(),
            category: $('#var-category').val(),
            locator: $('#var-locator').val().trim() || null,
            source_system: 'ui'
        };

        if (!data.name || !data.display_name) {
            alert('Name and Display Name are required');
            return;
        }

        try {
            await $.ajax({
                url: '/api/variables',
                method: 'POST',
                contentType: 'application/json',
                data: JSON.stringify(data)
            });

            $('#add-variable-modal').remove();
            this.fetchVariables();
            alert('Variable created successfully');
        } catch (error) {
            console.error('Error creating variable:', error);
            alert('Failed to create variable: ' + (error.responseJSON?.detail || error.message));
        }
    }

    async showEditVariableModal(variableName) {
        // Fetch full variable details
        try {
            const variable = await $.getJSON(`/api/variables/${variableName}`);

            const modalHtml = `
                <div class="modal-overlay" id="edit-variable-modal">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h3>Edit Variable: ${variable.name}</h3>
                            <button class="modal-close">&times;</button>
                        </div>
                        <div class="modal-body">
                            <div class="form-group">
                                <label>Name:</label>
                                <input type="text" id="edit-var-name" value="${variable.name}" disabled>
                            </div>
                            <div class="form-group">
                                <label>Display Name:</label>
                                <input type="text" id="edit-var-display-name" value="${variable.display_name}">
                            </div>
                            <div class="form-group">
                                <label>Units:</label>
                                <input type="text" id="edit-var-units" value="${variable.units}">
                            </div>
                            <div class="form-group">
                                <label>Description:</label>
                                <textarea id="edit-var-description" rows="3">${variable.description}</textarea>
                            </div>
                            <div class="form-group">
                                <label>Category:</label>
                                <select id="edit-var-category">
                                    <option value="oper" ${variable.category === 'oper' ? 'selected' : ''}>Operational</option>
                                    <option value="config" ${variable.category === 'config' ? 'selected' : ''}>Config</option>
                                    <option value="prot" ${variable.category === 'prot' ? 'selected' : ''}>Protection</option>
                                </select>
                            </div>
                            <div class="form-group">
                                <label>Locator:</label>
                                <input type="text" id="edit-var-locator" value="${variable.locator || ''}">
                            </div>
                        </div>
                        <div class="modal-footer">
                            <button class="btn-cancel">Cancel</button>
                            <button class="btn-save">Save Changes</button>
                        </div>
                    </div>
                </div>
            `;

            $('body').append(modalHtml);

            $('#edit-variable-modal .modal-close, #edit-variable-modal .btn-cancel').on('click', () => {
                $('#edit-variable-modal').remove();
            });

            $('#edit-variable-modal .btn-save').on('click', () => {
                this.updateVariable(variableName);
            });
        } catch (error) {
            console.error('Error fetching variable:', error);
            alert('Failed to load variable details');
        }
    }

    async updateVariable(variableName) {
        const data = {
            display_name: $('#edit-var-display-name').val().trim(),
            units: $('#edit-var-units').val().trim(),
            description: $('#edit-var-description').val().trim(),
            category: $('#edit-var-category').val(),
            locator: $('#edit-var-locator').val().trim() || null
        };

        try {
            await $.ajax({
                url: `/api/variables/${variableName}`,
                method: 'PUT',
                contentType: 'application/json',
                data: JSON.stringify(data)
            });

            $('#edit-variable-modal').remove();
            this.fetchVariables();
            alert('Variable updated successfully');
        } catch (error) {
            console.error('Error updating variable:', error);
            alert('Failed to update variable: ' + (error.responseJSON?.detail || error.message));
        }
    }

    async deleteVariable(variableName) {
        if (!confirm(`Are you sure you want to delete variable "${variableName}"?`)) {
            return;
        }

        try {
            await $.ajax({
                url: `/api/variables/${variableName}`,
                method: 'DELETE'
            });

            this.fetchVariables();
            alert('Variable deleted successfully');
        } catch (error) {
            console.error('Error deleting variable:', error);
            alert('Failed to delete variable: ' + (error.responseJSON?.detail || error.message));
        }
    }
}

// class ProtectionsWidget {
//   constructor(container, config) {
//     this.container = container;
//     this.config = config || {};
//     this.protectionData = null;
//     this.isEditMode = false;
    
//     // Configuration options with defaults
//     this.showDisabled = this.config.show_disabled !== false;
//     this.highlightActiveAlarms = this.config.highlight_active_alarms !== false;
//     this.allowEdit = this.config.allow_edit !== false;
    
//     this.render();
//   }

//   render() {
//     // Create the main widget structure
//     this.container.innerHTML = `
//       <div class="protections-widget">
//         <div class="protections-header">
//           ${this.allowEdit ? '<button class="btn-edit" id="btn-edit-protections">Edit</button>' : ''}
//         </div>
//         <div class="protections-content">
//           <div class="protections-loading">Loading protections...</div>
//         </div>
//       </div>
//     `;

//     // Attach event listeners
//     if (this.allowEdit) {
//       const editBtn = this.container.querySelector('#btn-edit-protections');
//       if (editBtn) {
//         editBtn.addEventListener('click', () => this.toggleEditMode());
//       }
//     }

//     console.log('ProtectionsWidget initialized');
//   }

//   toggleEditMode() {
//     this.isEditMode = !this.isEditMode;
//     console.log('Edit mode:', this.isEditMode);
    
//     if (this.isEditMode) {
//       this.showEditModal();
//     }
//   }

//   showEditModal() {
//     console.log('Opening edit modal... (to be implemented)');
//     alert('Edit modal coming in next step!');
//   }

//   async fetchProtections() {
//     try {
//       const response = await fetch(`${API_BASE}/api/protections?source=current`);
//       if (!response.ok) {
//         throw new Error(`HTTP error! status: ${response.status}`);
//       }
//       const data = await response.json();
//       this.protectionData = data;
//       console.log('Fetched protection data:', data);
//       return data;
//     } catch (error) {
//       console.error('Error fetching protections:', error);
//       return null;
//     }
//   }

//   async update() {
//     const data = await this.fetchProtections();
//     if (data && data.protections) {
//       this.renderProtections(data);
//     }
//   }

//   renderProtections(data) {
//     const content = this.container.querySelector('.protections-content');
//     if (!content) return;

//     const variables = Object.keys(data.protections || {});
    
//     if (variables.length === 0) {
//       content.innerHTML = '<div class="protections-empty">No protections configured</div>';
//       return;
//     }

//     content.innerHTML = `
//       <div class="protections-list">
//         ${variables.map(varName => {
//           const varData = data.protections[varName];
//           const state = varData.current_state || 'normal';
//           const value = varData.current_value !== null && varData.current_value !== undefined 
//             ? varData.current_value.toFixed(2) 
//             : 'N/A';
          
//           return `
//             <div class="protection-item ${this.highlightActiveAlarms && state !== 'normal' ? 'alarm-active' : ''}">
//               <div class="protection-main">
//                 <span class="protection-name">${this.formatVariableName(varName)}</span>
//                 <span class="protection-value">${value} ${varData.meta.unit || ''}</span>
//                 <span class="protection-state state-${state}">${state.toUpperCase()}</span>
//               </div>
//             </div>
//           `;
//         }).join('')}
//       </div>
//     `;
//   }

//   formatVariableName(name) {
//     // Convert snake_case to Title Case
//     return name
//       .split('_')
//       .map(word => word.charAt(0).toUpperCase() + word.slice(1))
//       .join(' ');
//   }

//   destroy() {
//     console.log('ProtectionsWidget destroyed');
//   }
// }

// // Register the widget
// if (typeof window.WidgetRegistry !== 'undefined') {
//   window.WidgetRegistry.register('protections', ProtectionsWidget);
// }
// ------------------------------------------------------------------
// ------------------------------------------------------------------
// Widget registry
// ------------------------------------------------------------------
const WidgetTypes = {
  table: (node,cfg)=>new TableWidget(node,cfg),
  line:  (node,cfg)=>new LineWidget(node,cfg),
  dial:  (node,cfg)=>new DialWidget(node,cfg),
  vartable: (node,cfg)=>new VarTableWidget(node,cfg),
  indexgrid: (node,cfg)=>new IndexGridWidget(node,cfg),   // 👈 new
  protections: (node,cfg)=>new ProtectionsWidget(node,cfg),   // 👈 new
  VariablesWidget: (node,cfg)=>new VariablesWidget(node,cfg)  // Add this

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