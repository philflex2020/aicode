async function getJSON(url) {
  const r = await fetch(url, {cache:'no-store'});
  if (!r.ok) throw new Error(await r.text());
  return r.json();
}
async function postJSON(url, data) {
  const r = await fetch(url, {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(data||{})});
  if (!r.ok) throw new Error(await r.text());
  return r.json();
}
function qparams(obj){
  const u = new URLSearchParams();
  for (const [k,v] of Object.entries(obj)) if (v!=null) u.set(k, v);
  const s = u.toString();
  return s ? '?' + s : '';
}
