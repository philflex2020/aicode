// variables.js

// --- Configurable defaults ---
const DEFAULT_FILTERS = {
  category: 'oper',
  locator: '',
  name_pattern: '%cell%',
  include_paths: true,
  since_version: null
};

// --- Fetch and populate variables ---
async function loadVariables(filters = {}) {
  const params = new URLSearchParams();

  const effectiveFilters = { ...DEFAULT_FILTERS, ...filters };

  if (effectiveFilters.category) params.append('category', effectiveFilters.category);
  if (effectiveFilters.locator) params.append('locator', effectiveFilters.locator);
  if (effectiveFilters.name_pattern) params.append('name_pattern', effectiveFilters.name_pattern);
  if (effectiveFilters.include_paths !== undefined) params.append('include_paths', effectiveFilters.include_paths);
  if (effectiveFilters.since_version !== null) params.append('since_version', effectiveFilters.since_version);

  const url = `${window.SERVER_BASE || 'http://192.168.86.51:8902'}/api/variables?${params.toString()}`;
  console.log('Loading variables from:', url);

  const select = document.getElementById('variableSelect');
  if (!select) return;

  select.innerHTML = '<option disabled>Loading...</option>';

  try {
    const res = await fetch(url, { headers: { 'Accept': 'application/json' } });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);

    const variables = await res.json();

    select.innerHTML = '';
    if (variables.length === 0) {
      select.innerHTML = '<option disabled>No variables found</option>';
      return;
    }

    variables.forEach(v => {
      const option = document.createElement('option');
      option.value = v.name;
      option.textContent = v.name;
      select.appendChild(option);
    });

    // Select first item by default
    if (variables.length > 0) {
      select.value = variables[0].name;
    }
  } catch (err) {
    console.error('Error loading variables:', err);
    select.innerHTML = '<option disabled>Error loading variables</option>';
  }
}

// --- Render filter form ---
function renderVariableFilterForm(containerId = 'variableFilterForm') {
  const container = document.getElementById(containerId);
  if (!container) return;

  container.innerHTML = `
    <fieldset style="border:1px solid #444; padding:10px; margin-bottom:10px;">
      <legend>Filter Variables</legend>
      <div style="display:grid; grid-template-columns:1fr 1fr; gap:10px;">
        <div>
          <label>Category</label>
          <select id="filter_category" style="width:100%;">
            <option value="">Any</option>
            <option value="config">Config</option>
            <option value="prot">Prot</option>
            <option value="oper" selected>Oper</option>
          </select>
        </div>
        <div>
          <label>Locator</label>
          <input type="text" id="filter_locator" placeholder="e.g. sbmu1" />
        </div>
        <div>
          <label>Name Pattern</label>
          <input type="text" id="filter_name_pattern" placeholder="e.g. %cell%" />
        </div>
        <div>
          <label>Since Version</label>
          <input type="number" id="filter_since_version" placeholder="e.g. 100" />
        </div>
        <div>
          <label><input type="checkbox" id="filter_include_paths" checked /> Include Paths</label>
        </div>
        <div>
          <button type="button" id="applyFilterBtn">Apply Filter</button>
        </div>
      </div>
    </fieldset>
  `;

  // Apply button handler
  document.getElementById('applyFilterBtn').addEventListener('click', () => {
    const filters = {
      category: document.getElementById('filter_category').value || null,
      locator: document.getElementById('filter_locator').value || null,
      name_pattern: document.getElementById('filter_name_pattern').value || null,
      include_paths: document.getElementById('filter_include_paths').checked,
      since_version: parseInt(document.getElementById('filter_since_version').value) || null
    };

    loadVariables(filters);
  });
}

// --- Initialize on DOM ready ---
document.addEventListener('DOMContentLoaded', () => {
  renderVariableFilterForm();
  loadVariables(DEFAULT_FILTERS);
});