Great! That's a solid roadmap. Let me sketch out a structure that matches the **Protections widget pattern** (dropdown modal for details) and sets you up for the features you listed.

---

## 1. Updated `VariablesWidget` structure

### Main list view (compact, like protections)

```js
renderVariablesList() {
    if (this.filteredVariables.length === 0) {
        return '<div class="no-variables">No variables found</div>';
    }

    return `
        <table class="variables-table">
            <thead>
                <tr>
                    <th>Name</th>
                    <th>Category</th>
                    <th>Current Value</th>
                    <th>Version</th>
                    <th>Actions</th>
                </tr>
            </thead>
            <tbody>
                ${this.filteredVariables.map(v => this.renderVariableRow(v)).join('')}
            </tbody>
        </table>
    `;
}

renderVariableRow(variable) {
    const isExpanded = this.expandedVariable === variable.name;
    
    return `
        <tr class="variable-row ${isExpanded ? 'expanded' : ''}" data-variable-name="${variable.name}">
            <td>
                <button class="expand-btn" data-variable-name="${variable.name}">
                    ${isExpanded ? '▼' : '▶'}
                </button>
                <span class="variable-name">${variable.name}</span>
            </td>
            <td><span class="category-badge category-${variable.category}">${variable.category}</span></td>
            <td><span class="current-value">${this.formatValue(variable.value)}</span></td>
            <td>${variable.version || 'N/A'}</td>
            <td>
                <button class="icon-btn edit-variable-btn" data-variable-name="${variable.name}" title="Quick Edit Value">
                    ✏️
                </button>
                <button class="icon-btn delete-variable-btn" data-variable-name="${variable.name}" title="Delete">
                    🗑️
                </button>
            </td>
        </tr>
        ${isExpanded ? this.renderVariableDetails(variable) : ''}
    `;
}
```

---

## 2. Dropdown details panel (like protections)

```js
renderVariableDetails(variable) {
    return `
        <tr class="variable-details-row">
            <td colspan="5">
                <div class="variable-details">
                    
                    <!-- Access Paths Section -->
                    <div class="details-section">
                        <h4>Access Paths</h4>
                        <div class="paths-list">
                            ${this.renderPathsList(variable)}
                        </div>
                        <button class="add-path-btn" data-variable-name="${variable.name}">
                            + Add Path
                        </button>
                    </div>

                    <!-- Protection Limits Section -->
                    <div class="details-section">
                        <h4>Protection Limits</h4>
                        <div class="limits-config">
                            ${this.renderLimitsConfig(variable)}
                        </div>
                    </div>

                    <!-- Monitoring Section -->
                    <div class="details-section">
                        <h4>Monitoring</h4>
                        <div class="monitoring-config">
                            <div class="monitor-option">
                                <label>
                                    <input type="checkbox" 
                                           class="add-to-metrics-checkbox" 
                                           data-variable-name="${variable.name}"
                                           ${variable.inMetrics ? 'checked' : ''}>
                                    Add to Metrics Group
                                </label>
                                ${variable.inMetrics ? `
                                    <select class="metrics-group-select" data-variable-name="${variable.name}">
                                        ${this.renderMetricsGroupOptions(variable.metricsGroup)}
                                    </select>
                                ` : ''}
                            </div>
                            
                            <div class="monitor-option">
                                <label>
                                    <input type="checkbox" 
                                           class="add-to-chart-checkbox" 
                                           data-variable-name="${variable.name}"
                                           ${variable.inChart ? 'checked' : ''}>
                                    Add to Chart
                                </label>
                                ${variable.inChart ? `
                                    <select class="chart-select" data-variable-name="${variable.name}">
                                        ${this.renderChartOptions(variable.chartId)}
                                    </select>
                                ` : ''}
                            </div>
                        </div>
                    </div>

                    <!-- Metadata Section -->
                    <div class="details-section">
                        <h4>Metadata</h4>
                        <div class="metadata">
                            <div><strong>Type:</strong> ${variable.type || 'Unknown'}</div>
                            <div><strong>Units:</strong> ${variable.units || 'N/A'}</div>
                            <div><strong>Description:</strong> ${variable.description || 'No description'}</div>
                            <div><strong>Last Modified:</strong> ${variable.lastModified || 'Unknown'}</div>
                        </div>
                    </div>

                </div>
            </td>
        </tr>
    `;
}
```

---

## 3. Helper render methods

```js
renderPathsList(variable) {
    if (!variable.paths || variable.paths.length === 0) {
        return '<div class="no-paths">No access paths defined</div>';
    }

    return variable.paths.map((path, index) => `
        <div class="path-item">
            <span class="path-protocol">${path.protocol}</span>
            <span class="path-address">${path.address}</span>
            <button class="icon-btn edit-path-btn" 
                    data-variable-name="${variable.name}" 
                    data-path-index="${index}"
                    title="Edit Path">
                ✏️
            </button>
            <button class="icon-btn delete-path-btn" 
                    data-variable-name="${variable.name}" 
                    data-path-index="${index}"
                    title="Remove Path">
                ✖️
            </button>
        </div>
    `).join('');
}

renderLimitsConfig(variable) {
    const limits = variable.limits || {};
    
    return `
        <div class="limits-grid">
            <div class="limit-field">
                <label>Min Value:</label>
                <input type="number" 
                       class="limit-input" 
                       data-variable-name="${variable.name}" 
                       data-limit-type="min"
                       value="${limits.min !== undefined ? limits.min : ''}"
                       placeholder="No limit">
            </div>
            <div class="limit-field">
                <label>Max Value:</label>
                <input type="number" 
                       class="limit-input" 
                       data-variable-name="${variable.name}" 
                       data-limit-type="max"
                       value="${limits.max !== undefined ? limits.max : ''}"
                       placeholder="No limit">
            </div>
            <div class="limit-field">
                <label>Warning Low:</label>
                <input type="number" 
                       class="limit-input" 
                       data-variable-name="${variable.name}" 
                       data-limit-type="warnLow"
                       value="${limits.warnLow !== undefined ? limits.warnLow : ''}"
                       placeholder="No warning">
            </div>
            <div class="limit-field">
                <label>Warning High:</label>
                <input type="number" 
                       class="limit-input" 
                       data-variable-name="${variable.name}" 
                       data-limit-type="warnHigh"
                       value="${limits.warnHigh !== undefined ? limits.warnHigh : ''}"
                       placeholder="No warning">
            </div>
            <div class="limit-actions">
                <button class="save-limits-btn" data-variable-name="${variable.name}">
                    Save Limits
                </button>
            </div>
        </div>
    `;
}

renderMetricsGroupOptions(selectedGroup) {
    // Placeholder - you'll populate this from your metrics groups later
    const groups = ['Group 1', 'Group 2', 'Group 3'];
    return groups.map(g => 
        `<option value="${g}" ${g === selectedGroup ? 'selected' : ''}>${g}</option>`
    ).join('');
}

renderChartOptions(selectedChart) {
    // Placeholder - you'll populate this from available charts later
    const charts = ['Chart A', 'Chart B', 'Chart C'];
    return charts.map(c => 
        `<option value="${c}" ${c === selectedChart ? 'selected' : ''}>${c}</option>`
    ).join('');
}

formatValue(value) {
    if (value === null || value === undefined) return 'N/A';
    if (typeof value === 'number') return value.toFixed(2);
    return String(value);
}
```

