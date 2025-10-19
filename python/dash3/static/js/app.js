let DASH=null, ACTIVE_TAB=null, WIDGETS=[], POLL_T=null;
let activeTabIndex = 0;       // currently‑visible tab index
const tabs = [];              // collected tab descriptors

function renderTabs(tabs){
  const el=document.getElementById('tabs'); el.innerHTML='';
  for(const t of tabs){
    const b=document.createElement('div'); b.className='tab'; b.textContent=t.title||t.id;
    b.onclick=()=>{ document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active')); b.classList.add('active'); switchTab(t.id); };
    el.appendChild(b);
  }
  const first=el.querySelector('.tab'); if(first) first.classList.add('active');
}

function switchTab(tabId){
  ACTIVE_TAB = tabId;
  activeTabIndex = tabId;
  const container=document.getElementById('pages'); container.innerHTML='';

  if (tabId === 'controls') { controlsPage(container, DASH.controls); stopPolling(); return; }
  if (tabId === 'values')   { valuesPage(container, DASH.values);   stopPolling(); return; }

  const grid=document.createElement('div'); grid.className='grid';
  container.appendChild(grid);
  if (DASH.grid){
    grid.style.setProperty('--gap', `${DASH.grid.gap||10}px`);
    grid.style.setProperty('--cols', (DASH.grid.cols||12));
    grid.style.setProperty('--rowh', `${(DASH.grid.rowHeight||90)}px`);
    grid.style.gridTemplateColumns = `repeat(${DASH.grid.cols||12}, 1fr)`;
  }
  WIDGETS=[];
  for(const w of (DASH.widgets||[])){
    if (w.tab!==tabId) continue;
    const cell=document.createElement('div');
    place(cell, w.pos||{row:1,col:1,w:3,h:2});
    grid.appendChild(cell);
    const inst = (WidgetTypes[w.type]||WidgetTypes['table'])(cell, w);
    WIDGETS.push(inst);
  }
  startPolling();
}

function collectSeriesNamesForActiveTab() {
  const active = tabs[activeTabIndex];
  const names = [];
  if (!active || !Array.isArray(active.widgets)) return names;

  for (const w of active.widgets) {
    if (w && w.cfg && Array.isArray(w.cfg.series)) {
      for (const s of w.cfg.series) if (s?.name) names.push(s.name);
    }
  }
  return names;
}

function xxcollectSeriesNamesForActiveTab(){
  const names=new Set();
  for (const w of WIDGETS){
    if (w.cfg.series) for (const s of w.cfg.series) if (s && s.name) names.add(s.name);
  }
  return Array.from(names);
}

// ---------------------------------------------------------
// Helper to append ?profile=<activeProfile> to any URL
// ---------------------------------------------------------
function withProfileURL(baseURL) {
  const url = new URL(baseURL, window.location.origin);
  if (window.activeProfile) url.searchParams.set('profile', window.activeProfile);
  return url.toString();
}

async function pollOnce() {
  // collect all active series names
  const names = collectSeriesNamesForActiveTab();

  // compute max window length safely across all widgets with valid cfg
  const windowSec = Math.max(
    ...(
      WIDGETS
        .filter(w => w && w.cfg && Array.isArray(w.cfg.series))
        .flatMap(w =>
          w.cfg.series.map(s => s.windowSec || 60)
        )
    ),
    60 // default
  );

  let cache = {};

  if (names.length) {
    try {
      const js = await getJSON(
        withProfileURL('/series' + qparams({ names: names.join(','), window: windowSec }))
      );
      // const js = await getJSON('/series' + qparams({
      //   names: names.join(','),
      //   window: windowSec
      // }));
      cache = js.series || {};
    } catch (err) {
      console.warn('Polling failed:', err);
    }
  }

  // tick each widget safely
  for (const w of WIDGETS) {
    if (!w || typeof w.tick !== 'function') continue;

    try {
      if (w.cfg && w.cfg.type === 'table') {
        await w.tick();
      } else {
        await w.tick(cache);
      }
    } catch (err) {
      console.warn('Widget tick error:', w, err);
    }
  }
}

async function pollOnce1(){
  const names = collectSeriesNamesForActiveTab();
  const windowSec = Math.max(...(WIDGETS.flatMap(w => (w.cfg.series||[]).map(s=>s.windowSec||60))), 60);
  let cache = {};
  if (names.length){
    const js = await getJSON('/series' + qparams({ names:names.join(','), window:windowSec }));
    cache = js.series||{};
  }
  for (const w of WIDGETS){
    if (w.tick) {
      if (w.cfg.type==='table') await w.tick();
      else await w.tick(cache);
    }
  }
}
function startPolling(){ if (POLL_T) clearInterval(POLL_T); POLL_T = setInterval(pollOnce, 1000); pollOnce(); }
function stopPolling(){ if (POLL_T) { clearInterval(POLL_T); POLL_T = null; } }

async function loadDash(){
  const d = await getJSON('/dash.json');
  DASH = d;
  renderTabs(d.tabs||[]);
  switchTab((d.tabs&&d.tabs[0]&&d.tabs[0].id) || 'overview');
}

// const WidgetTypes = window.WidgetTypes; // from widgets.js
// window.addEventListener('DOMContentLoaded', loadDash);
window.addEventListener('DOMContentLoaded', loadDash);
