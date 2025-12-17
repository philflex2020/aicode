You’re right, the current `index.html` I gave you doesn’t have:

- The **Var List** selector (dropdown / option box)
- The **Refresh Lists** button

Let’s add those back in, wired to the backend, while keeping everything else as-is.

Below is a **drop-in replacement** for the relevant parts of your `index.html`:

- Updates the **Target tab toolbar** UI
- Adds functions for:
  - Loading the list of available var lists (`/api/alarms/target/var-lists`)
  - Selecting a var list and loading its variables (`/api/alarms/target/vars?list_name=...`)
  - Saving current table into the selected list (`POST /api/alarms/target/vars`)
  - Creating a new list (via typing into the dropdown and Save)
  - Refreshing the dropdown (`Refresh Lists`)

If your API endpoints are named differently, we can tweak the URLs after.

---

### 1. Replace Target Tab Toolbar HTML

Find this block in `index.html` (inside `#tab-target`):

```html
<div class="target-section">
    <h3>Variable Get/Set</h3>
    <div class="tab-controls">
        <button onclick="addTargetVar()">➕ Add Variable</button>
        <button onclick="saveTargetList()">💾 Save List</button>
        <button onclick="loadTargetList()">🔄 Load List</button>
        <span style="color: #666; font-size: 11px;">Type variable name or shared mem ref, then click Get/Set</span>
    </div>
    ...
</div>
```

Replace it with this, which adds the **Var List** selector and **Refresh Lists** button:

```html
<div class="target-section">
    <h3>Variable Get/Set</h3>
    <div class="tab-controls">
        <label style="display:flex; align-items:center; gap:4px; font-size:12px;">
            Var List:
            <select id="varListSelect" onchange="onVarListChange()" style="min-width:140px; font-size:12px;">
                <!-- options filled by JS -->
            </select>
        </label>
        <button onclick="refreshVarLists()">🔄 Refresh Lists</button>
        <button onclick="addTargetVar()">➕ Add Variable</button>
        <button onclick="saveTargetList()">💾 Save List</button>
        <span style="color: #666; font-size: 11px;">Select a list, edit, then Save List. Type variable name or shared mem ref, then click Get/Set.</span>
    </div>
    <table id="targetVarsTable">
        <thead>
            <tr>
                <th>Variable Name / Ref</th>
                <th>sm_name</th>
                <th>reg_type</th>
                <th>offset</th>
                <th>num</th>
                <th>write_data (JSON array)</th>
                <th>read_data (JSON array)</th>
                <th>Actions</th>
            </tr>
        </thead>
        <tbody>
            <!-- filled by JS -->
        </tbody>
    </table>
</div>
```

---

### 2. Add Var-List Logic in `<script>`

In your `<script>` block, we keep the existing target functions but **augment** them with list support.

#### 2.1. Add helper: current list name

Place this near the other helpers (e.g., after `setStatus`):

```js
function getCurrentVarListName() {
    const sel = document.getElementById('varListSelect');
    if (!sel) return null;
    const val = sel.value.trim();
    return val === '' ? null : val;
}
```

#### 2.2. Update `collectTargetVars()` to include list name (no change in structure sent to server yet; list_name will be added by save)

You can leave `collectTargetVars()` as-is if you already have it; just ensure it returns the array of row objects.

#### 2.3. New: load available var lists

Add:

```js
async function refreshVarLists() {
    setStatus('Refreshing variable lists...');
    try {
        // Adjust endpoint if needed
        const res = await fetch(apiBase + '/target/var-lists');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();

        const lists = data.lists || data.var_lists || []; // be forgiving on key name

        const sel = document.getElementById('varListSelect');
        sel.innerHTML = '';

        // Option: blank to create new name by typing then Save List
        const blankOpt = document.createElement('option');
        blankOpt.value = '';
        blankOpt.textContent = '-- new list --';
        sel.appendChild(blankOpt);

        lists.forEach(name => {
            const opt = document.createElement('option');
            opt.value = name;
            opt.textContent = name;
            sel.appendChild(opt);
        });

        setStatus('Variable lists refreshed.', false);

        // Optionally auto-load first list if any
        if (lists.length > 0) {
            sel.value = lists[0];
            await loadTargetList();
        }
    } catch (err) {
        console.error(err);
        setStatus('Error refreshing lists: ' + err.message, true);
    }
}
```

