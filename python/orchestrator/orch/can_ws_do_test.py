#!/usr/bin/env python3
# can_ws_do_test_clean.py
#
# For each GPIO pattern (first byte of CAN data), send:
#   cansend can0 1857EDF4#<PATTERN><7x00>
# then fetch SOC-like value via WebSocket JSON (wscat), and read DIO inputs
# from a local HTTP API. Prints neat per-step blocks + a final summary table.

import subprocess
import time
import json
import re
import requests
from typing import Any, Dict, List, Optional, Tuple

# ---------------- Configuration ----------------
CAN_IF   = "can0"
CAN_ID   = "1857EDF4"          # 8 hex digits => extended frame (can-utils)
WAIT_SECS_AFTER_CAN = 0.30     # settle time before WS read

WS_HOST  = "192.168.86.20"
WS_PORT  = 9001
WS_SM    = "rtos"
WS_REG   = "mb_hold"
WS_OFF   = 101
WS_NUM   = 1
WS_NOTE  = "get module SOC"

# DIO API (your local service exposing inputs)
DIO_HOST = "192.168.86.46"
DIO_PORT = 8192

# Patterns to test (first CAN data byte)
PATTERNS = [0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x30,0x40,0x60,0x80,0x00]

# Optional expected DIO bitmaps per pattern (LSB first)
DIOS: Dict[int, List[bool]] = {
    0x00: [True, False, False, True, True, False, False, False],
    0x01: [True, False, False, True, True, True,  False, False],
    0x02: [False,False, False, True, True, True,  False, False],
    0x04: [False,False, False, True, True, True,  False, False],
    0x08: [True, False, False, True, True, False, False, False],
    0x10: [True, False, True,  True, True, False, False, False],
    0x20: [True, True,  False, True, True, False, False, False],
    0x30: [True, True,  True,  True, True, False, False, False],
    0x40: [True, False, False, True, True, False, False, False],
    0x60: [True, True,  False, True, True, False, False, False],
    0x80: [True, False, False, True, True, False, False, False],
}

# Optional overrides for WS expected value: {pattern: expected_value}
EXPECTED_OVERRIDES: Dict[int, Any] = {
    0x60: 160,
    0x80: 64,
    # 0x40: 128,
}

# Timeout for wscat (seconds)
WSCAT_TIMEOUT = 5.0

# ---------------- Helpers ----------------
def build_can_data(pattern: int, payload_len: int = 8, pad: int = 0x00) -> str:
    p = pattern & 0xFF
    rest = "".join(f"{pad & 0xFF:02X}" for _ in range(max(0, payload_len-1)))
    return f"{p:02X}{rest}"

