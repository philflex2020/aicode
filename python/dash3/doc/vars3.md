Here's the complete cleaned-up version of `VariablesWidget`:

```js
class VariablesWidget extends BaseWidget {
    constructor(node, cfg) {
        super(node, cfg);
        this.variables = [];
        this.filteredVariables = [];
        this.selectedCategory = 'oper';
        this.searchQuery = '';
        this.showPaths = false;
        this.sortBy = 'name';
        this.sortAsc = true;
        this.expandedVariable = null;
        this.systemVersion = 'Unknown';
        
        // Load data once on initialization
        this.fetchVariables();
        this.fetchSystemVersion();
    }

    async fetchVariables() {
        console.log("Fetching variables for category:", this.selectedCategory);
        try {
            const url = this.selectedCategory === 'all' 
                ? '/api/variables'
                : `/api/variables?category=${this.selectedCategory}`;

            console.log("Fetching variables for url:", url);
            const data = await getJSON(url);
            console.log("Fetching variables data:", data);

            if (Array.isArray(data)) {
                this.variables = data;
                console.log("before apply filters:");
                this.applyFilters();
                console.log("before apply render:");
                this.render();
            }
        } catch (error) {
            console.error('Error fetching variables:', error);
            this.renderError('Failed to load variables');
        }
    }

    async fetchSystemVersion() {
        try {
            const data = await getJSON('/api/system/version');
            this.systemVersion = data.system_version;
        } catch (error) {
            console.error('Error fetching system version:', error);
        }
    }

    applyFilters() {
        this.filteredVariables = this.variables.filter(v => {
            // Search filter
            if (this.searchQuery) {
                const query = this.searchQuery.toLowerCase();
                const matchName = (v.name || '').toLowerCase().includes(query);
                const matchDisplay = (v.display_name || '').toLowerCase().includes(query);
                const matchDesc = (v.description || '').toLowerCase().includes(query);
                if (!matchName && !matchDisplay && !matchDesc) {
                    return false;
                }
            }
            return true;
        });

        // Sort
        this.filteredVariables.sort((a, b) => {
            let aVal, bVal;

            switch (this.sortBy) {
                case 'name':
                    aVal = a.name;
                    bVal = b.name;
                    break;
                case 'category':
                    aVal = a.category;
                    bVal = b.category;
                    break;
                case 'version':
                    aVal = a.variable_version;
                    bVal = b.variable_version;
                    break;
                default:
                    aVal = a.name;
                    bVal = b.name;
            }

            if (typeof aVal === 'string') {
                return this.sortAsc 
                    ? aVal.localeCompare(bVal)
                    : bVal.localeCompare(aVal);
            } else {
                return this.sortAsc ? aVal - bVal : bVal - aVal;
            }
        });
    }

    render() {
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

                        <div class="control-group">
                            <label>
                                <input type="checkbox" class="show-paths-checkbox" ${this.showPaths ? 'checked' : ''}>
                                Show Access Paths
                            </label>
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
                                    <option value="dnp3">DNP3</option>
                                    <option value="iec61850">IEC 61850</option>
                                    <option value="opcua">OPC UA</option>
                                    <option value="mqtt">MQTT</option>
                                    <option value="http">HTTP/REST</option>
                                    <option value="snmp">SNMP</option>
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
        
        this.body.innerHTML = html;
        this.attachEventHandlers();
    }

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

    renderError(message) {
        this.body.innerHTML = `
            <div class="variables-widget">
                <div class="error-message">${message}</div>
            </div>
        `;
    }

    attachEventHandlers() {
        const container = this.body;

        // Category filter
        const categoryFilter = container.querySelector('.category-filter');
        if (categoryFilter) {
            categoryFilter.value = this.selectedCategory;
            categoryFilter.addEventListener('change', (e) => {
                this.selectedCategory = e.target.value;
                this.fetchVariables();
            });
        }

        // Search input
        const searchInput = container.querySelector('.search-input');
        if (searchInput) {
            searchInput.addEventListener('input', (e) => {
                this.searchQuery = e.target.value;
                this.applyFilters();
                this.render();
            });
        }

        // Sort controls
        const sortSelect = container.querySelector('.sort-select');
        if (sortSelect) {
            sortSelect.value = this.sortBy;
            sortSelect.addEventListener('change', (e) => {
                this.sortBy = e.target.value;
                this.applyFilters();
                this.render();
            });
        }

        const sortDirBtn = container.querySelector('.sort-direction-btn');
        if (sortDirBtn) {
            sortDirBtn.addEventListener('click', () => {
                this.sortAsc = !this.sortAsc;
                this.applyFilters();
                this.render();
            });
        }

        // Show paths checkbox
        const showPathsCheckbox = container.querySelector('.show-paths-checkbox');
        if (showPathsCheckbox) {
            showPathsCheckbox.addEventListener('change', (e) => {
                this.showPaths = e.target.checked;
                this.render();
            });
        }

        // Refresh button
        const refreshBtn = container.querySelector('.refresh-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => {
                this.fetchVariables();
                this.fetchSystemVersion();
            });
        }

        // Add variable button
        const addBtn = container.querySelector('.add-variable-btn');
        if (addBtn) {
            addBtn.addEventListener('click', () => {
                this.showAddVariableModal();
            });
        }

        // Edit variable buttons
        const editBtns = container.querySelectorAll('.edit-variable-btn');
        editBtns.forEach(btn => {
            btn.addEventListener('click', (e) => {
                const variableName = btn.getAttribute('data-variable-name');
                this.showEditVariableModal(variableName);
            });
        });

        // Delete variable buttons
        const deleteBtns = container.querySelectorAll('.delete-variable-btn');
        deleteBtns.forEach(btn => {
            btn.addEventListener('click', (e) => {
                const variableName = btn.getAttribute('data-variable-name');
                this.deleteVariable(variableName);
            });
        });

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

    toggleVariableDetails(variableName) {
        if (this.expandedVariable === variableName) {
            this.expandedVariable = null;
        } else {
            this.expandedVariable = variableName;
        }
        this.render();
    }

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
        form.querySelector('.path-enabled').checked = path.enabled !== false;
        form.querySelector('.path-description').value = path.description || '';
        
        // Show modal
        modal.style.display = 'flex';
    }

    closePathModal() {
        const modal = this.body.querySelector('.path-modal');
        modal.style.display = 'none';
    }

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

    showAddVariableModal() {
        const modalHtml = `
            <div class="modal-overlay" id="add-variable-modal">
                <div class="modal-content">
                    <div class="modal-header">
                        <h3>Add New Variable</h3>
                        <button class="modal-close">&times;</button>
                    </div>
                    <div class="modal-body">
                        <div class="form-group">
                            <label>Name:</label>
                            <input type="text" id="var-name" placeholder="e.g., battery_voltage">
                        </div>
                        <div class="form-group">
                            <label>Display Name:</label>
                            <input type="text" id="var-display-name" placeholder="e.g., Battery Voltage">
                        </div>
                        <div class="form-group">
                            <label>Units:</label>
                            <input type="text" id="var-units" placeholder="e.g., V">
                        </div>
                        <div class="form-group">
                            <label>Description:</label>
                            <textarea id="var-description" rows="3"></textarea>
                        </div>
                        <div class="form-group">
                            <label>Category:</label>
                            <select id="var-category">
                                <option value="oper">Operational</option>
                                <option value="config">Config</option>
                                <option value="prot">Protection</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Locator (optional):</label>
                            <input type="text" id="var-locator" placeholder="e.g., rbms.battery.voltage">
                        </div>
                    </div>
                    <div class="modal-footer">
                        <button class="btn-cancel">Cancel</button>
                        <button class="btn-save">Create Variable</button>
                    </div>
                </div>
            </div>
        `;

        document.body.insertAdjacentHTML('beforeend', modalHtml);

        const modal = document.getElementById('add-variable-modal');
        modal.querySelector('.modal-close').addEventListener('click', () => modal.remove());
        modal.querySelector('.btn-cancel').addEventListener('click', () => modal.remove());
        modal.querySelector('.btn-save').addEventListener('click', () => this.createVariable());
    }

    async createVariable() {
        const data = {
            name: document.getElementById('var-name').value.trim(),
            display_name: document.getElementById('var-display-name').value.trim(),
            units: document.getElementById('var-units').value.trim(),
            description: document.getElementById('var-description').value.trim(),
            category: document.getElementById('var-category').value,
            locator: document.getElementById('var-locator').value.trim() || null,
            source_system: 'ui'
        };

        if (!data.name || !data.display_name) {
            alert('Name and Display Name are required');
            return;
        }

        try {
            const response = await fetch('/api/variables', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });

            if (!response.ok) {
                const error = await response.json();
                throw new Error(error.detail || 'Failed to create variable');
            }

            document.getElementById('add-variable-modal').remove();
            this.fetchVariables();
            alert('Variable created successfully');
        } catch (error) {
            console.error('Error creating variable:', error);
            alert('Failed to create variable: ' + error.message);
        }
    }

    async showEditVariableModal(variableName) {
        try {
            const variable = await getJSON(`/api/variables/${variableName}`);

            const modalHtml = `
                <div class="modal-overlay" id="edit-variable-modal">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h3>Edit Variable: ${variable.name}</h3>
                            <button class="modal-close">&times;</button>
                        </div>
                        <div class="modal-body">
                            <div class="form-group">
                                <label>Name:</label>
                                <input type="text" id="edit-var-name" value="${variable.name}" disabled>
                            </div>
                            <div class="form-group">
                                <label>Display Name:</label>
                                <input type="text" id="edit-var-display-name" value="${variable.display_name}">
                            </div>
                            <div class="form-group">
                                <label>Units:</label>
                                <input type="text" id="edit-var-units" value="${variable.units}">
                            </div>
                            <div class="form-group">
                                <label>Description:</label>
                                <textarea id="edit-var-description" rows="3">${variable.description}</textarea>
                            </div>
                            <div class="form-group">
                                <label>Category:</label>
                                <select id="edit-var-category">
                                    <option value="oper" ${variable.category === 'oper' ? 'selected' : ''}>Operational</option>
                                    <option value="config" ${variable.category === 'config' ? 'selected' : ''}>Config</option>
                                    <option value="prot" ${variable.category === 'prot' ? 'selected' : ''}>Protection</option>
                                </select>
                            </div>
                            <div class="form-group">
                                <label>Locator:</label>
                                <input type="text" id="edit-var-locator" value="${variable.locator || ''}">
                            </div>
                        </div>
                        <div class="modal-footer">
                            <button class="btn-cancel">Cancel</button>
                            <button class="btn-save">Save Changes</button>
                        </div>
                    </div>
                </div>
            `;

            document.body.insertAdjacentHTML('beforeend', modalHtml);

            const modal = document.getElementById('edit-variable-modal');
            modal.querySelector('.modal-close').addEventListener('click', () => modal.remove());
            modal.querySelector('.btn-cancel').addEventListener('click', () => modal.remove());
            modal.querySelector('.btn-save').addEventListener('click', () => this.updateVariable(variableName));
        } catch (error) {
            console.error('Error fetching variable:', error);
            alert('Failed to load variable details');
        }
    }

    async updateVariable(variableName) {
        const data = {
            display_name: document.getElementById('edit-var-display-name').value.trim(),
            units: document.getElementById('edit-var-units').value.trim(),
            description: document.getElementById('edit-var-description').value.trim(),
            category: document.getElementById('edit-var-category').value,
            locator: document.getElementById('edit-var-locator').value.trim() || null
        };

        try {
            const response = await fetch(`/api/variables/${variableName}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });

            if (!response.ok) {
                const error = await response.json();
                throw new Error(error.detail || 'Failed to update variable');
            }

            document.getElementById('edit-variable-modal').remove();
            this.fetchVariables();
            alert('Variable updated successfully');
        } catch (error) {
            console.error('Error updating variable:', error);
            alert('Failed to update variable: ' + error.message);
        }
    }

    async deleteVariable(variableName) {
        if (!confirm(`Are you sure you want to delete variable "${variableName}"?`)) {
            return;
        }

        try {
            const response = await fetch(`/api/variables/${variableName}`, {
                method: 'DELETE'
            });

            if (!response.ok) {
                const error = await response.json();
                throw new Error(error.detail || 'Failed to delete variable');
            }

            this.fetchVariables();
            alert('Variable deleted successfully');
        } catch (error) {
            console.error('Error deleting variable:', error);
            alert('Failed to delete variable: ' + error.message);
        }
    }
}
```

## Key Changes Made:

1. ✅ **Removed duplicate `systemVersion` initialization**
2. ✅ **Added null checks in `applyFilters()`** - Uses `(v.name || '')` pattern
3. ✅ **Replaced `$.getJSON` with `getJSON` helper** in `fetchSystemVersion()`
4. ✅ **Converted all jQuery modal code to vanilla JS**:
   - `$('body').append()` → `document.body.insertAdjacentHTML()`
   - `$('#id')` → `document.getElementById()`
   - `$.ajax()` → `fetch()`
5. ✅ **Removed `xrenderPathsList()` dead code**
6. ✅ **Removed `init()` and `tick()` methods** (not needed since you fetch in constructor)
7. ✅ **Consistent error handling** with try/catch and proper response checking
8. ✅ **All AJAX calls now use `fetch` API**

The code is now clean, consistent, and follows modern JavaScript patterns! 🎉


add path modal

The issue is that the path modal is rendered inside `this.body` (the widget container), but when you try to show it with `modal.style.display = 'flex'`, it might be getting clipped or hidden by the widget's container styles.

Here's the fix - move the modal outside of the widget container and append it to the document body:

## Option 1: Move Modal to Body (Recommended)

Update the `render()` method to NOT include the modal in the widget HTML, and instead create it separately:

```js
render() {
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

                    <div class="control-group">
                        <label>
                            <input type="checkbox" class="show-paths-checkbox" ${this.showPaths ? 'checked' : ''}>
                            Show Access Paths
                        </label>
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
    `;
    
    this.body.innerHTML = html;
    
    // Create or update the path modal (outside widget container)
    this.ensurePathModal();
    
    this.attachEventHandlers();
}

ensurePathModal() {
    // Check if modal already exists
    let modal = document.getElementById('path-modal');
    
    if (!modal) {
        const modalHtml = `
            <div class="modal path-modal" id="path-modal" style="display: none;">
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
                                    <option value="dnp3">DNP3</option>
                                    <option value="iec61850">IEC 61850</option>
                                    <option value="opcua">OPC UA</option>
                                    <option value="mqtt">MQTT</option>
                                    <option value="http">HTTP/REST</option>
                                    <option value="snmp">SNMP</option>
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
        
        document.body.insertAdjacentHTML('beforeend', modalHtml);
        modal = document.getElementById('path-modal');
        
        // Attach modal event handlers once
        this.attachModalHandlers(modal);
    }
}