#### 2.4. New: when list selection changes

```js
async function onVarListChange() {
    const name = getCurrentVarListName();
    if (!name) {
        // New list mode: clear table but keep one default row maybe
        renderTargetVars([]);
        setStatus('New list selected. Add variables and click "Save List".', false);
        return;
    }
    await loadTargetList();  // reuse existing loader but make it list-aware
}
```

#### 2.5. Update `loadTargetList()` to use selected list

Replace your current `loadTargetList()` with:

```js
async function loadTargetList() {
    const listName = getCurrentVarListName();
    if (!listName) {
        // if no list name, just clear
        renderTargetVars([]);
        setStatus('No list selected. Choose a list or create a new one.', false);
        return;
    }

    setStatus('Loading target variable list "' + listName + '"...');
    try {
        // GET /target/vars?list_name=...
        const res = await fetch(apiBase + '/target/vars?list_name=' + encodeURIComponent(listName));
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const data = await res.json();

        // Expecting data.variables; adjust if your API returns another key
        renderTargetVars(data.variables || []);
        setStatus('Target variable list "' + listName + '" loaded.', false);
    } catch (err) {
        console.error(err);
        setStatus('Error loading variable list: ' + err.message, true);
    }
}
```

#### 2.6. Update `saveTargetList()` to send list name

Replace your existing `saveTargetList()` with:

```js
async function saveTargetList() {
    const vars = collectTargetVars();
    const listName = getCurrentVarListName();

    // if current selection is blank, let user type a new name into the select box
    const sel = document.getElementById('varListSelect');
    const rawValue = sel.value.trim();
    const finalName = rawValue || prompt('Enter a name for this variable list:', '');

    if (!finalName) {
        setStatus('Save cancelled: no list name.', true);
        return;
    }

    sel.value = finalName;  // reflect chosen name in dropdown

    setStatus('Saving target variable list "' + finalName + '"...');
    try {
        // Adjust payload/endpoint as needed
        const res = await fetch(apiBase + '/target/vars', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                list_name: finalName,
                variables: vars
            })
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || ('HTTP ' + res.status));

        setStatus('Target variable list "' + finalName + '" saved.', false);

        // Make sure list dropdown is up to date
        await refreshVarLists();
        document.getElementById('varListSelect').value = finalName;
    } catch (err) {
        console.error(err);
        setStatus('Error saving variable list: ' + err.message, true);
    }
}
```

---

### 3. Init: call `refreshVarLists()` on load

At the bottom of the script, where you currently have:

```js
// Initial load
loadFromDB();
loadTargetConfig();
loadTargetList();
```

Change to:

```js
// Initial load
loadFromDB();
loadTargetConfig();
refreshVarLists();  // will also auto-load first list if present
```

---

### 4. What I’m Assuming About Your API

I’m assuming these endpoints exist (or will):

- `GET  /api/alarms/target/var-lists`  
  → `{"lists": ["default", "debug_set1", ...]}`

- `GET  /api/alarms/target/vars?list_name=default`  
  → `{"variables": [ ... rows ... ]}`

- `POST /api/alarms/target/vars` with body:  
  `{"list_name": "default", "variables": [ ... ]}`  
  → `{"message": "ok"}`

If your actual keys or URLs are different, tell me exactly what your backend returns and I’ll align the JS to it.

---

If you like, paste your current `target`-related API endpoints from `api_endpoints.py`, and I’ll match the front-end exactly against them.