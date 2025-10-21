#!/usr/bin/env python3
# modbus_multi_client.py
# Generic Modbus client (TCP, RTU-over-TCP, Serial RTU) with web UI:
# - Connections list (from connections.json) with status + Connect buttons
# - Groups runner (from mb_multi_groups.json) with expect checks
# - One-off Operation panel: pick connection, set op/FC/addr/etc, Execute
# pip install flask pyserial

import os, time, json, socket, struct, threading
from typing import List, Dict, Any, Optional
from flask import Flask, jsonify, request, Response

# ----------------------------- Utils -----------------------------

def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF

def pack_u16_be(v: int) -> bytes:
    return struct.pack(">H", v & 0xFFFF)

def unpack_u16_be(b: bytes) -> int:
    return struct.unpack(">H", b)[0]

def parse_int(v: Any, default: int = 0) -> int:
    try:
        if isinstance(v, int):
            return v
        s = str(v).strip()
        if s.endswith(("h", "H")):
            return int(s[:-1], 16)
        return int(s, 0)
    except Exception:
        return default

# --------------------------- Transports --------------------------

class Transport:
    def open(self): pass
    def close(self): pass
    def send_pdu(self, unit: int, pdu: bytes) -> bytes: raise NotImplementedError

class TcpTransport(Transport):
    """Modbus/TCP (port 502…)."""
    def __init__(self, host: str, port: int = 502, timeout: float = 0.7):
        self.host, self.port, self.timeout = host, int(port), float(timeout)
        self._sock: Optional[socket.socket] = None
        self._tid = 1
        self._lock = threading.Lock()

    def open(self):
        if self._sock:
            return
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(self.timeout)
        s.connect((self.host, self.port))
        self._sock = s

    def close(self):
        if self._sock:
            try: self._sock.close()
            finally: self._sock = None

    def _recv_exact(self, n: int) -> bytes:
        buf = bytearray()
        while len(buf) < n:
            chunk = self._sock.recv(n - len(buf))
            if not chunk: break
            buf.extend(chunk)
        return bytes(buf)

    def send_pdu(self, unit: int, pdu: bytes) -> bytes:
        try:
            if not self._sock:
                self.open()
            with self._lock:
                tid = self._tid & 0xFFFF
                self._tid += 1
                mbap = struct.pack(">HHHB", tid, 0, 1 + len(pdu), unit & 0xFF)
                self._sock.sendall(mbap + pdu)
                hdr = self._recv_exact(7)
                if len(hdr) != 7:
                    return b""
                r_tid, r_pid, r_len, r_uid = struct.unpack(">HHHB", hdr)
                if r_tid != tid or r_pid != 0 or r_uid != (unit & 0xFF):
                    return b""
                rpdu = self._recv_exact(r_len - 1)
                return rpdu
        except Exception:
            self.close()
            return b""

class RtuTcpTransport(Transport):
    """Modbus RTU frame over a raw TCP bridge (e.g., socket://ip:port)."""
    def __init__(self, host: str, port: int = 502, timeout: float = 0.7):
        self.host, self.port, self.timeout = host, int(port), float(timeout)
        self._sock: Optional[socket.socket] = None
        self._lock = threading.Lock()

    def open(self):
        if self._sock: return
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(self.timeout)
        s.connect((self.host, self.port))
        self._sock = s
        time.sleep(0.02)

    def close(self):
        if self._sock:
            try: self._sock.close()
            finally: self._sock = None

    def _recv_until_timeout(self, deadline: float) -> bytes:
        buf = bytearray()
        while time.time() < deadline:
            try:
                chunk = self._sock.recv(256)
                if chunk:
                    buf.extend(chunk)
                else:
                    time.sleep(0.01)
            except socket.timeout:
                break
        return bytes(buf)

    def send_pdu(self, unit: int, pdu: bytes) -> bytes:
        try:
            if not self._sock:
                self.open()
            with self._lock:
                adu = bytes([unit & 0xFF]) + pdu
                crc = crc16_modbus(adu)
                adu += bytes([crc & 0xFF, (crc >> 8) & 0xFF])
                self._sock.sendall(adu)
                deadline = time.time() + self.timeout
                buf = self._recv_until_timeout(deadline)
                if len(buf) < 3 or buf[0] != (unit & 0xFF):
                    return b""
                fc = buf[1]
                # Exceptions: addr, fc|0x80, code, crc
                if fc & 0x80:
                    return bytes([fc, buf[2]])  # PDU: exception fc + code
                if fc in (1,2,3,4):
                    if len(buf) >= 5:
                        bc = buf[2]
                        need = 3 + bc + 2
                        if len(buf) >= need:
                            data = buf[3:3+bc]
                            return bytes([fc, bc]) + data
                elif fc in (5,6,15,16):
                    # Echo: addr,fc,addr_hi,addr_lo,val_hi,val_lo,crc_lo,crc_hi
                    if len(buf) >= 8:
                        return bytes(buf[1:6])  # fc + 4 bytes
                return b""
        except Exception:
            self.close()
            return b""

