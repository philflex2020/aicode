


 Fractal Dashboard v6 Manual Test Steps

---

## Step 1: Connect Behavior
- Stop any running WebSocket server.
- Open `test_runner.html`.
- EXPECT: Dashboard loads fakeData automatically (tabs show: connect, config, param, rack0, ...).

---

## Step 2: Tab Switching
- Click on "rack0" tab.
- Edit a field or a table value.
- Click on "rack1" tab, then back to "rack0".
- EXPECT: Edited value is still there (NOT reset).

---

## Step 3: Input Field Buttons
- On "config" tab, manually edit a field.
- Click [Clear] ➔ input should become empty.
- Click [Default] ➔ input should reset to default value.
- Click [Set] ➔ Value should apply (no crash).

---

## Step 4: Reload Button
- Click [Reload] button.
- EXPECT: Current frame redraws based on the latest live data (no crash).

---

## Step 5: Table Switch (Volt / SOC / SOH / Temp)
- On "rack0", use the table selector dropdown.
- Switch between Volt ➔ SOC ➔ SOH ➔ Temp.
- EXPECT: Table updates cleanly, no page reload needed, no crash.

---

## Step 6: Manual WebSocket Connection
- Start `mock_ws_server.js` locally (`node mock_ws_server.js`).
- Go to "connect" tab.
- Enter IP: `localhost` and Port: `8080`.
- Click [Connect].
- EXPECT: Dashboard reloads with live WebSocket data (WS Field, WS Param).

---

# ✅ If all steps pass, FractalDashboard v6 is field-verified and ready for industrial use.