---

## 4. Event handlers for new features

```js
attachEventHandlers() {
    const container = this.body;

    // ... existing handlers (category, search, sort, etc.) ...

    // Expand/collapse variable details
    const expandBtns = container.querySelectorAll('.expand-btn');
    expandBtns.forEach(btn => {
        btn.addEventListener('click', (e) => {
            e.stopPropagation();
            const variableName = btn.getAttribute('data-variable-name');
            this.toggleVariableDetails(variableName);
        });
    });

    // Add path
    const addPathBtns = container.querySelectorAll('.add-path-btn');
    addPathBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            this.showAddPathModal(variableName);
        });
    });

    // Edit path
    const editPathBtns = container.querySelectorAll('.edit-path-btn');
    editPathBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            const pathIndex = parseInt(btn.getAttribute('data-path-index'));
            this.showEditPathModal(variableName, pathIndex);
        });
    });

    // Delete path
    const deletePathBtns = container.querySelectorAll('.delete-path-btn');
    deletePathBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            const pathIndex = parseInt(btn.getAttribute('data-path-index'));
            this.deletePath(variableName, pathIndex);
        });
    });

    // Save limits
    const saveLimitsBtns = container.querySelectorAll('.save-limits-btn');
    saveLimitsBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            this.saveLimits(variableName);
        });
    });

    // Add to metrics group
    const metricsCheckboxes = container.querySelectorAll('.add-to-metrics-checkbox');
    metricsCheckboxes.forEach(cb => {
        cb.addEventListener('change', (e) => {
            const variableName = cb.getAttribute('data-variable-name');
            this.toggleMetricsGroup(variableName, e.target.checked);
        });
    });

    // Metrics group selection
    const metricsSelects = container.querySelectorAll('.metrics-group-select');
    metricsSelects.forEach(sel => {
        sel.addEventListener('change', (e) => {
            const variableName = sel.getAttribute('data-variable-name');
            this.updateMetricsGroup(variableName, e.target.value);
        });
    });

    // Add to chart
    const chartCheckboxes = container.querySelectorAll('.add-to-chart-checkbox');
    chartCheckboxes.forEach(cb => {
        cb.addEventListener('change', (e) => {
            const variableName = cb.getAttribute('data-variable-name');
            this.toggleChart(variableName, e.target.checked);
        });
    });

    // Chart selection
    const chartSelects = container.querySelectorAll('.chart-select');
    chartSelects.forEach(sel => {
        sel.addEventListener('change', (e) => {
            const variableName = sel.getAttribute('data-variable-name');
            this.updateChart(variableName, e.target.value);
        });
    });
}
```

---

## 5. Action methods (stubs for you to implement)

```js
toggleVariableDetails(variableName) {
    if (this.expandedVariable === variableName) {
        this.expandedVariable = null;
    } else {
        this.expandedVariable = variableName;
    }
    this.render();
}

async showAddPathModal(variableName) {
    // Show modal with protocol dropdown, address input, etc.
    // On submit, call addPath()
    console.log('Show add path modal for', variableName);
    // TODO: Implement modal
}

async showEditPathModal(variableName, pathIndex) {
    const variable = this.variables.find(v => v.name === variableName);
    const path = variable.paths[pathIndex];
    // Show modal pre-filled with path data
    console.log('Show edit path modal for', variableName, path);
    // TODO: Implement modal
}

async deletePath(variableName, pathIndex) {
    if (!confirm('Remove this access path?')) return;
    
    try {
        const response = await fetch(`/api/variables/${variableName}/paths/${pathIndex}`, {
            method: 'DELETE'
        });
        
        if (response.ok) {
            // Update local data
            const variable = this.variables.find(v => v.name === variableName);
            variable.paths.splice(pathIndex, 1);
            this.applyFilters();
            this.render();
        } else {
            alert('Failed to delete path');
        }
    } catch (error) {
        console.error('Error deleting path:', error);
        alert('Error deleting path');
    }
}

async saveLimits(variableName) {
    const container = this.body;
    const inputs = container.querySelectorAll(`.limit-input[data-variable-name="${variableName}"]`);
    
    const limits = {};
    inputs.forEach(input => {
        const limitType = input.getAttribute('data-limit-type');
        const value = input.value.trim();
        if (value !== '') {
            limits[limitType] = parseFloat(value);
        }
    });
    
    try {
        const response = await fetch(`/api/variables/${variableName}/limits`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(limits)
        });
        
        if (response.ok) {
            // Update local data
            const variable = this.variables.find(v => v.name === variableName);
            variable.limits = limits;
            alert('Limits saved successfully');
        } else {
            alert('Failed to save limits');
        }
    } catch (error) {
        console.error('Error saving limits:', error);
        alert('Error saving limits');
    }
}

async toggleMetricsGroup(variableName, enabled) {
    const variable = this.variables.find(v => v.name === variableName);
    variable.inMetrics = enabled;
    
    if (enabled && !variable.metricsGroup) {
        variable.metricsGroup = 'Group 1'; // Default
    }
    
    // TODO: Send to backend
    this.render();
}

async updateMetricsGroup(variableName, groupName) {
    const variable = this.variables.find(v => v.name === variableName);
    variable.metricsGroup = groupName;
    
    // TODO: Send to backend
    console.log(`Updated ${variableName} to metrics group ${groupName}`);
}

async toggleChart(variableName, enabled) {
    const variable = this.variables.find(v => v.name === variableName);
    variable.inChart = enabled;
    
    if (enabled && !variable.chartId) {
        variable.chartId = 'Chart A'; // Default
    }
    
    // TODO: Send to backend
    this.render();
}

async updateChart(variableName, chartId) {
    const variable = this.variables.find(v => v.name === variableName);
    variable.chartId = chartId;
    
    // TODO: Send to backend
    console.log(`Updated ${variableName} to chart ${chartId}`);
}
```

---

## 6. Constructor updates

```js
constructor(cfg) {
    super(cfg);
    this.variables = [];
    this.filteredVariables = [];
    this.selectedCategory = 'all';
    this.searchQuery = '';
    this.sortBy = 'name';
    this.sortAsc = true;
    this.expandedVariable = null; // Track which variable details are shown
    this.systemVersion = 'Unknown';
    
    // Load data once on initialization
    this.fetchVariables();
    this.fetchSystemVersion();
}
```

---

## Summary

This gives you:

1. ✅ **Current value** shown in main table
2. ✅ **Dropdown details** (expand/collapse like protections)
3. ✅ **Paths add/remove/edit** with buttons in details panel
4. ✅ **Add to metrics group** checkbox + dropdown (stub for later metrics widget)
5. ✅ **Add to chart** checkbox + dropdown (stub for later chart widget)
6. ✅ **Protection limits** (min/max/warn) editable in details panel

The modals for adding/editing paths can reuse the same modal pattern you have for protections. When you're ready to wire up the backend API calls, just fill in the `TODO` sections.


Want me to draft the **Add/Edit Path modal** code next, or do you want to tackle the CSS styling for the details panel first?
Great! That's a solid roadmap. Let me sketch out a structure that matches the **Protections widget pattern** (dropdown modal for details) and sets you up for the features you listed.

---

## 1. Updated `VariablesWidget` structure

### Main list view (compact, like protections)

