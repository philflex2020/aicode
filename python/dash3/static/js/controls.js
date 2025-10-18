async function apiGetState(){ return getJSON('/api/state'); }
async function apiSetState(data){ return postJSON('/api/state', {data}); }
async function apiButton(name){ return postJSON('/api/button/'+encodeURIComponent(name), {}); }

function place(node, pos){
  node.style.gridColumn = `${pos.col} / span ${pos.w}`;
  node.style.gridRow    = `${pos.row} / span ${pos.h}`;
}

// Circular knob component (canvas)
function createCircularKnob(el, body){
  const wrap=document.createElement('div'); wrap.className='knob-wrap';
  const knobSize = 120;
  const canvas=document.createElement('canvas');
  canvas.width = knobSize * (window.devicePixelRatio||1);
  canvas.height = knobSize * (window.devicePixelRatio||1);
  canvas.style.width = knobSize+'px';
  canvas.style.height = knobSize+'px';
  const label=document.createElement('div');
  label.className='small';
  label.innerHTML=`${el.label||el.id}: <span id="${el.id}_read"></span>`;
  wrap.appendChild(canvas);
  wrap.appendChild(label);
  body.appendChild(wrap);

  const cfg = {
    id: el.id,
    min: el.min ?? 0,
    max: el.max ?? 100,
    step: el.step ?? 1,
    value: el.default ?? 0
  };

  const hidden = document.createElement('input');
  hidden.type='hidden';
  hidden.id = el.id;
  hidden.value = cfg.value;
  body.appendChild(hidden);

  function quantize(v){ const step = +cfg.step || 1; return Math.round(v/step)*step; }
  function valueToAngle(v){
    const aStart = (135 * Math.PI/180), aEnd = (405 * Math.PI/180);
    const p = (v - cfg.min) / (cfg.max - cfg.min || 1);
    return aStart + (aEnd - aStart) * Math.min(1, Math.max(0, p));
  }
  function angleToValue(a){
    const aStart = (135 * Math.PI/180), aEnd = (405 * Math.PI/180);
    let p = (a - aStart) / (aEnd - aStart);
    p = Math.min(1, Math.max(0, p));
    return cfg.min + p * (cfg.max - cfg.min);
  }
  function drawKnob(val){
    const dpr = window.devicePixelRatio||1;
    const w = canvas.width/dpr, h = canvas.height/dpr;
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr,0,0,dpr,0,0);
    ctx.clearRect(0,0,w,h);

    const cx = w/2, cy = h/2;
    const R = Math.min(w,h)/2 - 8;
    const aStart = (135*Math.PI/180), aEnd = (405*Math.PI/180);
    const aVal = valueToAngle(val);

    // Background circle
    ctx.beginPath();
    ctx.arc(cx, cy, R, 0, Math.PI*2);
    ctx.fillStyle = '#101a33';
    ctx.fill();

    // Track
    ctx.beginPath();
    ctx.arc(cx, cy, R*0.8, aStart, aEnd);
    ctx.strokeStyle = '#2a3b5c';
    ctx.lineWidth = 8;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Value arc
    ctx.beginPath();
    ctx.arc(cx, cy, R*0.8, aStart, aVal);
    ctx.strokeStyle = '#6bd';
    ctx.lineWidth = 8;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Indicator
    const r2 = R*0.65;
    const ix = cx + r2 * Math.cos(aVal);
    const iy = cy + r2 * Math.sin(aVal);
    ctx.beginPath();
    ctx.arc(ix, iy, 6, 0, Math.PI*2);
    ctx.fillStyle = '#9bd';
    ctx.fill();

    // Value text
    ctx.fillStyle = '#e8eef6';
    ctx.font = '600 14px system-ui';
    ctx.textAlign = 'center';
    ctx.fillText(String(quantize(val)), cx, cy+4);
  }
  function update(val){
    val = Math.min(cfg.max, Math.max(cfg.min, quantize(val)));
    cfg.value = val;
    document.getElementById(el.id+'_read').textContent = String(val);
    hidden.value = String(val);
    drawKnob(val);
  }
  function clientAngle(x, y){
    const rect = canvas.getBoundingClientRect();
    const cx = rect.left + rect.width/2;
    const cy = rect.top + rect.height/2;
    return Math.atan2(y - cy, x - cx);
  }
  let dragging = false;
  function setFromPointer(e){
    const pt = e.touches ? e.touches[0] : e;
    const ang = clientAngle(pt.clientX, pt.clientY);
    const aStart = (135*Math.PI/180), aEnd = (405*Math.PI/180);
    let a = ang;
    while (a < aStart - Math.PI) a += Math.PI*2;
    while (a > aEnd + Math.PI) a -= Math.PI*2;
    const v = angleToValue(a);
    update(v);
  }
  canvas.addEventListener('mousedown', (e)=>{ dragging=true; setFromPointer(e); });
  canvas.addEventListener('mousemove', (e)=>{ if (dragging) setFromPointer(e); });
  window.addEventListener('mouseup', ()=>{ dragging=false; });
  canvas.addEventListener('touchstart', (e)=>{ dragging=true; setFromPointer(e); e.preventDefault(); }, {passive:false});
  canvas.addEventListener('touchmove',  (e)=>{ if (dragging) setFromPointer(e); e.preventDefault(); }, {passive:false});
  window.addEventListener('touchend', ()=>{ dragging=false; });
  canvas.addEventListener('wheel', (e)=>{
    e.preventDefault();
    const delta = e.deltaY < 0 ? cfg.step : -cfg.step;
    update(cfg.value + delta);
  }, {passive:false});

  canvas._redraw = (v)=>{ cfg.value = v; drawKnob(v); };
  drawKnob(cfg.value);
}