try:
    import serial  # pyserial
    class SerialRtuTransport(Transport):
        def __init__(self, device: str, baud: int = 9600, parity: str = "N",
                     stopbits: int = 1, bytesize: int = 8, timeout: float = 0.7):
            self.device, self.baud = device, int(baud)
            self.parity = parity.upper()
            self.stopbits, self.bytesize = int(stopbits), int(bytesize)
            self.timeout = float(timeout)
            self._ser: Optional[serial.Serial] = None
            self._lock = threading.Lock()

        def open(self):
            if self._ser: return
            pr = {"N": serial.PARITY_NONE, "E": serial.PARITY_EVEN, "O": serial.PARITY_ODD}.get(self.parity, serial.PARITY_NONE)
            self._ser = serial.Serial(self.device, self.baud, parity=pr,
                                      stopbits=self.stopbits, bytesize=self.bytesize,
                                      timeout=self.timeout)
            time.sleep(0.02)

        def close(self):
            if self._ser:
                try: self._ser.close()
                finally: self._ser = None

        def send_pdu(self, unit: int, pdu: bytes) -> bytes:
            try:
                if not self._ser:
                    self.open()
                with self._lock:
                    frame = bytes([unit & 0xFF]) + pdu
                    crc = crc16_modbus(frame)
                    frame += bytes([crc & 0xFF, (crc >> 8) & 0xFF])
                    self._ser.reset_input_buffer()
                    self._ser.write(frame)
                    deadline = time.time() + self.timeout
                    buf = bytearray()
                    while time.time() < deadline:
                        chunk = self._ser.read(128)
                        if chunk:
                            buf.extend(chunk)
                            if len(buf) < 3 or buf[0] != (unit & 0xFF):  # wait for more
                                continue
                            fc = buf[1]
                            if fc & 0x80 and len(buf) >= 3:
                                return bytes([fc, buf[2]])
                            if fc in (1,2,3,4) and len(buf) >= 5:
                                bc = buf[2]
                                need = 3 + bc + 2
                                if len(buf) >= need:
                                    return bytes([fc, bc]) + bytes(buf[3:3+bc])
                            if fc in (5,6,15,16) and len(buf) >= 8:
                                return bytes(buf[1:6])
                        else:
                            time.sleep(0.01)
                    return b""
            except Exception:
                self.close()
                return b""
except Exception:
    SerialRtuTransport = None

# ------------------------- Modbus helpers ------------------------

def mb_read_bits(tr: Transport, unit: int, fc: int, addr: int, count: int) -> List[bool]:
    pdu = struct.pack(">BHH", fc, addr & 0xFFFF, count & 0xFFFF)
    rpdu = tr.send_pdu(unit, pdu)
    if not rpdu or (rpdu[0] & 0x80):  # exception
        return []
    if rpdu[0] != fc or len(rpdu) < 2:
        return []
    bc = rpdu[1]
    data = rpdu[2:2+bc]
    out = []
    for i in range(count):
        byte_i, bit_i = i // 8, i % 8
        if byte_i >= len(data): break
        out.append(bool((data[byte_i] >> bit_i) & 1))
    return out

def mb_read_regs(tr: Transport, unit: int, fc: int, addr: int, count: int) -> List[int]:
    pdu = struct.pack(">BHH", fc, addr & 0xFFFF, count & 0xFFFF)
    rpdu = tr.send_pdu(unit, pdu)
    if not rpdu or (rpdu[0] & 0x80):
        return []
    if rpdu[0] != fc or len(rpdu) < 2:
        return []
    bc = rpdu[1]
    data = rpdu[2:2+bc]
    if bc != 2*count or len(data) < 2*count:
        return []
    return [unpack_u16_be(data[i:i+2]) for i in range(0, 2*count, 2)]

def mb_write_single_reg(tr: Transport, unit: int, addr: int, value: int) -> bool:
    pdu = struct.pack(">BHH", 0x06, addr & 0xFFFF, value & 0xFFFF)
    rpdu = tr.send_pdu(unit, pdu)
    return bool(rpdu and rpdu[0] == 0x06)

def mb_write_many_regs(tr: Transport, unit: int, addr: int, values: List[int]) -> bool:
    count = max(0, min(len(values), 123))
    if count == 0: return False
    payload = b"".join(pack_u16_be(v) for v in values[:count])
    pdu = struct.pack(">BHHB", 0x10, addr & 0xFFFF, count, len(payload)) + payload
    rpdu = tr.send_pdu(unit, pdu)
    return bool(rpdu and rpdu[0] == 0x10)

def mb_write_single_coil(tr: Transport, unit: int, addr: int, on: bool) -> bool:
    val = 0xFF00 if on else 0x0000
    pdu = struct.pack(">BHH", 0x05, addr & 0xFFFF, val & 0xFFFF)
    rpdu = tr.send_pdu(unit, pdu)
    return bool(rpdu and rpdu[0] == 0x05)