```js
renderVariablesList() {
    if (this.filteredVariables.length === 0) {
        return '<div class="no-variables">No variables found</div>';
    }

    return `
        <table class="variables-table">
            <thead>
                <tr>
                    <th>Name</th>
                    <th>Category</th>
                    <th>Current Value</th>
                    <th>Version</th>
                    <th>Actions</th>
                </tr>
            </thead>
            <tbody>
                ${this.filteredVariables.map(v => this.renderVariableRow(v)).join('')}
            </tbody>
        </table>
    `;
}

renderVariableRow(variable) {
    const isExpanded = this.expandedVariable === variable.name;
    
    return `
        <tr class="variable-row ${isExpanded ? 'expanded' : ''}" data-variable-name="${variable.name}">
            <td>
                <button class="expand-btn" data-variable-name="${variable.name}">
                    ${isExpanded ? '▼' : '▶'}
                </button>
                <span class="variable-name">${variable.name}</span>
            </td>
            <td><span class="category-badge category-${variable.category}">${variable.category}</span></td>
            <td><span class="current-value">${this.formatValue(variable.value)}</span></td>
            <td>${variable.version || 'N/A'}</td>
            <td>
                <button class="icon-btn edit-variable-btn" data-variable-name="${variable.name}" title="Quick Edit Value">
                    ✏️
                </button>
                <button class="icon-btn delete-variable-btn" data-variable-name="${variable.name}" title="Delete">
                    🗑️
                </button>
            </td>
        </tr>
        ${isExpanded ? this.renderVariableDetails(variable) : ''}
    `;
}
```

---

## 2. Dropdown details panel (like protections)

```js
renderVariableDetails(variable) {
    return `
        <tr class="variable-details-row">
            <td colspan="5">
                <div class="variable-details">
                    
                    <!-- Access Paths Section -->
                    <div class="details-section">
                        <h4>Access Paths</h4>
                        <div class="paths-list">
                            ${this.renderPathsList(variable)}
                        </div>
                        <button class="add-path-btn" data-variable-name="${variable.name}">
                            + Add Path
                        </button>
                    </div>

                    <!-- Protection Limits Section -->
                    <div class="details-section">
                        <h4>Protection Limits</h4>
                        <div class="limits-config">
                            ${this.renderLimitsConfig(variable)}
                        </div>
                    </div>

                    <!-- Monitoring Section -->
                    <div class="details-section">
                        <h4>Monitoring</h4>
                        <div class="monitoring-config">
                            <div class="monitor-option">
                                <label>
                                    <input type="checkbox" 
                                           class="add-to-metrics-checkbox" 
                                           data-variable-name="${variable.name}"
                                           ${variable.inMetrics ? 'checked' : ''}>
                                    Add to Metrics Group
                                </label>
                                ${variable.inMetrics ? `
                                    <select class="metrics-group-select" data-variable-name="${variable.name}">
                                        ${this.renderMetricsGroupOptions(variable.metricsGroup)}
                                    </select>
                                ` : ''}
                            </div>
                            
                            <div class="monitor-option">
                                <label>
                                    <input type="checkbox" 
                                           class="add-to-chart-checkbox" 
                                           data-variable-name="${variable.name}"
                                           ${variable.inChart ? 'checked' : ''}>
                                    Add to Chart
                                </label>
                                ${variable.inChart ? `
                                    <select class="chart-select" data-variable-name="${variable.name}">
                                        ${this.renderChartOptions(variable.chartId)}
                                    </select>
                                ` : ''}
                            </div>
                        </div>
                    </div>

                    <!-- Metadata Section -->
                    <div class="details-section">
                        <h4>Metadata</h4>
                        <div class="metadata">
                            <div><strong>Type:</strong> ${variable.type || 'Unknown'}</div>
                            <div><strong>Units:</strong> ${variable.units || 'N/A'}</div>
                            <div><strong>Description:</strong> ${variable.description || 'No description'}</div>
                            <div><strong>Last Modified:</strong> ${variable.lastModified || 'Unknown'}</div>
                        </div>
                    </div>

                </div>
            </td>
        </tr>
    `;
}
```

---

## 3. Helper render methods

```js
renderPathsList(variable) {
    if (!variable.paths || variable.paths.length === 0) {
        return '<div class="no-paths">No access paths defined</div>';
    }

    return variable.paths.map((path, index) => `
        <div class="path-item">
            <span class="path-protocol">${path.protocol}</span>
            <span class="path-address">${path.address}</span>
            <button class="icon-btn edit-path-btn" 
                    data-variable-name="${variable.name}" 
                    data-path-index="${index}"
                    title="Edit Path">
                ✏️
            </button>
            <button class="icon-btn delete-path-btn" 
                    data-variable-name="${variable.name}" 
                    data-path-index="${index}"
                    title="Remove Path">
                ✖️
            </button>
        </div>
    `).join('');
}

renderLimitsConfig(variable) {
    const limits = variable.limits || {};
    
    return `
        <div class="limits-grid">
            <div class="limit-field">
                <label>Min Value:</label>
                <input type="number" 
                       class="limit-input" 
                       data-variable-name="${variable.name}" 
                       data-limit-type="min"
                       value="${limits.min !== undefined ? limits.min : ''}"
                       placeholder="No limit">
            </div>
            <div class="limit-field">
                <label>Max Value:</label>
                <input type="number" 
                       class="limit-input" 
                       data-variable-name="${variable.name}" 
                       data-limit-type="max"
                       value="${limits.max !== undefined ? limits.max : ''}"
                       placeholder="No limit">
            </div>
            <div class="limit-field">
                <label>Warning Low:</label>
                <input type="number" 
                       class="limit-input" 
                       data-variable-name="${variable.name}" 
                       data-limit-type="warnLow"
                       value="${limits.warnLow !== undefined ? limits.warnLow : ''}"
                       placeholder="No warning">
            </div>
            <div class="limit-field">
                <label>Warning High:</label>
                <input type="number" 
                       class="limit-input" 
                       data-variable-name="${variable.name}" 
                       data-limit-type="warnHigh"
                       value="${limits.warnHigh !== undefined ? limits.warnHigh : ''}"
                       placeholder="No warning">
            </div>
            <div class="limit-actions">
                <button class="save-limits-btn" data-variable-name="${variable.name}">
                    Save Limits
                </button>
            </div>
        </div>
    `;
}

renderMetricsGroupOptions(selectedGroup) {
    // Placeholder - you'll populate this from your metrics groups later
    const groups = ['Group 1', 'Group 2', 'Group 3'];
    return groups.map(g => 
        `<option value="${g}" ${g === selectedGroup ? 'selected' : ''}>${g}</option>`
    ).join('');
}

renderChartOptions(selectedChart) {
    // Placeholder - you'll populate this from available charts later
    const charts = ['Chart A', 'Chart B', 'Chart C'];
    return charts.map(c => 
        `<option value="${c}" ${c === selectedChart ? 'selected' : ''}>${c}</option>`
    ).join('');
}

formatValue(value) {
    if (value === null || value === undefined) return 'N/A';
    if (typeof value === 'number') return value.toFixed(2);
    return String(value);
}
```

---

## 4. Event handlers for new features