def send_cansend(can_if: str, can_id: str, data_hex: str) -> Tuple[bool, str]:
    cmd = ["cansend", can_if, f"{can_id}#{data_hex}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode == 0:
        return True, ""
    return False, f"cansend rc={r.returncode}\nSTDERR:\n{r.stderr}\nSTDOUT:\n{r.stdout}"

def _extract_first_json(text: str) -> dict:
    for line in text.splitlines():
        if "{" in line and "}" in line:
            try:
                frag = line[line.find("{"):line.rfind("}")+1]
                return json.loads(frag)
            except Exception:
                pass
    m = re.search(r"\{.*\}", text, re.S)
    if m:
        return json.loads(m.group(0))
    raise ValueError("No JSON object found in wscat output")

def ws_get_data(host: str, port: int, sm_name: str, reg_type: str,
                offset: int, num: int, note: Optional[str] = None,
                seq: int = 124, timeout: float = WSCAT_TIMEOUT) -> Tuple[bool, Optional[List[Any]], str]:
    msg = {
        "action": "get",
        "seq": seq,
        "sm_name": sm_name,
        "reg_type": reg_type,
        "offset": offset,
        "num": num,
    }
    if note:
        msg["note"] = note

    cmd = ["wscat", "--no-color", "-c", f"ws://{host}:{port}", "-x", json.dumps(msg)]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return False, None, "wscat timeout"

    if r.returncode != 0:
        return False, None, f"wscat rc={r.returncode}\nSTDERR:\n{r.stderr}\nSTDOUT:\n{r.stdout}"

    try:
        obj = _extract_first_json(r.stdout)
    except Exception as e:
        return False, None, f"parse error: {e}\nSTDOUT:\n{r.stdout}"

    data = obj.get("data")
    if not isinstance(data, list):
        return False, None, f"JSON missing list `data`: {json.dumps(obj, indent=2)}"
    return True, data, ""

def dio_get_inputs(host: str, port: int) -> Tuple[bool, Optional[List[bool]], str]:
    url = f"http://{host}:{port}/api/inputs"
    try:
        resp = requests.get(url, timeout=3)
        resp.raise_for_status()
        j = resp.json()
        inputs = j.get("inputs")
        if not isinstance(inputs, list) or not all(isinstance(x, bool) for x in inputs):
            return False, None, f"bad JSON payload: {j}"
        return True, inputs, ""
    except Exception as e:
        return False, None, f"HTTP error: {e}"

def default_expected(pattern: int) -> Optional[int]:
    p = pattern & 0xFF
    if 0x00 <= p <= 0x3F:
        return 192 + p
    if p == 0x40:
        return 128
    return None

def compute_expected(pattern: int) -> Optional[Any]:
    if pattern in EXPECTED_OVERRIDES:
        return EXPECTED_OVERRIDES[pattern]
    return default_expected(pattern)

def bools_to_bits(b: List[bool]) -> str:
    # LSB first list -> show as b7..b0 for readability
    # If list has more than 8, still show all with MSB left
    return "".join("1" if x else "0" for x in reversed(b))

def checkmark(ok: bool) -> str:
    return "✅" if ok else "❌"

def hr(w: int = 72) -> str:
    return "-" * w

# ---------------- Main ----------------
if __name__ == "__main__":
    results = []  # collect per-pattern summary

    total = len(PATTERNS)
    print(hr())
    print(f"CAN/WS/DIO Test  |  CAN_IF={CAN_IF}  WS={WS_HOST}:{WS_PORT}  DIO={DIO_HOST}:{DIO_PORT}")
    print(hr())

    for idx, pat in enumerate(PATTERNS, 1):
        title = f"[{idx:02d}/{total:02d}] Pattern 0x{pat:02X} ({pat})"
        print(title)
        print(hr())

        # 1) CAN send
        data_hex = build_can_data(pat)
        can_ok, can_err = send_cansend(CAN_IF, CAN_ID, data_hex)
        print(f" CAN  : {CAN_ID}#{data_hex:<16} {checkmark(can_ok)}")
        if not can_ok:
            print(f"        -> {can_err.strip()}")
            results.append({"pat": pat, "can": False, "ws": False, "ws_val": None, "exp": None, "exp_ok": False, "dio": False})
            print(hr(), "\n", sep="")
            continue

        # 2) wait
        time.sleep(WAIT_SECS_AFTER_CAN)

        # 3) WS read
        ws_ok, ws_data, ws_err = ws_get_data(
            host=WS_HOST, port=WS_PORT,
            sm_name=WS_SM, reg_type=WS_REG,
            offset=WS_OFF, num=WS_NUM,
            note=WS_NOTE, seq=1000+idx
        )
        ws_val = ws_data[0] if (ws_ok and isinstance(ws_data, list) and len(ws_data) == 1) else ws_data
        if ws_ok:
            if isinstance(ws_val, list):
                disp = json.dumps(ws_val)
            else:
                disp = str(ws_val)
            print(f" WS   : off={WS_OFF} num={WS_NUM} -> {disp:<10} {checkmark(True)}")
        else:
            print(f" WS   : {checkmark(False)}")
            print(f"        -> {ws_err.strip()}")

        # 4) Expected WS value (if known)
        exp_val = compute_expected(pat)
        exp_ok = None
        if ws_ok and exp_val is not None and ws_val is not None:
            exp_ok = (ws_val == exp_val)
            print(f" EXP  : expect {exp_val} -> {checkmark(exp_ok)}")
        elif exp_val is None:
            print(f" EXP  : (no expectation)")
        else:
            print(f" EXP  : (skipped)")

        # 5) DIO inputs
        dio_ok, inputs, dio_err = dio_get_inputs(DIO_HOST, DIO_PORT)
        dio_expect = DIOS.get(pat)
        if dio_ok and inputs is not None:
            bits = bools_to_bits(inputs)
            if dio_expect is not None:
                dio_pass = (inputs[:len(dio_expect)] == dio_expect)
                exp_bits = bools_to_bits(dio_expect)
                print(f" DIO  : {bits}  vs exp {exp_bits}  {checkmark(dio_pass)}")
            else:
                dio_pass = None
                print(f" DIO  : {bits}  (no expectation)")
        else:
            dio_pass = False
            print(f" DIO  : {checkmark(False)}")
            print(f"        -> {dio_err.strip()}")

        results.append({
            "pat": pat, "can": can_ok,
            "ws": ws_ok, "ws_val": ws_val,
            "exp": exp_val, "exp_ok": (bool(exp_ok) if exp_ok is not None else None),
            "dio": (bool(dio_pass) if dio_pass is not None else None)
        })

        print(hr(), "\n", sep="")

    # -------- Summary --------
    print("\nSUMMARY")
    print(hr())
    # Header
    print(f"{'PAT':>4} | {'CAN':^5} | {'WS':^5} | {'VAL':>6} | {'EXP':>6} | {'WS=EXP':^7} | {'DIO':^5}")
    print("-"*4 + "-+-" + "-"*5 + "-+-" + "-"*5 + "-+-" + "-"*6 + "-+-" + "-"*6 + "-+-" + "-"*7 + "-+-" + "-"*5)
    for r in results:
        pat = r["pat"]
        can = checkmark(r["can"])
        ws  = checkmark(r["ws"])
        val = ("-" if r["ws_val"] is None else (str(r["ws_val"]) if not isinstance(r["ws_val"], list) else json.dumps(r["ws_val"])))
        exp = ("-" if r["exp"] is None else str(r["exp"]))
        wse = "-" if r["exp_ok"] is None else checkmark(r["exp_ok"])
        dio = "-" if r["dio"] is None else checkmark(r["dio"])
        print(f"0x{pat:02X} | {can:^5} | {ws:^5} | {val:>6} | {exp:>6} | {wse:^7} | {dio:^5}")
    print(hr())

# #!/usr/bin/env python3
# # can_ws_do_test.py
# #
# # For each GPIO pattern (first byte of CAN data), send:
# #   cansend can0 1857EDF4#<PATTERN><7x00>
# # then fetch SOC-like value via WebSocket JSON (wscat) and validate.

# import subprocess
# import time
# import json
# import re
# import requests

# from typing import Any, Dict, List, Optional, Tuple

# # ---------------- Configuration ----------------
# CAN_IF   = "can0"
# CAN_ID   = "1857EDF4"          # 8 hex digits => extended frame (can-utils)
# WAIT_SECS_AFTER_CAN = 0.3      # settle time before WS read

# WS_HOST  = "192.168.86.20"
# WS_PORT  = 9001
# WS_SM    = "rtos"
# WS_REG   = "mb_hold"
# WS_OFF   = 101
# WS_NUM   = 1
# WS_NOTE  = "get module SOC"

# # Patterns to test (first data byte)
# PATTERNS = [
#     0x00, 0x01, 0x02, 0x04, 0x08,
#     0x10, 0x20, 0x30, 0x40, 0x60, 0x80, 0x00
# ]

# DIOS = {
#     0x00: [True,  False, False,  True,  True, False, False, False],
#     0x01: [True,  False, False,  True,  True,  True, False, False],
#     0x02: [False, False, False,  True,  True,  True, False, False],
#     0x04: [False, False, False,  True,  True,  True, False, False],
#     0x08: [True,  False, False,  True,  True, False, False, False],
#     0x10: [True,  False,  True,  True,  True, False, False, False],
#     0x20: [True,   True, False,  True,  True, False, False, False],
#     0x30: [True,   True,  True,  True,  True, False, False, False],
#     0x40: [True,  False, False,  True,  True, False, False, False],
#     0x60: [True,   True, False,  True,  True, False, False, False],
#     0x80: [True,  False, False,  True,  True, False, False, False],
# }

# # Optional explicit expected overrides: {pattern: expected_value}
# # Leave empty to use default_expected() below.
# EXPECTED_OVERRIDES: Dict[int, Any] = {
#     # Example:
#     0x60: 160,
#     0x80: 64,
#     # 0x40: 128,
# }

# # Timeout for wscat (seconds)
# WSCAT_TIMEOUT = 5.0

# # ---------------- Helpers ----------------
# def build_can_data(pattern: int, payload_len: int = 8, pad: int = 0x00) -> str:
#     """
#     Build hex data string for cansend: first byte is pattern,
#     remaining bytes (payload_len-1) are pad.
#     """
#     pattern = pattern & 0xFF
#     pad = pad & 0xFF
#     rest = "".join(f"{pad:02X}" for _ in range(max(0, payload_len-1)))
#     return f"{pattern:02X}{rest}"

# def send_cansend(can_if: str, can_id: str, data_hex: str) -> None:
#     """
#     Send one CAN frame using `cansend`.
#     can_id must be 3 (std) or 8 (ext) hex chars for can-utils.
#     """
#     cmd = ["cansend", can_if, f"{can_id}#{data_hex}"]
#     r = subprocess.run(cmd, capture_output=True, text=True)
#     if r.returncode != 0:
#         raise RuntimeError(f"cansend failed (rc={r.returncode})\nSTDERR:\n{r.stderr}\nSTDOUT:\n{r.stdout}")

# def _extract_first_json(text: str) -> dict:
#     """
#     wscat often prints noise; pull the first {...} block and parse it.
#     """
#     # Fast path: line-wise search
#     for line in text.splitlines():
#         if "{" in line and "}" in line:
#             try:
#                 frag = line[line.find("{"):line.rfind("}")+1]
#                 return json.loads(frag)
#             except Exception:
#                 pass
#     # Fallback: greedy search
#     m = re.search(r"\{.*\}", text, re.S)
#     if m:
#         return json.loads(m.group(0))
#     raise ValueError("No JSON object found in wscat output")

# def ws_get_data(host: str, port: int, sm_name: str, reg_type: str,
#                 offset: int, num: int, note: Optional[str] = None,
#                 seq: int = 124, timeout: float = WSCAT_TIMEOUT) -> Tuple[List[Any], dict, str, str]:
#     """
#     Use `wscat` single-shot (-x) to send a JSON request and capture response.
#     Returns (data_list, full_reply_json, stdout, stderr)
#     """
#     msg = {
#         "action": "get",
#         "seq": seq,
#         "sm_name": sm_name,
#         "reg_type": reg_type,
#         "offset": offset,
#         "num": num,
#     }
#     if note:
#         msg["note"] = note

#     cmd = ["wscat", "--no-color", "-c", f"ws://{host}:{port}", "-x", json.dumps(msg)]
#     r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
#     if r.returncode != 0:
#         raise RuntimeError(f"wscat failed (rc={r.returncode})\nSTDERR:\n{r.stderr}\nSTDOUT:\n{r.stdout}")
#     obj = _extract_first_json(r.stdout)
#     if "data" not in obj or not isinstance(obj["data"], list):
#         raise ValueError(f"No list `data` in reply.\nJSON:\n{json.dumps(obj, indent=2)}\nSTDOUT:\n{r.stdout}")
#     return obj["data"], obj, r.stdout, r.stderr



# def dio_get_inputs(pat, host: str = "192.168.86.46", port: int = 8192):
#     """
#     Fetches the digital input/output (DIO) data from a local API.

#     The API endpoint is http://192.168.86.46:8192/api/inputs.
#     The response is expected to be in JSON format, for example:
#     {"inputs":[true,false,false,true,true,false,false,false],"ok":true}

#     This function extracts and returns the list of boolean values from the 'inputs' key.

#     Returns:
#         list: A list of booleans representing the digital input values.
#               Returns an empty list if the request fails or the data is malformed.
    
#     192  [True,  False, False,  True,  True, False,  False, False]
#     193  [True,  False, False,  True,  True,  True,  False, False]
#     194  [False, False, False,  True,  True,  True,  False, False]

#      192 [True, False, False, True, True, False, False, False]
#      193 [True, False, False, True, True, True, False, False]
#      194 [False, False, False, True, True, True, False, False]
#      196 [False, False, False, True, True, True, False, False]
#      200 [True, False, False, True, True, False, False, False]
#      208 [True, False, True, True, True, False, False, False]
#      224 [True, True, False, True, True, False, False, False]
#      240 [True, True, True, True, True, False, False, False]
    
#     128 [True, False, False, True, True, False, False, False]
#     160 [True, True, False, True, True, False, False, False] 
#     64 [True, False, False, True, True, False, False, False] 
#     192 [True, False, False, True, True, False, False, False]

#     """
    
#     api_url = f"http://{host}:{port}/api/inputs"
#     #api_url = "http://192.168.86.46:8192/api/inputs"
    
#     try:
#         # Make a GET request to the API
#         response = requests.get(api_url)
        
#         # Raise an exception for bad status codes (4xx or 5xx)
#         response.raise_for_status()
        
#         # Parse the JSON response
#         data = response.json()
        
#         # Extract the 'inputs' list. Using .get() prevents errors if the key is missing.
#         inputs = data.get("inputs", [])
#         if DIOS[pat] == inputs:
#             print(" PASS DIO pattern match ")     
#         else:
#             print(" FAIL no DIO pattern match ")     

#         return inputs
        
#     except requests.exceptions.RequestException as e:
#         # Catch any request-related errors (e.g., connection issues, timeouts)
#         print(f"Error making request to API: {e}")
#     except json.JSONDecodeError:
#         # Catch JSON parsing errors
#         print("Error decoding JSON from API response.")
    
#     # Return an empty list on any error
#     return []

# # Example usage:
# # if __name__ == '__main__':
# #     inputs_list = dio_get_data()
# #     print("Received inputs:", inputs_list)

# #     # You can then use this data in your program.
# #     # For example, to check the state of a specific input:
# #     if inputs_list:
# #         first_input_state = inputs_list[0]
# #         print(f"State of the first input is: {first_input_state}")


# def default_expected(pattern: int) -> Optional[int]:
#     """
#     Your described behavior:
#       - 0x00..0x3F map to 192..255 (i.e., 192 + pattern)
#       - 0x40 maps to 128
#       - others: unknown (None => skip check)
#     """
#     p = pattern & 0xFF
#     if 0x00 <= p <= 0x3F:
#         return 192 + p
#     if p == 0x40:
#         return 128
#     return None  # unknown / don't check

# def compute_expected(pattern: int) -> Optional[Any]:
#     if pattern in EXPECTED_OVERRIDES:
#         return EXPECTED_OVERRIDES[pattern]
#     return default_expected(pattern)

# # ---------------- Main ----------------
# if __name__ == "__main__":
#     for i, pat in enumerate(PATTERNS, 1):
#         print(f"\n=== Step {i}/{len(PATTERNS)} | CAN send pattern 0x{pat:02X} on {CAN_IF}, id {CAN_ID} ===")

#         # 1) send CAN frame
#         data_hex = build_can_data(pat, payload_len=8, pad=0x00)
#         try:
#             send_cansend(CAN_IF, CAN_ID, data_hex)
#             print(f"  CAN send     : {CAN_ID}#{data_hex}  ✅")
#         except Exception as e:
#             print(f"  CAN send     : ❌ {e}")
#             continue

#         # 2) wait a touch
#         time.sleep(WAIT_SECS_AFTER_CAN)

#         # 3) query via WebSocket for mb_hold @ offset=101
#         try:
#             data, reply, so, se = ws_get_data(
#                 host=WS_HOST, port=WS_PORT,
#                 sm_name=WS_SM, reg_type=WS_REG,
#                 offset=WS_OFF, num=WS_NUM,
#                 note=WS_NOTE, seq=124+i
#             )
#             # Print compact for num=1
#             if len(data) == 1:
#                 print(f"  WS data      : {data[0]}  (full {data})")
#             else:
#                 print(f"  WS data      : {data}")
#         except Exception as e:
#             print(f"  WS read      : ❌ {e}")
#             continue

#         # 4) check expected (if known)
#         exp = compute_expected(pat)
#         if exp is None:
#             print("  Expect       : (none / skipped)")
#         else:
#             got = data[0] if len(data) == 1 else data
#             ok = (got == exp)
#             print(f"  Expect       : {exp}")
#             print(f"  Check        : {'✅ PASS' if ok else '❌ FAIL'}")

#         # 5) query via WebSocket inpts
#         try:
#             inputs = dio_get_inputs(pat)
#         except Exception as e:
#             print(f"  could not get inputs      : ❌ {e}")
#             continue
#         print(f"  Inputs       : {inputs}")