def exec_modbus(tr: Transport, unit: int, op: str, fc: int, addr: int,
                count: int = 1, value: Optional[int] = None, values: Optional[List[int]] = None) -> Dict[str, Any]:
    try:
        if op == "read":
            if fc in (1,2):
                vals = mb_read_bits(tr, unit, fc, addr, count)
                return {"ok": vals != [], "values": vals}
            if fc in (3,4):
                vals = mb_read_regs(tr, unit, fc, addr, count)
                return {"ok": vals != [], "values": vals}
            return {"ok": False, "error":"unsupported read FC"}
        elif op == "write":
            if fc == 6 and value is not None:
                ok = mb_write_single_reg(tr, unit, addr, int(value))
                return {"ok": ok}
            if fc == 16 and values:
                ok = mb_write_many_regs(tr, unit, addr, [int(v) for v in values])
                return {"ok": ok}
            if fc == 5 and value is not None:
                ok = mb_write_single_coil(tr, unit, addr, bool(value))
                return {"ok": ok}
            return {"ok": False, "error":"unsupported write form"}
        else:
            return {"ok": False, "error":"unknown op"}
    except Exception as e:
        return {"ok": False, "error": str(e)}

# ------------------------- Expect evaluator ----------------------

def evaluate_expect(exp: Dict[str, Any], values: List[Any], is_bits: bool = False) -> Dict[str, Any]:
    try:
        if "equals" in exp:
            want = exp["equals"]
            ok = list(values) == list(want)
            return {"pass": ok, "mode":"equals", "detail": f"got={values}, want={want}"}
        if "any_of" in exp:
            any_ok = any(list(values) == list(c) for c in exp["any_of"])
            return {"pass": any_ok, "mode":"any_of", "detail": f"got={values}"}
        if "range" in exp and isinstance(exp["range"], dict):
            lo = exp["range"].get("min", float("-inf"))
            hi = exp["range"].get("max", float("+inf"))
            ok = all(lo <= v <= hi for v in values)
            return {"pass": ok, "mode":"range", "detail": f"{lo}<=values<= {hi}, got={values}"}
        if "mask_eq" in exp and isinstance(exp["mask_eq"], dict):
            m = parse_int(exp["mask_eq"].get("mask", 0xFFFF))
            e = parse_int(exp["mask_eq"].get("eq", 0))
            if isinstance(values, list) and values:
                ok = (values[0] & m) == e
                return {"pass": ok, "mode":"mask_eq", "detail": f"(v0 & 0x{m:04X})==0x{e:04X}, v0=0x{values[0]:04X}"}
            return {"pass": False, "mode":"mask_eq", "detail":"no value"}
        if "contains" in exp:
            sub = list(exp["contains"]); got = list(values)
            it = iter(got)
            ok = all(any(x==y for y in it) for x in sub)
            return {"pass": ok, "mode":"contains", "detail": f"want {sub} subseq of {got}"}
        return {"pass": True, "mode":"none", "detail":"no expectations"}
    except Exception as e:
        return {"pass": False, "mode":"error", "detail": str(e)}

# ---------------------------- Web UI -----------------------------