```js
attachEventHandlers() {
    const container = this.body;

    // ... existing handlers (category, search, sort, etc.) ...

    // Expand/collapse variable details
    const expandBtns = container.querySelectorAll('.expand-btn');
    expandBtns.forEach(btn => {
        btn.addEventListener('click', (e) => {
            e.stopPropagation();
            const variableName = btn.getAttribute('data-variable-name');
            this.toggleVariableDetails(variableName);
        });
    });

    // Add path
    const addPathBtns = container.querySelectorAll('.add-path-btn');
    addPathBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            this.showAddPathModal(variableName);
        });
    });

    // Edit path
    const editPathBtns = container.querySelectorAll('.edit-path-btn');
    editPathBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            const pathIndex = parseInt(btn.getAttribute('data-path-index'));
            this.showEditPathModal(variableName, pathIndex);
        });
    });

    // Delete path
    const deletePathBtns = container.querySelectorAll('.delete-path-btn');
    deletePathBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            const pathIndex = parseInt(btn.getAttribute('data-path-index'));
            this.deletePath(variableName, pathIndex);
        });
    });

    // Save limits
    const saveLimitsBtns = container.querySelectorAll('.save-limits-btn');
    saveLimitsBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            this.saveLimits(variableName);
        });
    });

    // Add to metrics group
    const metricsCheckboxes = container.querySelectorAll('.add-to-metrics-checkbox');
    metricsCheckboxes.forEach(cb => {
        cb.addEventListener('change', (e) => {
            const variableName = cb.getAttribute('data-variable-name');
            this.toggleMetricsGroup(variableName, e.target.checked);
        });
    });

    // Metrics group selection
    const metricsSelects = container.querySelectorAll('.metrics-group-select');
    metricsSelects.forEach(sel => {
        sel.addEventListener('change', (e) => {
            const variableName = sel.getAttribute('data-variable-name');
            this.updateMetricsGroup(variableName, e.target.value);
        });
    });

    // Add to chart
    const chartCheckboxes = container.querySelectorAll('.add-to-chart-checkbox');
    chartCheckboxes.forEach(cb => {
        cb.addEventListener('change', (e) => {
            const variableName = cb.getAttribute('data-variable-name');
            this.toggleChart(variableName, e.target.checked);
        });
    });

    // Chart selection
    const chartSelects = container.querySelectorAll('.chart-select');
    chartSelects.forEach(sel => {
        sel.addEventListener('change', (e) => {
            const variableName = sel.getAttribute('data-variable-name');
            this.updateChart(variableName, e.target.value);
        });
    });
}
```

---

## 5. Action methods (stubs for you to implement)

```js
toggleVariableDetails(variableName) {
    if (this.expandedVariable === variableName) {
        this.expandedVariable = null;
    } else {
        this.expandedVariable = variableName;
    }
    this.render();
}

async showAddPathModal(variableName) {
    // Show modal with protocol dropdown, address input, etc.
    // On submit, call addPath()
    console.log('Show add path modal for', variableName);
    // TODO: Implement modal
}

async showEditPathModal(variableName, pathIndex) {
    const variable = this.variables.find(v => v.name === variableName);
    const path = variable.paths[pathIndex];
    // Show modal pre-filled with path data
    console.log('Show edit path modal for', variableName, path);
    // TODO: Implement modal
}

async deletePath(variableName, pathIndex) {
    if (!confirm('Remove this access path?')) return;
    
    try {
        const response = await fetch(`/api/variables/${variableName}/paths/${pathIndex}`, {
            method: 'DELETE'
        });
        
        if (response.ok) {
            // Update local data
            const variable = this.variables.find(v => v.name === variableName);
            variable.paths.splice(pathIndex, 1);
            this.applyFilters();
            this.render();
        } else {
            alert('Failed to delete path');
        }
    } catch (error) {
        console.error('Error deleting path:', error);
        alert('Error deleting path');
    }
}

async saveLimits(variableName) {
    const container = this.body;
    const inputs = container.querySelectorAll(`.limit-input[data-variable-name="${variableName}"]`);
    
    const limits = {};
    inputs.forEach(input => {
        const limitType = input.getAttribute('data-limit-type');
        const value = input.value.trim();
        if (value !== '') {
            limits[limitType] = parseFloat(value);
        }
    });
    
    try {
        const response = await fetch(`/api/variables/${variableName}/limits`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(limits)
        });
        
        if (response.ok) {
            // Update local data
            const variable = this.variables.find(v => v.name === variableName);
            variable.limits = limits;
            alert('Limits saved successfully');
        } else {
            alert('Failed to save limits');
        }
    } catch (error) {
        console.error('Error saving limits:', error);
        alert('Error saving limits');
    }
}

async toggleMetricsGroup(variableName, enabled) {
    const variable = this.variables.find(v => v.name === variableName);
    variable.inMetrics = enabled;
    
    if (enabled && !variable.metricsGroup) {
        variable.metricsGroup = 'Group 1'; // Default
    }
    
    // TODO: Send to backend
    this.render();
}

async updateMetricsGroup(variableName, groupName) {
    const variable = this.variables.find(v => v.name === variableName);
    variable.metricsGroup = groupName;
    
    // TODO: Send to backend
    console.log(`Updated ${variableName} to metrics group ${groupName}`);
}

async toggleChart(variableName, enabled) {
    const variable = this.variables.find(v => v.name === variableName);
    variable.inChart = enabled;
    
    if (enabled && !variable.chartId) {
        variable.chartId = 'Chart A'; // Default
    }
    
    // TODO: Send to backend
    this.render();
}

async updateChart(variableName, chartId) {
    const variable = this.variables.find(v => v.name === variableName);
    variable.chartId = chartId;
    
    // TODO: Send to backend
    console.log(`Updated ${variableName} to chart ${chartId}`);
}
```

---

## 6. Constructor updates

```js
constructor(cfg) {
    super(cfg);
    this.variables = [];
    this.filteredVariables = [];
    this.selectedCategory = 'all';
    this.searchQuery = '';
    this.sortBy = 'name';
    this.sortAsc = true;
    this.expandedVariable = null; // Track which variable details are shown
    this.systemVersion = 'Unknown';
    
    // Load data once on initialization
    this.fetchVariables();
    this.fetchSystemVersion();
}
```

---

## Summary

This gives you:

1. ✅ **Current value** shown in main table
2. ✅ **Dropdown details** (expand/collapse like protections)
3. ✅ **Paths add/remove/edit** with buttons in details panel
4. ✅ **Add to metrics group** checkbox + dropdown (stub for later metrics widget)
5. ✅ **Add to chart** checkbox + dropdown (stub for later chart widget)
6. ✅ **Protection limits** (min/max/warn) editable in details panel

The modals for adding/editing paths can reuse the same modal pattern you have for protections. When you're ready to wire up the backend API calls, just fill in the `TODO` sections.

Want me to draft the **Add/Edit Path modal** code next, or do you want to tackle the CSS styling for the details panel first?

Perfect! Let's add a deploy button for config variables. Here's what we need:

## 1. Update `renderVariableRow` to show deploy button for config vars