function controlsPage(container, schema){
  if (!schema || !schema.cards) { container.innerHTML='<p>No controls defined</p>'; return; }
  const grid=document.createElement('div'); grid.className='grid';
  if (schema.layout){
    grid.style.setProperty('--gap', (schema.layout.gap||10)+'px');
    grid.style.setProperty('--cols', (schema.layout.cols||12));
    grid.style.setProperty('--rowh', (schema.layout.rowHeight||90)+'px');
    grid.style.gridTemplateColumns = `repeat(${schema.layout.cols||12}, 1fr)`;
  }
  container.appendChild(grid);

  for (const card of schema.cards){
    const c=document.createElement('div'); c.className='ctrl-card';
    place(c, card.pos||{row:1,col:1,w:4,h:2});
    grid.appendChild(c);
    if (card.title){
      const h3=document.createElement('h3'); h3.textContent=card.title; c.appendChild(h3);
    }
    const body=document.createElement('div'); body.className='ctrl-row'; c.appendChild(body);

    for (const el of (card.elements||[])){
      if (el.type==='text'){
        const lab=document.createElement('label'); lab.textContent=el.label||el.id;
        const inp=document.createElement('input'); inp.type='text'; inp.id=el.id; inp.placeholder=el.placeholder||'';
        lab.appendChild(inp); body.appendChild(lab);
      }
      else if (el.type==='slider'){
        const lab=document.createElement('label');
        lab.innerHTML = `${el.label||el.id}: <span id="${el.id}_read" class="small"></span>`;
        const inp=document.createElement('input'); inp.type='range'; inp.id=el.id;
        inp.min=el.min??0; inp.max=el.max??100; inp.step=el.step??1;
        inp.addEventListener('input', ()=>{ document.getElementById(el.id+'_read').textContent=inp.value; });
        lab.appendChild(inp); body.appendChild(lab);
      }
      else if (el.type==='checkbox'){
        const lab=document.createElement('label');
        const inp=document.createElement('input'); inp.type='checkbox'; inp.id=el.id;
        lab.appendChild(inp); lab.appendChild(document.createTextNode(el.label||el.id));
        body.appendChild(lab);
      }
      else if (el.type==='radio'){
        const wrap=document.createElement('div'); wrap.innerHTML=`<strong>${el.label||el.id}</strong><br/>`;
        for (const opt of (el.options||[])){
          const lab=document.createElement('label');
          const inp=document.createElement('input'); inp.type='radio'; inp.name=el.id; inp.value=opt.value;
          lab.appendChild(inp); lab.appendChild(document.createTextNode(opt.label||opt.value));
          wrap.appendChild(lab);
        }
        body.appendChild(wrap);
      }
      else if (el.type==='select'){
        const lab=document.createElement('label'); lab.innerHTML=`<strong>${el.label||el.id}</strong><br/>`;
        const sel=document.createElement('select'); sel.id=el.id;
        for (const opt of (el.options||[])){
          const o=document.createElement('option'); o.value=opt.value; o.textContent=opt.label||opt.value;
          sel.appendChild(o);
        }
        lab.appendChild(sel); body.appendChild(lab);
      }
      else if (el.type==='knob'){
        createCircularKnob(el, body);
      }
      else if (el.type==='button'){
        const btn=document.createElement('button'); btn.className='wbtn'; btn.textContent=el.label||el.id;
        btn.onclick=async ()=>{
          if (el.action==='set_state'){
            const payload={};
            for (const fid of (el.fields||[])){
              const inp=document.getElementById(fid);
              if (inp) payload[fid] = inp.type==='checkbox'? inp.checked : inp.value;
            }
            await apiSetState(payload);
            const {state}=await apiGetState(); writeInputs(state, schema);
          }
        };
        body.appendChild(btn);
      }
      else if (el.type==='action'){
        const btn=document.createElement('button'); btn.className='wbtn'; btn.textContent=el.label||el.id;
        btn.onclick=async ()=>{
          if (el.action==='button'){
            await apiButton(el.name||el.id);
            const {state}=await apiGetState(); writeInputs(state, schema);
          }
        };
        if (!body.querySelector('.btn-row')){
          const br=document.createElement('div'); br.className='btn-row'; body.appendChild(br);
        }
        body.querySelector('.btn-row').appendChild(btn);
      }
    }

    if (card.readbacks){
      for (const rb of card.readbacks){
        const d=document.createElement('div'); d.className='small';
        d.innerHTML = `${rb.label||rb.id}: <span id="${rb.id}_read"></span>`;
        body.appendChild(d);
      }
    }
  }

  apiGetState().then(({state})=>writeInputs(state, schema));
}

