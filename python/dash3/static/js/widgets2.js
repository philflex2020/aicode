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
    const rack = (this.cfg.rack != null ? this.cfg.rack : 0);
    if (!names) return;
    
    const rsp = await getJSON('/vars?names=' + encodeURIComponent(names) + '&rack=' + rack);
    this.tbody.innerHTML = '';
    
    for (const nm of (this.cfg.names || [])) {
      const currVal = rsp[nm];
      const displayVal = Array.isArray(currVal) ? currVal.join(', ') : (currVal == null ? '—' : currVal);
      
      // Store default if not already set
      if (!(nm in this.defaults)) {
        this.defaults[nm] = currVal;
      }
      
      const tr = document.createElement('tr');
      
      // Name column
      const tdName = document.createElement('td');
      tdName.textContent = nm;
      tr.appendChild(tdName);
      
      // New Value input column
      const tdNew = document.createElement('td');
      const input = document.createElement('input');
      input.type = 'text';
      input.className = 'var-input';
      input.id = `var_new_${nm}`;
      input.placeholder = 'Enter new value';
      input.style.width = '100%';
      input.style.background = '#0b1126';
      input.style.color = '#e8eef6';
      input.style.border = '1px solid #1e2a43';
      input.style.borderRadius = '4px';
      input.style.padding = '4px 6px';
      input.style.fontSize = '12px';
      tdNew.appendChild(input);
      tr.appendChild(tdNew);
      
      // Current Value column
      const tdCurr = document.createElement('td');
      tdCurr.id = `var_curr_${nm}`;
      tdCurr.textContent = displayVal;
      tr.appendChild(tdCurr);
      
      // Action button column
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
      // POST to /vars endpoint with the new value
      const payload = { [name]: newValue };
      await postJSON('/vars', payload);
      
      // Refresh to show updated current value
      await this.tick();
      
      // Clear the input after successful send
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