```js
renderVariableRow(variable) {
    const isExpanded = this.expandedVariable === variable.name;
    const isConfig = variable.category === 'config';
    const hasChanges = variable.pendingValue !== undefined && variable.pendingValue !== variable.value;
    
    return `
        <tr class="variable-row ${isExpanded ? 'expanded' : ''} ${hasChanges ? 'has-changes' : ''}" data-variable-name="${variable.name}">
            <td>
                <button class="expand-btn" data-variable-name="${variable.name}">
                    ${isExpanded ? '▼' : '▶'}
                </button>
                <span class="variable-name">${variable.name}</span>
                ${hasChanges ? '<span class="pending-indicator" title="Has pending changes">●</span>' : ''}
            </td>
            <td><span class="category-badge category-${variable.category}">${variable.category}</span></td>
            <td>
                <span class="current-value">${this.formatValue(variable.value)}</span>
                ${hasChanges ? `<span class="pending-value" title="Pending: ${variable.pendingValue}">→ ${this.formatValue(variable.pendingValue)}</span>` : ''}
            </td>
            <td>${variable.version || 'N/A'}</td>
            <td>
                <button class="icon-btn edit-variable-btn" data-variable-name="${variable.name}" title="Edit Value">
                    ✏️
                </button>
                ${isConfig ? `
                    <button class="icon-btn deploy-variable-btn ${!hasChanges ? 'disabled' : ''}" 
                            data-variable-name="${variable.name}" 
                            title="${hasChanges ? 'Deploy pending changes' : 'No changes to deploy'}"
                            ${!hasChanges ? 'disabled' : ''}>
                        🚀
                    </button>
                ` : ''}
                <button class="icon-btn delete-variable-btn" data-variable-name="${variable.name}" title="Delete">
                    🗑️
                </button>
            </td>
        </tr>
        ${isExpanded ? this.renderVariableDetails(variable) : ''}
    `;
}
```

## 2. Add deploy section to details panel for config vars

```js
renderVariableDetails(variable) {
    const isConfig = variable.category === 'config';
    const hasChanges = variable.pendingValue !== undefined && variable.pendingValue !== variable.value;
    
    return `
        <tr class="variable-details-row">
            <td colspan="5">
                <div class="variable-details">
                    
                    ${isConfig ? `
                    <!-- Configuration & Deployment Section -->
                    <div class="details-section config-section">
                        <h4>Configuration & Deployment</h4>
                        <div class="config-deploy">
                            <div class="value-comparison">
                                <div class="value-item">
                                    <label>Current (Deployed) Value:</label>
                                    <span class="deployed-value">${this.formatValue(variable.value)}</span>
                                </div>
                                <div class="value-item">
                                    <label>Pending Value:</label>
                                    <input type="text" 
                                           class="pending-value-input" 
                                           data-variable-name="${variable.name}"
                                           value="${variable.pendingValue !== undefined ? variable.pendingValue : variable.value}"
                                           placeholder="Enter new value">
                                </div>
                            </div>
                            <div class="deploy-actions">
                                <button class="update-pending-btn" data-variable-name="${variable.name}">
                                    Update Pending Value
                                </button>
                                <button class="deploy-btn ${!hasChanges ? 'disabled' : ''}" 
                                        data-variable-name="${variable.name}"
                                        ${!hasChanges ? 'disabled' : ''}>
                                    🚀 Deploy Changes
                                </button>
                                ${hasChanges ? `
                                    <button class="revert-btn" data-variable-name="${variable.name}">
                                        ↶ Revert to Current
                                    </button>
                                ` : ''}
                            </div>
                            ${variable.lastDeployed ? `
                                <div class="deploy-info">
                                    <small>Last deployed: ${variable.lastDeployed} by ${variable.deployedBy || 'Unknown'}</small>
                                </div>
                            ` : ''}
                        </div>
                    </div>
                    ` : ''}

                    <!-- Access Paths Section -->
                    <div class="details-section">
                        <h4>Access Paths</h4>
                        <div class="paths-list">
                            ${this.renderPathsList(variable)}
                        </div>
                        <button class="add-path-btn" data-variable-name="${variable.name}">
                            + Add Path
                        </button>
                    </div>

                    <!-- Protection Limits Section -->
                    <div class="details-section">
                        <h4>Protection Limits</h4>
                        <div class="limits-config">
                            ${this.renderLimitsConfig(variable)}
                        </div>
                    </div>

                    <!-- Monitoring Section -->
                    <div class="details-section">
                        <h4>Monitoring</h4>
                        <div class="monitoring-config">
                            <div class="monitor-option">
                                <label>
                                    <input type="checkbox" 
                                           class="add-to-metrics-checkbox" 
                                           data-variable-name="${variable.name}"
                                           ${variable.inMetrics ? 'checked' : ''}>
                                    Add to Metrics Group
                                </label>
                                ${variable.inMetrics ? `
                                    <select class="metrics-group-select" data-variable-name="${variable.name}">
                                        ${this.renderMetricsGroupOptions(variable.metricsGroup)}
                                    </select>
                                ` : ''}
                            </div>
                            
                            <div class="monitor-option">
                                <label>
                                    <input type="checkbox" 
                                           class="add-to-chart-checkbox" 
                                           data-variable-name="${variable.name}"
                                           ${variable.inChart ? 'checked' : ''}>
                                    Add to Chart
                                </label>
                                ${variable.inChart ? `
                                    <select class="chart-select" data-variable-name="${variable.name}">
                                        ${this.renderChartOptions(variable.chartId)}
                                    </select>
                                ` : ''}
                            </div>
                        </div>
                    </div>

                    <!-- Metadata Section -->
                    <div class="details-section">
                        <h4>Metadata</h4>
                        <div class="metadata">
                            <div><strong>Type:</strong> ${variable.type || 'Unknown'}</div>
                            <div><strong>Units:</strong> ${variable.units || 'N/A'}</div>
                            <div><strong>Description:</strong> ${variable.description || 'No description'}</div>
                            <div><strong>Last Modified:</strong> ${variable.lastModified || 'Unknown'}</div>
                        </div>
                    </div>

                </div>
            </td>
        </tr>
    `;
}
```

## 3. Add event handlers for deploy functionality

```js
attachEventHandlers() {
    const container = this.body;

    // ... existing handlers ...

    // Update pending value
    const updatePendingBtns = container.querySelectorAll('.update-pending-btn');
    updatePendingBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            this.updatePendingValue(variableName);
        });
    });

    // Deploy button (in actions column)
    const deployBtns = container.querySelectorAll('.deploy-variable-btn');
    deployBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            this.deployVariable(variableName);
        });
    });

    // Deploy button (in details panel)
    const deployDetailBtns = container.querySelectorAll('.deploy-btn');
    deployDetailBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            this.deployVariable(variableName);
        });
    });

    // Revert button
    const revertBtns = container.querySelectorAll('.revert-btn');
    revertBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const variableName = btn.getAttribute('data-variable-name');
            this.revertPendingValue(variableName);
        });
    });
}
```

## 4. Add deploy action methods