function writeInputs(state, schema){
  if (!schema || !schema.cards) return;
  for (const card of schema.cards){
    for (const el of (card.elements||[])){
      const inp=document.getElementById(el.id);
      if (!inp) continue;
      const val = state[el.id] ?? el.default;
      if (el.type==='text' || el.type==='slider' || el.type==='select'){
        inp.value = val ?? '';
        const read=document.getElementById(el.id+'_read');
        if (read) read.textContent = inp.value;
      }
      else if (el.type==='checkbox'){
        inp.checked = !!val;
      }
      else if (el.type==='radio'){
        document.querySelectorAll(`input[name="${el.id}"]`).forEach(r=>r.checked=(r.value===val));
      }
      else if (el.type==='knob'){
        inp.value = val ?? 0;
        const read=document.getElementById(el.id+'_read');
        if (read) read.textContent = inp.value;
        const span = document.getElementById(el.id+'_read');
        const wrap = span ? span.closest('.knob-wrap') : null;
        const canvas = wrap ? wrap.querySelector('canvas') : null;
        if (canvas && canvas._redraw) {
          canvas._redraw(Number(inp.value));
        }
      }
    }
    if (card.readbacks){
      for (const rb of card.readbacks){
        const span=document.getElementById(rb.id+'_read');
        if (span) span.textContent = state[rb.id]||'';
      }
    }
  }
}

function valuesPage(container, schema){
  if (!schema || !schema.fields) { container.innerHTML='<p>No values defined</p>'; return; }
  const card=document.createElement('div'); card.className='ctrl-card';
  container.appendChild(card);
  card.innerHTML = `
    <h3>Current Values</h3>
    <table class="values-table">
      <thead><tr>${(schema.columns||['name','value','readback']).map(c=>`<th>${c}</th>`).join('')}</tr></thead>
      <tbody id="values_body"></tbody>
    </table>
    <div class="action-line">
      <button id="send_all" class="wbtn">Send All</button>
      <span id="send_all_status" class="small"></span>
    </div>`;

  function readInputsSnapshot(){
    const snap={};
    for (const fid of schema.fields){
      const inp=document.getElementById(fid);
      if (!inp) { snap[fid]=null; continue; }
      if (inp.type==='checkbox') snap[fid]=inp.checked;
      else if (inp.type==='radio'){
        const checked=document.querySelector(`input[name="${fid}"]:checked`);
        snap[fid] = checked ? checked.value : null;
      }
      else snap[fid] = inp.value;
    }
    return snap;
  }
  function rowHTML(n,v,r){
    const safe = (x)=> (x==null?'':String(x));
    return `<tr><td>${n}</td><td>${safe(v)}</td><td>${safe(r)}</td></tr>`;
  }
  async function refresh(){
    const {state} = await apiGetState();
    const inputs = readInputsSnapshot();
    const tbody = document.getElementById('values_body');
    const rows = schema.fields.map(fid=>rowHTML(fid, inputs[fid], state[fid]));
    tbody.innerHTML = rows.join('');
  }
  document.getElementById('send_all').addEventListener('click', async ()=>{
    const s = document.getElementById('send_all_status');
    try{
      const inputs = readInputsSnapshot();
      const res = await apiSetState(inputs);
      s.textContent = res.ok ? 'Sent ✓' : 'Send failed';
      s.className = res.ok ? 'small status-ok' : 'small';
      setTimeout(()=>{ s.textContent=''; }, 1200);
      await refresh();
    }catch(e){ s.textContent='Send failed'; s.className='small'; }
  });

  refresh();
}
