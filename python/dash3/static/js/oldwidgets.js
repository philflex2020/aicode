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

class BaseWidget {
  constructor(node, cfg){ this.node=node; this.cfg=cfg; this.body=null; this.head=null; this.initFrame(); }
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
  onShow(){} onHide(){}
}

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
  loadVisible(){ try { return JSON.parse(localStorage.getItem(this.storeKey)||'null'); } catch(_) { return null; } }
  saveVisible(){ localStorage.setItem(this.storeKey, JSON.stringify(this.visible)); }
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

class LineWidget extends BaseWidget {
  constructor(node, cfg){ super(node,cfg); this.canvas=document.createElement('canvas'); this.body.appendChild(this.canvas); this.last=[]; }
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

class DialWidget extends BaseWidget {
  constructor(node, cfg){ super(node,cfg); this.canvas=document.createElement('canvas'); this.body.appendChild(this.canvas); }
  async tick(seriesCache){
    const name = (this.cfg.series&&this.cfg.series[0]&&this.cfg.series[0].name) || '';
    const pts = (seriesCache && seriesCache[name]) || [];
    const v = pts.length ? pts[pts.length-1][1] : 0;
    const min = (this.cfg.opts&&this.cfg.opts.min)||0;
    const max = (this.cfg.opts&&this.cfg.opts.max)||100;
    drawDial(this.canvas, v, min, max, this.cfg.title||name);
  }
}

class VarTableWidget extends BaseWidget {
  constructor(node, cfg){
    super(node,cfg);
    this.tbl=document.createElement('table');
    const thead=document.createElement('thead'), tr=document.createElement('tr');
    (this.cfg.columns || ['name','value']).forEach(h=>{
      const th=document.createElement('th'); th.textContent=h; tr.appendChild(th);
    });
    thead.appendChild(tr); this.tbl.appendChild(thead);
    this.tbody=document.createElement('tbody'); this.tbl.appendChild(this.tbody);
    this.body.innerHTML=''; this.body.appendChild(this.tbl);
  }
  async tick(){
    const names = (this.cfg.names||[]).join(',');
    const rack  = (this.cfg.rack!=null ? this.cfg.rack : 0);
    if (!names) return;
    const rsp = await getJSON('/vars?names='+encodeURIComponent(names)+'&rack='+rack);
    this.tbody.innerHTML = '';
    for (const nm of (this.cfg.names||[])) {
      const tr=document.createElement('tr');
      const v = rsp[nm];
      tr.innerHTML = `<td>${nm}</td><td>${(Array.isArray(v)? v.join(', ') : (v==null?'—':v))}</td>`;
      this.tbody.appendChild(tr);
    }
  }
}

const WidgetTypes = {
  table: (node,cfg)=>new TableWidget(node,cfg),
  line:  (node,cfg)=>new LineWidget(node,cfg),
  dial:  (node,cfg)=>new DialWidget(node,cfg),
  vartable: (node,cfg)=>new VarTableWidget(node,cfg),
};