attachModalHandlers(modal) {
    // Modal close buttons
    const modalClose = modal.querySelector('.modal-close');
    if (modalClose) {
        modalClose.addEventListener('click', () => {
            this.closePathModal();
        });
    }

    const modalCancel = modal.querySelector('.modal-cancel');
    if (modalCancel) {
        modalCancel.addEventListener('click', () => {
            this.closePathModal();
        });
    }

    // Close modal on background click
    modal.addEventListener('click', (e) => {
        if (e.target === modal) {
            this.closePathModal();
        }
    });

    // Path form submission
    const pathForm = modal.querySelector('.path-form');
    if (pathForm) {
        pathForm.addEventListener('submit', (e) => {
            e.preventDefault();
            this.savePathFromForm();
        });
    }
}
```

Now update `attachEventHandlers()` to remove the modal-related handlers (since they're now in `attachModalHandlers()`):

```js
attachEventHandlers() {
    const container = this.body;

    // Category filter
    const categoryFilter = container.querySelector('.category-filter');
    if (categoryFilter) {
        categoryFilter.value = this.selectedCategory;
        categoryFilter.addEventListener('change', (e) => {
            this.selectedCategory = e.target.value;
            this.fetchVariables();
        });
    }

    // Search input
    const searchInput = container.querySelector('.search-input');
    if (searchInput) {
        searchInput.addEventListener('input', (e) => {
            this.searchQuery = e.target.value;
            this.applyFilters();
            this.render();
        });
    }

    // Sort controls
    const sortSelect = container.querySelector('.sort-select');
    if (sortSelect) {
        sortSelect.value = this.sortBy;
        sortSelect.addEventListener('change', (e) => {
            this.sortBy = e.target.value;
            this.applyFilters();
            this.render();
        });
    }

    const sortDirBtn = container.querySelector('.sort-direction-btn');
    if (sortDirBtn) {
        sortDirBtn.addEventListener('click', () => {
            this.sortAsc = !this.sortAsc;
            this.applyFilters();
            this.render();
        });
    }

    // Show paths checkbox
    const showPathsCheckbox = container.querySelector('.show-paths-checkbox');
    if (showPathsCheckbox) {
        showPathsCheckbox.addEventListener('change', (e) => {
            this.showPaths = e.target.checked;
            this.render();
        });
    }

    // Refresh button
    const refreshBtn = container.querySelector('.refresh-btn');
    if (refreshBtn) {
        refreshBtn.addEventListener('click', () => {
            this.fetchVariables();
            this.fetchSystemVersion();
        });
    }

    // Add variable button
    const addBtn = container.querySelector('.add-variable-btn');
    if (addBtn) {
        addBtn.addEventListener('click', () => {
            this.showAddVariableModal();
        });
    }

    // Edit variable buttons
    const editBtns = container.querySelectorAll('.edit-variable-btn');
    editBtns.forEach(btn => {
        btn.addEventListener('click', (e) => {
            const variableName = btn.getAttribute('data-variable-name');
            this.showEditVariableModal(variableName);
        });
    });

    // Delete variable buttons
    const deleteBtns = container.querySelectorAll('.delete-variable-btn');
    deleteBtns.forEach(btn => {
        btn.addEventListener('click', (e) => {
            const variableName = btn.getAttribute('data-variable-name');
            this.deleteVariable(variableName);
        });
    });

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

Update the modal methods to use `document.getElementById`:

```js
showAddPathModal(variableName) {
    const modal = document.getElementById('path-modal');
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
    const modal = document.getElementById('path-modal');
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
    form.querySelector('.path-enabled').checked = path.enabled !== false;
    form.querySelector('.path-description').value = path.description || '';
    
    // Show modal
    modal.style.display = 'flex';
}

closePathModal() {
    const modal = document.getElementById('path-modal');
    modal.style.display = 'none';
}

async savePathFromForm() {
    const form = document.getElementById('path-modal').querySelector('.path-form');
    
    const variableName = form.querySelector('.path-variable-name').value;
    const pathIndex = parseInt(form.querySelector('.path-index').value);
    
    // ... rest of the method stays the same
}
```

This approach ensures the modal is:
1. Created once and attached to `document.body`
2. Reused across multiple renders
3. Not affected by widget container styles
4. Properly positioned as a full-screen overlay

The modal will now display correctly! 🎉