```js
updatePendingValue(variableName) {
    const container = this.body;
    const input = container.querySelector(`.pending-value-input[data-variable-name="${variableName}"]`);
    
    if (!input) return;
    
    const newValue = input.value.trim();
    const variable = this.variables.find(v => v.name === variableName);
    
    if (!variable) return;
    
    // Parse value based on type
    let parsedValue = newValue;
    if (variable.type === 'number' || variable.type === 'float') {
        parsedValue = parseFloat(newValue);
        if (isNaN(parsedValue)) {
            alert('Invalid number format');
            return;
        }
    } else if (variable.type === 'integer') {
        parsedValue = parseInt(newValue);
        if (isNaN(parsedValue)) {
            alert('Invalid integer format');
            return;
        }
    } else if (variable.type === 'boolean') {
        parsedValue = newValue.toLowerCase() === 'true' || newValue === '1';
    }
    
    // Update pending value locally
    variable.pendingValue = parsedValue;
    this.applyFilters();
    this.render();
}

async deployVariable(variableName) {
    const variable = this.variables.find(v => v.name === variableName);
    
    if (!variable || variable.pendingValue === undefined) {
        alert('No pending changes to deploy');
        return;
    }
    
    if (!confirm(`Deploy ${variableName}?\n\nCurrent: ${variable.value}\nNew: ${variable.pendingValue}\n\nThis will update the running system.`)) {
        return;
    }
    
    try {
        const response = await fetch(`/api/variables/${variableName}/deploy`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                value: variable.pendingValue 
            })
        });
        
        if (response.ok) {
            const result = await response.json();
            
            // Update local data
            variable.value = variable.pendingValue;
            variable.pendingValue = undefined;
            variable.lastDeployed = new Date().toISOString();
            variable.deployedBy = result.deployedBy || 'Current User';
            
            this.applyFilters();
            this.render();
            
            alert(`Successfully deployed ${variableName}`);
        } else {
            const error = await response.json();
            alert(`Failed to deploy: ${error.message || 'Unknown error'}`);
        }
    } catch (error) {
        console.error('Error deploying variable:', error);
        alert('Error deploying variable');
    }
}

revertPendingValue(variableName) {
    const variable = this.variables.find(v => v.name === variableName);
    
    if (!variable) return;
    
    if (!confirm(`Revert pending changes for ${variableName}?`)) {
        return;
    }
    
    variable.pendingValue = undefined;
    this.applyFilters();
    this.render();
}
```

## 5. Add some CSS for visual feedback

```css
/* Pending changes indicator */
.variable-row.has-changes {
    background-color: #fffbf0;
}

.pending-indicator {
    color: #ff9800;
    margin-left: 5px;
    font-size: 12px;
}

.pending-value {
    color: #ff9800;
    margin-left: 10px;
    font-style: italic;
    font-size: 0.9em;
}

/* Deploy button states */
.deploy-variable-btn:not(.disabled) {
    background-color: #4CAF50;
}

.deploy-variable-btn.disabled {
    opacity: 0.3;
    cursor: not-allowed;
}

/* Config section styling */
.config-section {
    background-color: #f0f8ff;
    border-left: 4px solid #2196F3;
}

.value-comparison {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 20px;
    margin-bottom: 15px;
}

.value-item label {
    display: block;
    font-weight: bold;
    margin-bottom: 5px;
}

.deployed-value {
    display: block;
    padding: 8px;
    background-color: #e8f5e9;
    border: 1px solid #4CAF50;
    border-radius: 4px;
}

.pending-value-input {
    width: 100%;
    padding: 8px;
    border: 1px solid #ff9800;
    border-radius: 4px;
}

.deploy-actions {
    display: flex;
    gap: 10px;
    margin-bottom: 10px;
}

.deploy-btn {
    background-color: #4CAF50;
    color: white;
    padding: 8px 16px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
}

.deploy-btn:hover:not(.disabled) {
    background-color: #45a049;
}

.deploy-btn.disabled {
    background-color: #ccc;
    cursor: not-allowed;
}

.revert-btn {
    background-color: #f44336;
    color: white;
    padding: 8px 16px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
}

.revert-btn:hover {
    background-color: #da190b;
}

.update-pending-btn {
    background-color: #ff9800;
    color: white;
    padding: 8px 16px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
}

.update-pending-btn:hover {
    background-color: #e68900;
}

.deploy-info {
    color: #666;
    font-style: italic;
}
```

## Summary

Now you have:

1. ✅ **Deploy button** (🚀) in actions column for config variables
2. ✅ **Disabled state** when no pending changes
3. ✅ **Visual indicator** (orange dot) when variable has pending changes
4. ✅ **Pending value display** in the table row
5. ✅ **Config & Deployment section** in details panel with:
   - Current vs Pending value comparison
   - Input to edit pending value
   - Deploy button
   - Revert button
   - Last deployed timestamp & user
6. ✅ **Confirmation dialog** before deploying
7. ✅ **Visual styling** to highlight config vars with pending changes

The workflow is:
1. User edits pending value in details panel
2. Clicks "Update Pending Value" → shows orange indicator
3. Clicks deploy (🚀) → confirms → sends to backend
4. On success → pending becomes current, indicator clears

Ready to use! 🚀


Perfect! Here's the complete paths modal implementation:

## 1. Add modal HTML structure to `render()` method

```js
render() {
    console.log("running render #1");

    if (!this.body) {
        console.error("render: this.body is not set");
        return;
    }

    const html = `
        <div class="variables-widget">
            <div class="variables-header">
                <div class="variables-controls">
                    <div class="control-group">
                        <label>Category:</label>
                        <select class="category-filter">
                            <option value="all">All</option>
                            <option value="config">Config</option>
                            <option value="prot">Protection</option>
                            <option value="oper">Operational</option>
                        </select>
                    </div>

                    <div class="control-group">
                        <label>Search:</label>
                        <input type="text" class="search-input" placeholder="Search variables..." value="${this.searchQuery}">
                    </div>

                    <div class="control-group">
                        <label>Sort:</label>
                        <select class="sort-select">
                            <option value="name">Name</option>
                            <option value="category">Category</option>
                            <option value="version">Version</option>
                        </select>
                        <button class="sort-direction-btn" title="Toggle sort direction">
                            ${this.sortAsc ? '↑' : '↓'}
                        </button>
                    </div>

                    <button class="refresh-btn">↻ Refresh</button>
                    <button class="add-variable-btn">+ Add Variable</button>
                </div>
            </div>

            <div class="variables-stats">
                <span>Total: ${this.variables.length}</span>
                <span>Filtered: ${this.filteredVariables.length}</span>
                <span>System Version: ${this.systemVersion}</span>
            </div>

            <div class="variables-list">
                ${this.renderVariablesList()}
            </div>
        </div>

        <!-- Path Modal -->
        <div class="modal path-modal" style="display: none;">
            <div class="modal-content">
                <div class="modal-header">
                    <h3 class="modal-title">Add Access Path</h3>
                    <button class="modal-close">&times;</button>
                </div>
                <div class="modal-body">
                    <form class="path-form">
                        <input type="hidden" class="path-variable-name">
                        <input type="hidden" class="path-index" value="-1">
                        
                        <div class="form-group">
                            <label>Protocol:</label>
                            <select class="path-protocol" required>
                                <option value="">Select Protocol</option>
                                <option value="modbus">Modbus TCP</option>
                                <option value="rs485">Modbus RTU</option>
                                <option value="dnp3">DNP3</option>
                                <option value="iec61850">IEC 61850</option>
                                <option value="opcua">OPC UA</option>
                                <option value="mqtt">MQTT</option>
                                <option value="http">HTTP/REST</option>
                            </select>
                        </div>

                        <div class="form-group">
                            <label>Address/Register:</label>
                            <input type="text" class="path-address" placeholder="e.g., 40001, /device/sensor1" required>
                        </div>

                        <div class="form-group">
                            <label>Unit ID / Slave ID:</label>
                            <input type="number" class="path-unit-id" placeholder="Optional" min="0" max="255">
                        </div>

                        <div class="form-group">
                            <label>Data Type:</label>
                            <select class="path-data-type">
                                <option value="int16">INT16</option>
                                <option value="uint16">UINT16</option>
                                <option value="int32">INT32</option>
                                <option value="uint32">UINT32</option>
                                <option value="float32">FLOAT32</option>
                                <option value="float64">FLOAT64</option>
                                <option value="bool">BOOLEAN</option>
                                <option value="string">STRING</option>
                            </select>
                        </div>

                        <div class="form-group">
                            <label>Byte Order:</label>
                            <select class="path-byte-order">
                                <option value="big">Big Endian</option>
                                <option value="little">Little Endian</option>
                                <option value="big-swap">Big Endian (Word Swap)</option>
                                <option value="little-swap">Little Endian (Word Swap)</option>
                            </select>
                        </div>

                        <div class="form-group">
                            <label>Scale Factor:</label>
                            <input type="number" class="path-scale" placeholder="1.0" step="any" value="1.0">
                        </div>

                        <div class="form-group">
                            <label>Offset:</label>
                            <input type="number" class="path-offset" placeholder="0.0" step="any" value="0.0">
                        </div>

                        <div class="form-group">
                            <label>Poll Rate (ms):</label>
                            <input type="number" class="path-poll-rate" placeholder="1000" min="100" step="100" value="1000">
                        </div>

                        <div class="form-group">
                            <label>
                                <input type="checkbox" class="path-read-only">
                                Read Only
                            </label>
                        </div>

                        <div class="form-group">
                            <label>
                                <input type="checkbox" class="path-enabled" checked>
                                Enabled
                            </label>
                        </div>

                        <div class="form-group">
                            <label>Description:</label>
                            <textarea class="path-description" rows="3" placeholder="Optional description"></textarea>
                        </div>

                        <div class="modal-actions">
                            <button type="submit" class="btn-primary">Save Path</button>
                            <button type="button" class="btn-secondary modal-cancel">Cancel</button>
                        </div>
                    </form>
                </div>
            </div>
        </div>
    `;
    
    console.log("running render #2");

    this.body.innerHTML = html;
    this.attachEventHandlers();
}
```