INDEX_HTML = """<!doctype html>
<html>
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RBMS Modbus Multi-Client</title>
<style>
  :root{--bg:#0e1626;--panel:#0f1b2f;--ink:#e8eef6;--muted:#93a2bd;--ok:#20e3b2;--bad:#ff5a6b;--warn:#ffcf5a;--btn:#13243e;--gray:#47546b;--accent:#3bd1ff}
  body{margin:0;background:var(--bg);color:var(--ink);font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif}
  .wrap{max-width:1200px;margin:24px auto;padding:0 16px}
  .card{background:var(--panel);border:1px solid #1f2a44;border-radius:14px;box-shadow:0 10px 30px rgba(0,0,0,.35);padding:16px 18px;margin-bottom:16px}
  h1{margin:0 0 10px;font-size:20px}
  h2{margin:0 0 6px;font-size:16px}
  .row{display:flex;gap:10px;align-items:center;justify-content:space-between;flex-wrap:wrap}
  .muted{color:var(--muted)}
  .btn{border:0;border-radius:12px;background:var(--btn);color:var(--ink);padding:10px 12px;cursor:pointer}
  .btn:hover{filter:brightness(1.15)}
  .btn.sml{padding:6px 10px;border-radius:10px;font-size:13px}
  .pill{padding:6px 10px;border-radius:999px;background:#13243e;color:#cfe0ff;font-size:12px}
  .legend{display:flex;gap:12px;align-items:center;flex-wrap:wrap}
  .field{display:flex;flex-direction:column;gap:4px}
  .lbl{font-size:12px;color:#cfe0ff;font-weight:600}
  input[type="text"], input[type="number"], select, textarea{
    background:#10203b;border:1px solid #2a3a60;color:#f3f7ff;border-radius:10px;padding:8px 10px;min-width:140px
  }
  textarea{min-height:90px}
  input::placeholder{color:#9fb1d6;opacity:1}
  .led{display:inline-block;width:12px;height:12px;border-radius:50%;vertical-align:middle;margin-right:6px;border:1px solid rgba(0,0,0,.3)}
  .green{background:var(--ok)} .red{background:var(--bad)} .yellow{background:var(--warn)} .gray{background:var(--gray)}
  .two{display:grid;grid-template-columns: 1fr 1fr;gap:14px}
  .list{border:1px solid #1f2a44;border-radius:12px;overflow:hidden}
  .item{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:10px 12px;border-bottom:1px solid #18223b;background:#0f1a2f}
  .item:last-child{border-bottom:none}
  .mono{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace}
  pre{background:#0c1424;border:1px solid #1f2a44;color:#e8eef6;border-radius:10px;padding:12px;white-space:pre-wrap;word-break:break-word;max-height:420px;overflow:auto}
  .status{font-size:11px;border-radius:999px;padding:4px 8px;border:1px solid #2a3a60}
  .status.pass{background:#0f2a22;border-color:#1d6b4e}
  .status.fail{background:#2a1314;border-color:#7e2d2f}
</style>
</head>
<body>
<div class="wrap">

  <div class="card">
    <div class="row">
      <h1>RBMS Modbus Multi-Client</h1>
      <div class="legend">
        <span class="pill">Device: <span id="devpill">—</span></span>
        <span><span id="ledConn" class="led gray"></span><span class="muted" id="connText">Disconnected</span></span>
      </div>
      <div><button class="btn" onclick="refresh()">Refresh</button></div>
    </div>
  </div>

  <!-- Connections -->
  <div class="card">
    <div class="row">
      <h2>Connections</h2>
      <div class="legend">
        <div class="field">
          <label class="lbl">connections.json</label>
          <input id="cfile" type="text" class="mono" placeholder="connections.json" style="min-width:260px">
        </div>
        <button class="btn" onclick="loadConnections()">Load</button>
        <button class="btn" onclick="probeAll()">Refresh Status</button>
        <span class="pill">Loaded: <span id="connCount">0</span></span>
      </div>
    </div>
    <div class="list" id="connList"></div>
  </div>

  <!-- Groups -->
  <div class="card">
    <div class="row">
      <h2>Groups</h2>
      <div class="legend">
        <div class="field">
          <label class="lbl">mb_multi_groups.json</label>
          <input id="gfile" type="text" class="mono" placeholder="mb_multi_groups.json" style="min-width:260px">
        </div>
        <button class="btn" onclick="loadGroups()">Load</button>
        <button class="btn" onclick="runAll()">Run All</button>
        <span class="pill">Loaded: <span id="groupsCount">0</span></span>
        <span class="pill">Last run: <span id="lastRunSummary">—</span></span>
      </div>
    </div>

    <div class="two">
      <div class="list" id="groupList"></div>
      <div>
        <div class="legend" style="margin-bottom:8px">
          <button class="btn sml" onclick="showSpec()">Show Group Spec</button>
          <button class="btn sml" onclick="showResult()">Show Last Result</button>
          <span id="currentGroup" class="pill">Group: —</span>
        </div>
        <pre id="detailBox">Select a group, then “Show Group Spec” or “Show Last Result”.</pre>
      </div>
    </div>
  </div>

  <!-- Operation (one-off) -->
  <div class="card">
    <div class="row">
      <h2>Operation</h2>
      <div class="legend">
        <div class="field">
          <label class="lbl">Connection</label>
          <select id="opConn">
            <option value="">(use current)</option>
          </select>
        </div>
        <div class="field">
          <label class="lbl">Unit</label>
          <input id="opUnit" type="number" min="1" max="247" value="1" style="width:110px">
        </div>
        <div class="field">
          <label class="lbl">Op</label>
          <select id="opType">
            <option value="read">read</option>
            <option value="write">write</option>
          </select>
        </div>
        <div class="field">
          <label class="lbl">FC</label>
          <select id="opFC">
            <option>1</option><option>2</option><option selected>3</option><option>4</option>
            <option>5</option><option>6</option><option>15</option><option>16</option>
          </select>
        </div>
        <div class="field">
          <label class="lbl">Address</label>
          <input id="opAddr" type="text" class="mono" placeholder="0x0000">
        </div>
        <div class="field">
          <label class="lbl">Count</label>
          <input id="opCount" type="number" min="1" max="2000" value="1" style="width:110px">
        </div>
        <div class="field" id="valueField">
          <label class="lbl">Value (single)</label>
          <input id="opValue" type="text" class="mono" placeholder="e.g. 1 or 0x1234">
        </div>
        <div class="field" id="valuesField" style="display:none">
          <label class="lbl">Values (comma)</label>
          <input id="opValues" type="text" class="mono" placeholder="e.g. 0x1,0x2,0x3">
        </div>
        <button class="btn" onclick="execOp()">Execute</button>
      </div>
    </div>

    <div class="row" style="margin-top:10px">
      <div class="field" style="flex:1 1 320px">
        <label class="lbl">Expect (optional JSON)</label>
        <textarea id="opExpect" class="mono" placeholder='e.g. {"equals":[0,1]}'></textarea>
      </div>
      <div class="field" style="flex:1 1 320px">
        <label class="lbl">Result</label>
        <pre id="opResult">—</pre>
      </div>
    </div>
  </div>

</div>

<script>
let dev = null;
let connections = [];
let groups = [];
let resultsByName = {};
let selectedName = null;

const devpill = document.getElementById('devpill');
const ledConn = document.getElementById('ledConn');
const connText= document.getElementById('connText');
const connList= document.getElementById('connList');
const connCount=document.getElementById('connCount');
const groupList = document.getElementById('groupList');
const groupsCount = document.getElementById('groupsCount');
const lastRunSummary = document.getElementById('lastRunSummary');
const detailBox = document.getElementById('detailBox');
const currentGroup = document.getElementById('currentGroup');
const opConn = document.getElementById('opConn');
const opUnit = document.getElementById('opUnit');
const opType = document.getElementById('opType');
const opFC   = document.getElementById('opFC');
const opAddr = document.getElementById('opAddr');
const opCount= document.getElementById('opCount');
const opValue= document.getElementById('opValue');
const opValues=document.getElementById('opValues');
const valueField = document.getElementById('valueField');
const valuesField= document.getElementById('valuesField');
const opExpect   = document.getElementById('opExpect');
const opResult   = document.getElementById('opResult');
const gfile      = document.getElementById('gfile');
const cfile      = document.getElementById('cfile');

function setLed(ok){
  ledConn.className = 'led ' + (ok===true?'green':ok===false?'red':'gray');
  connText.textContent = ok===true ? 'Connected' : ok===false ? 'No reply' : '—';
}

function connTitle(c){
  if(c.type==='tcp' || c.type==='rtu-tcp') return `${c.name} · ${c.type} · ${c.host}:${c.port} · unit ${c.unit}`;
  if(c.type==='serial') return `${c.name} · serial · ${c.device}@${c.baud} · unit ${c.unit}`;
  return `${c.name} · ${c.type}`;
}

function renderConnections(){
  opConn.innerHTML = '<option value="">(use current)</option>';
  connList.innerHTML = "";
  connections.forEach(c=>{
    // populate op dropdown
    const opt = document.createElement('option');
    opt.value = c.name; opt.textContent = c.name;
    opConn.appendChild(opt);
    // list row
    const row = document.createElement('div'); row.className='item';
    const left = document.createElement('div');
    const st = c.probe===true?'green':c.probe===false?'red':'gray';
    left.innerHTML = `<span class="led ${st}"></span>${connTitle(c)}`;
    const right = document.createElement('div');
    right.innerHTML = `
      <button class="btn sml" onclick="connectName('${c.name}')">Connect</button>
      <button class="btn sml" onclick="probeOne('${c.name}')">Probe</button>`;
    row.appendChild(left); row.appendChild(right);
    connList.appendChild(row);
  });
  connCount.textContent = String(connections.length);
}

function renderGroups(){
  groupList.innerHTML = "";
  groups.forEach(g=>{
    const row = document.createElement('div'); row.className='item'; row.id='g-'+g.name; row.onclick=()=>runOne(g.name);
    const left = document.createElement('div');
    left.innerHTML = `<span style="font-weight:600">${g.name}</span>
      ${g.connection?`<span class="pill">conn: ${g.connection}</span>`:''}
      <span class="pill">${g.ops} ops</span>
      ${g.has_expect?'<span class="pill">expect</span>':''}`;
    const right = document.createElement('div');
    right.innerHTML = `<span class="status" id="st-${g.name}">not run</span>`;
    row.appendChild(left); row.appendChild(right);
    groupList.appendChild(row);
  });
  groupsCount.textContent = String(groups.length);
}

function markGroup(name, pass, hasExpect){
  const st = document.getElementById('st-'+name);
  if(!st) return;
  st.className = 'status ' + ((hasExpect && pass===false)?'fail':'pass');
  st.textContent = (hasExpect ? (pass ? 'PASS' : 'FAIL') : 'OK');
}

function toggleFields(){
  const op = opType.value, fc = parseInt(opFC.value,10);
  // show value for write fc 5/6; show values for write fc 16
  const needsValue  = (op==='write' && (fc===5 || fc===6));
  const needsValues = (op==='write' && fc===16);
  valueField.style.display  = needsValue ? '' : 'none';
  valuesField.style.display = needsValues ? '' : 'none';
}
opType.addEventListener('change', toggleFields);
opFC.addEventListener('change', toggleFields);
toggleFields();

async function refresh(){
  const r = await fetch('/api/scan').then(r=>r.json()).catch(()=>null);
  if(!r || !r.ok){ setLed(false); return; }
  dev = r.device;
  devpill.textContent = `${dev.name||dev.type||'—'}: ${dev.detail} (unit ${dev.unit})`;
  setLed(r.connected===true);
}

async function loadConnections(){
  const fn = cfile.value.trim() || 'connections.json';
  const j = await fetch('/api/connections/load', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({filename:fn})}).then(r=>r.json());
  if(!j.ok){ alert(j.error||'load failed'); return; }
  connections = j.connections || [];
  renderConnections();
}

async function probeAll(){
  const j = await fetch('/api/connections/probe_all', {method:'POST'}).then(r=>r.json());
  if(!j.ok){ alert(j.error||'probe failed'); return; }
  connections = j.connections || [];
  renderConnections();
}
async function probeOne(name){
  const j = await fetch('/api/connections/probe', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({name})}).then(r=>r.json());
  if(!j.ok){ alert(j.error||'probe failed'); return; }
  const i = connections.findIndex(c=>c.name===name);
  if(i>=0) connections[i].probe = j.probe;
  renderConnections();
}
async function connectName(name){
  const j = await fetch('/api/connect', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({name})}).then(r=>r.json());
  if(!j.ok){ alert(j.error||'connect failed'); return; }
  dev = j.device;
  devpill.textContent = `${dev.name||dev.type||'—'}: ${dev.detail} (unit ${dev.unit})`;
  setLed(j.connected===true);
}

async function loadGroups(){
  const fn = gfile.value.trim() || 'mb_multi_groups.json';
  const j = await fetch('/api/groups/load', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({filename:fn})}).then(r=>r.json());
  if(!j.ok){ alert(j.error||'load failed'); return; }
  groups = j.groups || [];
  resultsByName = {};
  renderGroups();
  lastRunSummary.textContent = '—';
}

async function runOne(name){
  selectedName = name;
  currentGroup.textContent = "Group: " + name;
  const j = await fetch('/api/groups/run', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({name})}).then(r=>r.json());
  if(!j.ok){ alert(j.error||'run failed'); return; }
  resultsByName[name] = j;
  const meta = groups.find(x=>x.name===name);
  const hasExp = meta ? !!meta.has_expect : true;
  markGroup(name, j.group_pass!==false, hasExp);
  lastRunSummary.textContent = (j.group_pass!==false ? "PASS" : "FAIL") + ` (${name})`;
}
async function runAll(){
  const j = await fetch('/api/groups/run_all', {method:'POST'}).then(r=>r.json());
  if(!j.ok){ alert(j.error||'run all failed'); return; }
  let passCnt=0, failCnt=0;
  (j.results||[]).forEach(it=>{
    resultsByName[it.name] = it.result || {};
    const meta = groups.find(x=>x.name===it.name);
    const hasExp = meta ? !!meta.has_expect : true;
    markGroup(it.name, it.group_pass!==false, hasExp);
    if (it.group_pass!==false) passCnt++; else failCnt++;
  });
  lastRunSummary.textContent = `All: ${passCnt} pass, ${failCnt} fail`;
}
async function showSpec(){
  if(!selectedName){ detailBox.textContent = "Select a group first."; return; }
  const j = await fetch('/api/groups/item?name='+encodeURIComponent(selectedName)).then(r=>r.json());
  if(!j.ok){ detailBox.textContent = j.error||"fetch failed"; return; }
  detailBox.textContent = JSON.stringify(j.group, null, 2);
}
function showResult(){
  if(!selectedName){ detailBox.textContent = "Select a group first."; return; }
  const res = resultsByName[selectedName];
  detailBox.textContent = res ? JSON.stringify(res, null, 2) : "No result yet.";
}

function inferUnitFromSelection(){
  const name = opConn.value;
  if(!name){ if(dev) opUnit.value = dev.unit||1; return; }
  const c = connections.find(x=>x.name===name);
  if(c) opUnit.value = c.unit||1;
}
opConn.addEventListener('change', inferUnitFromSelection);

async function execOp(){
  opResult.textContent = "—";
  const body = {
    connection: opConn.value || undefined,
    unit: parseInt(opUnit.value||'1',10),
    op: opType.value,
    fc: parseInt(opFC.value,10),
    addr: opAddr.value.trim(),
    count: parseInt(opCount.value||'1',10)
  };
  if (opType.value==='write'){
    const fc = parseInt(opFC.value,10);
    if (fc===5 || fc===6){
      body["value"] = opValue.value.trim();
    } else if (fc===16){
      body["values"] = opValues.value.split(",").map(s=>s.trim()).filter(s=>s.length>0);
    }
  }
  const expTxt = opExpect.value.trim();
  if (expTxt){
    try{ body["expect"] = JSON.parse(expTxt); }catch(e){ alert("Bad expect JSON: " + e); return; }
  }
  const j = await fetch('/api/op/exec', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)}).then(r=>r.json());
  if (!j.ok){ opResult.textContent = JSON.stringify(j, null, 2); return; }
  opResult.textContent = JSON.stringify(j, null, 2);
}

refresh();
</script>
</body>
</html>
"""

