Right now your widget is probably calling `getJSON('/api/protections')` inside `tick()` or a timer, so it’s hammering the backend every second.

Given protections are basically static until **save** or **deploy**, you can switch to a **cache + explicit refresh** model:

### 1. Only fetch protections on:
- Widget init
- After **Save**
- After **Deploy**
- (Optionally) on a manual “Refresh” button

### 2. Don’t call the API from `tick()`

In your `ProtectionsWidget`:

#### Before (likely pattern)

```javascript
tick(seriesCache) {
  this.fetchProtections();   // ← too frequent
}
```

#### After (better)

```javascript
tick(seriesCache) {
  // For protections, nothing to do each tick – values are static.
  // You can still update any UI timeouts here if needed, but no network.
}
```

And make `fetchProtections()` only used from init + callbacks:

```javascript
class ProtectionsWidget extends BaseWidget {
  constructor(rootEl, config) {
    super(rootEl, config);
    this.protections = null;
    this.isLoading = false;
  }

  async init() {
    this.root.innerHTML = `<div class="prot-card">Loading protections...</div>`;
    await this.fetchProtections();   // ← one-time on init
  }

  async fetchProtections(source = "current") {
    if (this.isLoading) return;
    this.isLoading = true;
    try {
      const data = await getJSON(`/api/protections?source=${source}`);
      if (!data || !data.ok) throw new Error("Bad response");
      this.protections = data.protections;
      this.renderProtections();
    } catch (err) {
      console.error("Failed to fetch protections:", err);
      this.root.innerHTML = `<div class="prot-error">Failed to load protections</div>`;
    } finally {
      this.isLoading = false;
    }
  }
```

### 3. Wire recall / save / deploy to refresh the cache

Use `fetchProtections()` at the end of those operations instead of relying on `tick()`:

```javascript
async recallValues(source) {
  try {
    const data = await getJSON(`/api/protections?source=${source}`);
    if (data.ok && data.protections[this.editingVar]) {
      this.editingData = JSON.parse(JSON.stringify(data.protections[this.editingVar]));
      const modalBody = document.querySelector('.prot-modal-body');
      if (modalBody) {
        modalBody.innerHTML = this.renderEditForm(this.editingVar, this.editingData);
      }
    }
  } catch (err) {
    console.error('Failed to recall values:', err);
    alert('Failed to recall values');
  }
}

async saveProtections(deploy = false) {
  // ...collect inputs into payload...

  try {
    const endpoint = deploy ? '/api/protections/deploy' : '/api/protections';
    const result = await getJSON(endpoint, {
      method: 'POST',
      body: JSON.stringify(payload),
    });

    if (result.ok) {
      alert(deploy ? 'Protections deployed!' : 'Protections saved!');
      this.closeEditModal();

      // 🔁 refresh cached protections from current profile
      await this.fetchProtections("current");
    } else {
      alert('Failed to save protections');
    }
  } catch (err) {
    console.error('Failed to save protections:', err);
    alert('Failed to save protections');
  }
}
```

*(Note: adapt the `getJSON` call signature to match your helper – some versions don’t take `method`/`body`; if so, you may be using a `postJSON` helper instead. We can align that when you’re back.)*

---

When you’re back, if you paste:
- Your current `ProtectionsWidget.tick`
- Your `getJSON` helper implementation

I can:
- Fix the save/deploy paths
- Make sure we’re using the right helper (`getJSON` / `postJSON`)
- Confirm protections are only fetched on init + after save/deploy.