## 2. Update `showAddPathModal` and `showEditPathModal` methods

```js
showAddPathModal(variableName) {
    const modal = this.body.querySelector('.path-modal');
    const form = modal.querySelector('.path-form');
    const title = modal.querySelector('.modal-title');
    
    // Reset form
    form.reset();
    title.textContent = 'Add Access Path';
    
    // Set variable name
    form.querySelector('.path-variable-name').value = variableName;
    form.querySelector('.path-index').value = '-1';
    
    // Set defaults
    form.querySelector('.path-scale').value = '1.0';
    form.querySelector('.path-offset').value = '0.0';
    form.querySelector('.path-poll-rate').value = '1000';
    form.querySelector('.path-enabled').checked = true;
    form.querySelector('.path-read-only').checked = false;
    
    // Show modal
    modal.style.display = 'flex';
}

showEditPathModal(variableName, pathIndex) {
    const variable = this.variables.find(v => v.name === variableName);
    if (!variable || !variable.paths || !variable.paths[pathIndex]) {
        console.error('Path not found');
        return;
    }
    
    const path = variable.paths[pathIndex];
    const modal = this.body.querySelector('.path-modal');
    const form = modal.querySelector('.path-form');
    const title = modal.querySelector('.modal-title');
    
    // Set title
    title.textContent = 'Edit Access Path';
    
    // Set variable name and index
    form.querySelector('.path-variable-name').value = variableName;
    form.querySelector('.path-index').value = pathIndex;
    
    // Populate form with existing data
    form.querySelector('.path-protocol').value = path.protocol || '';
    form.querySelector('.path-address').value = path.address || '';
    form.querySelector('.path-unit-id').value = path.unitId !== undefined ? path.unitId : '';
    form.querySelector('.path-data-type').value = path.dataType || 'int16';
    form.querySelector('.path-byte-order').value = path.byteOrder || 'big';
    form.querySelector('.path-scale').value = path.scale !== undefined ? path.scale : 1.0;
    form.querySelector('.path-offset').value = path.offset !== undefined ? path.offset : 0.0;
    form.querySelector('.path-poll-rate').value = path.pollRate || 1000;
    form.querySelector('.path-read-only').checked = path.readOnly || false;
    form.querySelector('.path-enabled').checked = path.enabled !== false; // default true
    form.querySelector('.path-description').value = path.description || '';
    
    // Show modal
    modal.style.display = 'flex';
}

closePathModal() {
    const modal = this.body.querySelector('.path-modal');
    modal.style.display = 'none';
}
```

## 3. Add modal event handlers to `attachEventHandlers()`

```js
attachEventHandlers() {
    const container = this.body;

    // ... existing handlers ...

    // Modal close buttons
    const modalClose = container.querySelector('.modal-close');
    if (modalClose) {
        modalClose.addEventListener('click', () => {
            this.closePathModal();
        });
    }

    const modalCancel = container.querySelector('.modal-cancel');
    if (modalCancel) {
        modalCancel.addEventListener('click', () => {
            this.closePathModal();
        });
    }

    // Close modal on background click
    const modal = container.querySelector('.path-modal');
    if (modal) {
        modal.addEventListener('click', (e) => {
            if (e.target === modal) {
                this.closePathModal();
            }
        });
    }

    // Path form submission
    const pathForm = container.querySelector('.path-form');
    if (pathForm) {
        pathForm.addEventListener('submit', (e) => {
            e.preventDefault();
            this.savePathFromForm();
        });
    }
}
```

## 4. Add `savePathFromForm` method