# --------------------------- Flask app ---------------------------

def create_app():
    app = Flask(__name__)

    state: Dict[str, Any] = {
        "transport": None,
        "device": {"name": None, "type": None, "detail": "—", "unit": 1},
        "connected": False,
        "connections_file": "connections.json",
        "connections": [],
        "groups_file": "mb_multi_groups.json",
        "groups": []
    }

    # ---- helpers ----
    def build_transport(cfg: Dict[str, Any]) -> Optional[Transport]:
        t = cfg.get("type","tcp").lower()
        timeout = float(cfg.get("timeout", 0.7))
        if t == "tcp":
            return TcpTransport(cfg["host"], int(cfg.get("port",502)), timeout)
        if t == "rtu-tcp":
            return RtuTcpTransport(cfg["host"], int(cfg.get("port",502)), timeout)
        if t == "serial" and SerialRtuTransport is not None:
            return SerialRtuTransport(
                cfg["device"], int(cfg.get("baud",9600)),
                cfg.get("parity","N"), int(cfg.get("stopbits",1)),
                int(cfg.get("bytesize",8)), timeout
            )
        return None

    def probe(cfg: Dict[str, Any]) -> bool:
        tr = build_transport(cfg)
        if not tr:
            print( "no tr") 
            return False
        unit = int(cfg.get("unit", 1))
        try:
            tr.open()
            ok = True
            #vals = mb_read_bits(tr, unit, 1, 0, 1)  # cheap probe
            #ok = (vals != []) or (vals == [False])
            tr.close()
            return ok
        except Exception:
            try: tr.close()
            except: pass
            return False

    def conn_detail(cfg: Dict[str, Any]) -> str:
        t = cfg.get("type","tcp")
        if t in ("tcp","rtu-tcp"):
            return f"{cfg.get('host')}:{cfg.get('port',502)}"
        if t=="serial":
            return f"{cfg.get('device')}@{cfg.get('baud',9600)}"
        return "—"

    def apply_current(cfg: Dict[str, Any], tr: Transport, connected: bool):
        state["transport"] = tr
        state["device"] = {
            "name": cfg.get("name"),
            "type": cfg.get("type"),
            "detail": conn_detail(cfg),
            "unit": int(cfg.get("unit",1))
        }
        state["connected"] = connected

    def has_expect(g: Dict[str, Any]) -> bool:
        for op in g.get("ops", []):
            if isinstance(op, dict) and "expect" in op:
                return True
        return False

    def find_conn_by_name(name: str) -> Optional[Dict[str, Any]]:
        for c in state["connections"]:
            if c.get("name")==name: return c
        return None

    # ---- routes ----
    @app.get("/")
    def ui_index():
        return Response(INDEX_HTML, mimetype="text/html")

    @app.get("/api/scan")
    def api_scan():
        ok = False
        tr: Transport = state["transport"]
        if tr:
            try:
                vals = mb_read_bits(tr, state["device"]["unit"], 1, 0, 1)
                ok = (vals != []) or (vals == [False])
            except Exception:
                ok = False
        return jsonify(ok=True, device=state["device"], connected=ok)

    # Connections
    @app.post("/api/connections/load")
    def api_connections_load():
        body = request.get_json(silent=True) or {}
        fn = str(body.get("filename","")).strip() or state["connections_file"]
        try:
            with open(fn, "r", encoding="utf-8") as f:
                data = json.load(f)
            conns = data.get("connections", data if isinstance(data, list) else [])
            cleaned = []
            for c in conns:
                if isinstance(c, dict) and "name" in c and "type" in c:
                    cleaned.append(c)
            for c in cleaned: c["probe"] = None
            state["connections_file"] = fn
            state["connections"] = cleaned
            return jsonify(ok=True, file=fn, connections=state["connections"])
        except Exception as e:
            return jsonify(ok=False, error=f"load error: {e}"), 400

    @app.post("/api/connections/probe_all")
    def api_connections_probe_all():
        if not state["connections"]:
            return jsonify(ok=False, error="no connections loaded"), 400
        out = []
        for c in state["connections"]:
            ok = probe(c)
            cc = dict(c); cc["probe"] = bool(ok)
            out.append(cc)
        state["connections"] = out
        return jsonify(ok=True, connections=out)

    @app.post("/api/connections/probe")
    def api_connections_probe():
        body = request.get_json(silent=True) or {}
        name = str(body.get("name","")).strip()
        c = find_conn_by_name(name)
        if not c: return jsonify(ok=False, error="not found"), 404
        ok = probe(c)
        c["probe"] = bool(ok)
        return jsonify(ok=True, name=name, probe=bool(ok))

    @app.post("/api/connect")
    def api_connect():
        body = request.get_json(silent=True) or {}
        if "name" in body:
            c = find_conn_by_name(str(body["name"]))
            if not c: return jsonify(ok=False, error="connection not found"), 404
            tr = build_transport(c)
            if not tr: return jsonify(ok=False, error="unsupported transport"), 400
            try:
                tr.open()
                ok = probe(c)
            except Exception:
                ok = False
            apply_current(c, tr, ok)
            return jsonify(ok=True, device=state["device"], connected=ok)
        # ad-hoc inline
        t = str(body.get("type","tcp"))
        cfg = dict(body)
        cfg.setdefault("name", "adhoc")
        cfg.setdefault("unit", int(body.get("unit",1)))
        tr = build_transport(cfg)
        if not tr: return jsonify(ok=False, error="bad transport params"), 400
        try:
            tr.open(); ok = True
        except Exception:
            ok = False
        apply_current(cfg, tr, ok)
        return jsonify(ok=True, device=state["device"], connected=ok)

    # Groups
    @app.post("/api/groups/load")
    def api_groups_load():
        body = request.get_json(silent=True) or {}
        fn = str(body.get("filename","")).strip() or state["groups_file"]
        try:
            with open(fn, "r", encoding="utf-8") as f:
                data = json.load(f)
            groups = data.get("groups", data if isinstance(data, list) else [])
            cleaned = [g for g in groups if isinstance(g, dict) and "name" in g and "ops" in g]
            state["groups_file"] = fn
            state["groups"] = cleaned
            listing = [{
                "name": g["name"],
                "connection": g.get("connection"),
                "ops": len(g.get("ops", [])),
                "has_expect": has_expect(g)
            } for g in cleaned]
            return jsonify(ok=True, file=fn, groups=listing)
        except Exception as e:
            return jsonify(ok=False, error=f"load error: {e}"), 400

    @app.get("/api/groups/item")
    def api_groups_item():
        name = str(request.args.get("name","")).strip()
        g = next((x for x in state["groups"] if x.get("name")==name), None)
        if not g: return jsonify(ok=False, error="group not found"), 404
        return jsonify(ok=True, group=g)

    def run_group(g: Dict[str, Any]) -> Dict[str, Any]:
        conn_name = g.get("connection")
        if conn_name:
            c = find_conn_by_name(conn_name)
            if not c:
                return {"ok": False, "error": f"connection '{conn_name}' not found"}
            tr = build_transport(c)
            if not tr:
                return {"ok": False, "error": "unsupported transport"}
            try: tr.open()
            except Exception: pass
            unit = int(c.get("unit", 1))
        else:
            tr = state["transport"]
            if tr is None:
                return {"ok": False, "error": "no active connection"}
            unit = int(state["device"]["unit"])

        results = []
        all_ok = True
        for op in g.get("ops", []):
            if not isinstance(op, dict): continue
            op_type = str(op.get("op","read")).lower()
            fc   = int(op.get("fc", 3))
            addr = parse_int(op.get("addr", 0), 0)
            count = int(op.get("count", 1))
            value = op.get("value")
            values= op.get("values")
            if isinstance(values, str):
                values = [parse_int(x,0) for x in values.split(",") if x.strip()]
            res = exec_modbus(tr, unit, op_type, fc, addr, count=count, value=value, values=values)
            exp = op.get("expect")
            exp_pass = None; exp_report=None
            if op_type=="read" and exp is not None and res.get("ok") and isinstance(res.get("values"), list):
                chk = evaluate_expect(exp, res["values"], is_bits=(fc in (1,2)))
                exp_pass = bool(chk.get("pass")); exp_report = f"{chk.get('mode','')}: {chk.get('detail','')}"
                if exp_pass is False: all_ok = False
            results.append({"req":op, "res":res, "expect":exp, "expect_pass":exp_pass, "expect_report":exp_report})

        if conn_name and hasattr(tr,"close"):
            try: tr.close()
            except: pass

        return {"ok": True, "name": g.get("name"), "group_pass": all_ok, "results": results}

    @app.post("/api/groups/run")
    def api_groups_run():
        body = request.get_json(silent=True) or {}
        name = str(body.get("name","")).strip()
        g = next((x for x in state["groups"] if x.get("name")==name), None)
        if not g: return jsonify(ok=False, error="group not found"), 404
        out = run_group(g)
        return jsonify(**out)

    @app.post("/api/groups/run_all")
    def api_groups_run_all():
        if not state["groups"]:
            return jsonify(ok=False, error="no groups loaded"), 400
        out = []
        for g in state["groups"]:
            r = run_group(g)
            out.append({"name": g.get("name"), "group_pass": r.get("group_pass", False), "result": r})
        return jsonify(ok=True, results=out)

    # One-off operation
    @app.post("/api/op/exec")
    def api_op_exec():
        body = request.get_json(silent=True) or {}
        conn_name = body.get("connection")
        unit = int(body.get("unit", state["device"]["unit"]))
        op = str(body.get("op","read")).lower()
        fc = int(body.get("fc", 3))
        addr = parse_int(body.get("addr", 0), 0)
        count = int(body.get("count", 1))
        value = body.get("value", None)
        values = body.get("values", None)
        if isinstance(values, list):
            values = [parse_int(v,0) for v in values]
        elif isinstance(values, str):
            values = [parse_int(v,0) for v in values.split(",") if v.strip()]

        # choose transport
        tr = None
        close_after = False
        if conn_name:
            c = find_conn_by_name(conn_name)
            if not c: return jsonify(ok=False, error="connection not found"), 404
            tr = build_transport(c)
            if not tr: return jsonify(ok=False, error="unsupported transport"), 400
            try: tr.open()
            except Exception: pass
            unit = int(body.get("unit", c.get("unit",1)))
            close_after = True
        else:
            tr = state["transport"]
            if tr is None: return jsonify(ok=False, error="no active connection"), 400

        result = exec_modbus(tr, unit, op, fc, addr, count=count, value=parse_int(value,0) if value is not None else None, values=values)
        exp = body.get("expect")
        exp_out = None
        if op=="read" and exp is not None and result.get("ok") and isinstance(result.get("values"), list):
            exp_out = evaluate_expect(exp, result["values"], is_bits=(fc in (1,2)))

        if close_after and hasattr(tr,"close"):
            try: tr.close()
            except: pass

        return jsonify(ok=True, request={"connection":conn_name, "unit":unit, "op":op, "fc":fc, "addr":addr, "count":count, "value":value, "values":values, "expect":exp},
                       result=result, expect=exp_out)

    return app

# ------------------------------ Main -----------------------------

if __name__ == "__main__":
    bind = os.getenv("API_BIND", "0.0.0.0")
    port = int(os.getenv("API_PORT", "8099"))
    app = create_app()
    app.run(host=bind, port=port, debug=False)