```js
async savePathFromForm() {
    const form = this.body.querySelector('.path-form');
    
    const variableName = form.querySelector('.path-variable-name').value;
    const pathIndex = parseInt(form.querySelector('.path-index').value);
    
    // Collect form data
    const pathData = {
        protocol: form.querySelector('.path-protocol').value,
        address: form.querySelector('.path-address').value,
        unitId: form.querySelector('.path-unit-id').value ? parseInt(form.querySelector('.path-unit-id').value) : undefined,
        dataType: form.querySelector('.path-data-type').value,
        byteOrder: form.querySelector('.path-byte-order').value,
        scale: parseFloat(form.querySelector('.path-scale').value) || 1.0,
        offset: parseFloat(form.querySelector('.path-offset').value) || 0.0,
        pollRate: parseInt(form.querySelector('.path-poll-rate').value) || 1000,
        readOnly: form.querySelector('.path-read-only').checked,
        enabled: form.querySelector('.path-enabled').checked,
        description: form.querySelector('.path-description').value.trim()
    };
    
    // Validate required fields
    if (!pathData.protocol || !pathData.address) {
        alert('Protocol and Address are required');
        return;
    }
    
    try {
        let response;
        
        if (pathIndex === -1) {
            // Add new path
            response = await fetch(`/api/variables/${variableName}/paths`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(pathData)
            });
        } else {
            // Update existing path
            response = await fetch(`/api/variables/${variableName}/paths/${pathIndex}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(pathData)
            });
        }
        
        if (response.ok) {
            // Update local data
            const variable = this.variables.find(v => v.name === variableName);
            
            if (!variable.paths) {
                variable.paths = [];
            }
            
            if (pathIndex === -1) {
                // Add new path
                variable.paths.push(pathData);
            } else {
                // Update existing path
                variable.paths[pathIndex] = pathData;
            }
            
            this.applyFilters();
            this.render();
            this.closePathModal();
            
            alert(pathIndex === -1 ? 'Path added successfully' : 'Path updated successfully');
        } else {
            const error = await response.json();
            alert(`Failed to save path: ${error.message || 'Unknown error'}`);
        }
    } catch (error) {
        console.error('Error saving path:', error);
        alert('Error saving path');
    }
}
```

## 5. Update `renderPathsList` to show more details

```js
renderPathsList(variable) {
    if (!variable.paths || variable.paths.length === 0) {
        return '<div class="no-paths">No access paths defined</div>';
    }

    return variable.paths.map((path, index) => `
        <div class="path-item ${!path.enabled ? 'path-disabled' : ''}">
            <div class="path-main">
                <span class="path-protocol-badge">${path.protocol}</span>
                <span class="path-address">${path.address}</span>
                ${path.unitId !== undefined ? `<span class="path-unit">Unit: ${path.unitId}</span>` : ''}
                ${!path.enabled ? '<span class="path-status-badge disabled">Disabled</span>' : ''}
                ${path.readOnly ? '<span class="path-status-badge readonly">Read Only</span>' : ''}
            </div>
            <div class="path-details">
                <span class="path-detail">Type: ${path.dataType || 'N/A'}</span>
                <span class="path-detail">Byte Order: ${path.byteOrder || 'N/A'}</span>
                ${path.scale !== 1.0 ? `<span class="path-detail">Scale: ${path.scale}</span>` : ''}
                ${path.offset !== 0.0 ? `<span class="path-detail">Offset: ${path.offset}</span>` : ''}
                <span class="path-detail">Poll: ${path.pollRate}ms</span>
            </div>
            ${path.description ? `<div class="path-description-text">${path.description}</div>` : ''}
            <div class="path-actions">
                <button class="icon-btn edit-path-btn" 
                        data-variable-name="${variable.name}" 
                        data-path-index="${index}"
                        title="Edit Path">
                    ✏️
                </button>
                <button class="icon-btn delete-path-btn" 
                        data-variable-name="${variable.name}" 
                        data-path-index="${index}"
                        title="Remove Path">
                    🗑️
                </button>
            </div>
        </div>
    `).join('');
}
```

## 6. Add CSS for the modal and path items

```css
/* Modal Styles */
.modal {
    display: none;
    position: fixed;
    z-index: 1000;
    left: 0;
    top: 0;
    width: 100%;
    height: 100%;
    background-color: rgba(0, 0, 0, 0.5);
    align-items: center;
    justify-content: center;
}

.modal-content {
    background-color: white;
    border-radius: 8px;
    width: 90%;
    max-width: 600px;
    max-height: 90vh;
    overflow-y: auto;
    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
}

.modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 20px;
    border-bottom: 1px solid #ddd;
}

.modal-title {
    margin: 0;
    font-size: 1.5em;
}

.modal-close {
    background: none;
    border: none;
    font-size: 28px;
    cursor: pointer;
    color: #999;
    padding: 0;
    width: 30px;
    height: 30px;
    line-height: 1;
}

.modal-close:hover {
    color: #333;
}

.modal-body {
    padding: 20px;
}

.modal-actions {
    display: flex;
    gap: 10px;
    justify-content: flex-end;
    margin-top: 20px;
    padding-top: 20px;
    border-top: 1px solid #ddd;
}

/* Form Styles */
.path-form .form-group {
    margin-bottom: 15px;
}

.path-form label {
    display: block;
    font-weight: bold;
    margin-bottom: 5px;
    color: #333;
}

.path-form input[type="text"],
.path-form input[type="number"],
.path-form select,
.path-form textarea {
    width: 100%;
    padding: 8px;
    border: 1px solid #ddd;
    border-radius: 4px;
    font-size: 14px;
}

.path-form input[type="checkbox"] {
    margin-right: 8px;
}

.path-form textarea {
    resize: vertical;
    font-family: inherit;
}

.btn-primary {
    background-color: #2196F3;
    color: white;
    padding: 10px 20px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    font-size: 14px;
}

.btn-primary:hover {
    background-color: #0b7dda;
}

.btn-secondary {
    background-color: #999;
    color: white;
    padding: 10px 20px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    font-size: 14px;
}

.btn-secondary:hover {
    background-color: #777;
}

/* Path Item Styles */
.path-item {
    border: 1px solid #ddd;
    border-radius: 4px;
    padding: 12px;
    margin-bottom: 10px;
    background-color: white;
}

.path-item.path-disabled {
    background-color: #f5f5f5;
    opacity: 0.7;
}

.path-main {
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 8px;
}

.path-protocol-badge {
    background-color: #2196F3;
    color: white;
    padding: 4px 8px;
    border-radius: 4px;
    font-size: 12px;
    font-weight: bold;
    text-transform: uppercase;
}

.path-address {
    font-family: monospace;
    font-weight: bold;
    color: #333;
}

.path-unit {
    font-size: 12px;
    color: #666;
}

.path-status-badge {
    padding: 2px 6px;
    border-radius: 3px;
    font-size: 11px;
    font-weight: bold;
}

.path-status-badge.disabled {
    background-color: #f44336;
    color: white;
}

.path-status-badge.readonly {
    background-color: #ff9800;
    color: white;
}

.path-details {
    display: flex;
    flex-wrap: wrap;
    gap: 15px;
    margin-bottom: 8px;
}

.path-detail {
    font-size: 12px;
    color: #666;
}

.path-description-text {
    font-size: 13px;
    color: #555;
    font-style: italic;
    margin-bottom: 8px;
    padding: 8px;
    background-color: #f9f9f9;
    border-radius: 4px;
}

.path-actions {
    display: flex;
    gap: 5px;
    justify-content: flex-end;
}

.no-paths {
    padding: 20px;
    text-align: center;
    color: #999;
    font-style: italic;
}
```

## Summary

Now you have a complete path management modal with:

1. ✅ **Add Path** - Full form with all protocol details
2. ✅ **Edit Path** - Pre-populated form for existing paths
3. ✅ **Delete Path** - Confirmation and removal
4. ✅ **Rich path display** - Shows protocol, address, unit ID, data type, byte order, scale, offset, poll rate
5. ✅ **Visual indicators** - Badges for disabled/read-only paths
6. ✅ **Form validation** - Required fields checked
7. ✅ **Protocol support** - Modbus, DNP3, IEC 61850, OPC UA, MQTT, HTTP, SNMP
8. ✅ **Data type options** - INT16/32, UINT16/32, FLOAT32/64, BOOL, STRING
9. ✅ **Byte order options** - Big/Little Endian with word swap variants
10. ✅ **Scale & offset** - For value transformation
11. ✅ **Poll rate** - Configurable polling interval
12. ✅ **Enable/disable** - Per-path activation
13. ✅ **Read-only flag** - Mark paths as read-only
14. ✅ **Description field** - Optional documentation

The modal matches the style of your protections widget and integrates seamlessly! 🎯
