// form-generator.js
// Relies on globals provided by schemas.js: SCHEMAS, FEATURES, ENDPOINT_MAP

// --- Configuration ---
const DEFAULT_API_PORT = 8902;

const serverBase = (function() {
  if (typeof location === 'undefined') return `http://localhost:${DEFAULT_API_PORT}`;
  const host = location.hostname || 'localhost';
  return `${location.protocol}//${host}:${DEFAULT_API_PORT}`;
})();

// Use global endpoint map (fallback to local mapping if not present)
const endpointMap = window.ENDPOINT_MAP || {
  accessPath: '/api/access-paths',
  protection: '/api/protections',
  chart: '/api/charts',
  metrics: '/api/metrics',
  pub: '/api/pubs'
};

// Use global schemas and features
const SCHEMAS = window.SCHEMAS || {};
const FEATURES = window.FEATURES || [
  { key: "accessPath", label: "Add Access Path", schemaKey: "accessPath" },
  { key: "protection", label: "Add Protection", schemaKey: "protection" },
  { key: "chart", label: "Add to Chart", schemaKey: "chart" },
  { key: "metrics", label: "Add to Metrics", schemaKey: "metrics" },
  { key: "pub", label: "Add to Pub", schemaKey: "pub" }
];

// --- Small helpers ---
function el(tag, props = {}, ...children) {
  const e = document.createElement(tag);
  for (const k in props) {
    if (k === 'class') e.className = props[k];
    else if (k === 'html') e.innerHTML = props[k];
    else e.setAttribute(k, props[k]);
  }
  children.forEach(c => { if (c !== null && c !== undefined) e.appendChild(typeof c === 'string' ? document.createTextNode(c) : c); });
  return e;
}

function escapeForSingleQuotedShell(str) {
  return String(str).replace(/'/g, "'\"'\"'");
}

// --- Render form ---
function renderForm(container, schema, data = {}) {
  container.innerHTML = '';

  (schema.fields || []).forEach(field => {
    const wrapper = el('div', { class: 'form-group' });

    const label = el('label', { for: field.name }, field.label);
    wrapper.appendChild(label);

    let input;
    switch (field.type) {
      case 'text':
      case 'number':
        input = document.createElement('input');
        input.type = field.type;
        input.id = field.name;
        input.name = field.name;
        input.value = data[field.name] ?? field.default ?? '';
        if (field.required) input.required = true;
        break;
      case 'select':
        input = document.createElement('select');
        input.id = field.name;
        input.name = field.name;
        (field.options || []).forEach(opt => {
          const option = document.createElement('option');
          option.value = opt;
          option.textContent = opt;
          if (data[field.name] === opt) option.selected = true;
          input.appendChild(option);
        });
        if (field.required) input.required = true;
        break;
      case 'checkbox':
        input = document.createElement('input');
        input.type = 'checkbox';
        input.id = field.name;
        input.name = field.name;
        input.checked = data[field.name] ?? !!field.default;
        break;
      default:
        console.warn('Unsupported field type:', field.type);
        return;
    }

    wrapper.appendChild(input);
    container.appendChild(wrapper);
  });

  const submitBtn = document.createElement('button');
  submitBtn.type = 'submit';
  submitBtn.textContent = 'Save';
  container.appendChild(submitBtn);
}

// --- Collect Payload  data and group nested fields ---
function collectPayloadFromForm(formElement, schema, variableName) {
  const flatData = collectFormData(formElement, schema);
  flatData.variable_name = variableName;

  if (schema.nested) {
    const nestedKey = schema.nested;
    const nestedData = {};
    const rootData = {};

    Object.keys(flatData).forEach(key => {
      if (key.startsWith(nestedKey + '_')) {
        const subKey = key.substring(nestedKey.length + 1); // remove "reference_" prefix
        nestedData[subKey] = flatData[key];
      } else {
        rootData[key] = flatData[key];
      }
    });

    rootData[nestedKey] = nestedData;
    return rootData;
  }

  return flatData;
}

// --- Collect form data ---
function collectFormData(formElement, schema) {
  const result = {};
  (schema.fields || []).forEach(field => {
    const input = formElement.querySelector(`[name="${field.name}"]`);
    if (!input) return;
    switch (field.type) {
      case 'checkbox':
        result[field.name] = input.checked;
        break;
      case 'number':
        result[field.name] = input.value ? Number(input.value) : null;
        break;
      default:
        result[field.name] = input.value;
    }
  });
  return result;
}

// --- Modal logic ---
let currentModalType = null;

function openModal(modalType, initialData = {}) {
  currentModalType = modalType;
  const modal = document.getElementById('pathModal');
  const form = document.getElementById('pathForm');
  const titleEl = document.getElementById('modalTitle');

  const schema = SCHEMAS[modalType];
  if (!schema) {
    alert(`Unknown modal type: ${modalType}`);
    return;
  }

  titleEl.textContent = schema.title;
  renderForm(form, schema, initialData);

  // show
  modal.style.display = 'flex';
  modal.setAttribute('aria-hidden', 'false');

  // focus first input for accessibility
  setTimeout(() => {
    const first = modal.querySelector('input, select, textarea, button');
    if (first) first.focus();
  }, 0);

  // Hide script panel
  hideScriptPanel();

  // attach submit
  form.onsubmit = async (e) => {
    e.preventDefault();
    const variableName = document.getElementById('variableSelect').value;
    const data = collectFormData(form, schema);
    data.variable_name = variableName;
    const payload = collectPayloadFromForm(form, schema, variableName);

    const endpoint = endpointMap[modalType];
    if (!endpoint) {
      alert('No endpoint defined for this modal type; check endpointMap.');
      return;
    }

    const url = `${serverBase}${endpoint}`;
    try {
      const submitBtn = form.querySelector('button[type="submit"]');
      if (submitBtn) submitBtn.disabled = true;

      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });

      let body = null;
      try { body = await res.json(); } catch (err) { body = await res.text(); }

      if (res.ok) {
        console.log(`[${modalType}] success:`, body);
        alert(`Saved ${modalType} for variable "${variableName}"`);
        closeModal();
      } else {
        console.error(`[${modalType}] error response:`, res.status, body);
        alert(`Error saving: ${res.status} - see console`);
      }
    } catch (err) {
      console.error('Network/error sending data', err);
      alert('Failed to send request (network error). Check console.');
    } finally {
      const submitBtn = form.querySelector('button[type="submit"]');
      if (submitBtn) submitBtn.disabled = false;
    }
  };
}

function closeModal() {
  currentModalType = null;
  const modal = document.getElementById('pathModal');

  // Move focus back to variableSelect for better UX
  const trigger = document.getElementById('variableSelect');
  if (trigger) trigger.focus();

  // Remove focus from any focused element inside modal (avoid aria-hidden warning)
  if (document.activeElement && modal.contains(document.activeElement)) {
    document.activeElement.blur();
  }

  modal.style.display = 'none';
  modal.setAttribute('aria-hidden', 'true');
  hideScriptPanel();
}

// --- Script panel (Show curl) ---
function showScriptPanel(curlText) {
  const panel = document.getElementById('scriptPanel');
  const pre = document.getElementById('curlScript');
  pre.textContent = curlText;
  panel.style.display = 'block';
  panel.setAttribute('aria-hidden', 'false');
}

function hideScriptPanel() {
  const panel = document.getElementById('scriptPanel');
  panel.style.display = 'none';
  panel.setAttribute('aria-hidden', 'true');
}

function generateCurlForCurrentForm() {
  if (!currentModalType) {
    alert('Modal not open.');
    return;
  }
  const schema = SCHEMAS[currentModalType];
  const form = document.getElementById('pathForm');
  const data = collectFormData(form, schema);
  const variableName = document.getElementById('variableSelect').value;
  data.variable_name = variableName;

  // prune
  Object.keys(data).forEach(k => {
    if (data[k] === null || data[k] === '') delete data[k];
  });

  const endpoint = endpointMap[currentModalType] || '/api/unknown';
  const url = `${serverBase}${endpoint}`;

  const jsonText = JSON.stringify(data, null, 2);
  const escaped = escapeForSingleQuotedShell(jsonText);

  const curl = `curl -X POST '${url}' \\\n  -H "Content-Type: application/json" \\\n  -d '${escaped}'`;
  return curl;
}

// --- Render feature buttons from FEATURES ---
function renderFeatureButtons() {
  const cont = document.getElementById('featuresContainer');
  cont.innerHTML = '';
  (FEATURES || []).forEach(f => {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'openModalBtn';
    btn.textContent = f.label;
    // use schemaKey if provided, else fallback to key
    const modalType = f.schemaKey || f.key;
    btn.setAttribute('data-modal', modalType);
    btn.addEventListener('click', () => openModal(modalType));
    cont.appendChild(btn);
  });
}

window.addEventListener('DOMContentLoaded', () => {
  // render feature buttons
  renderFeatureButtons();

  // Hard reload button
  document.getElementById('reloadButton').addEventListener('click', () => {
    // Force reload ignoring cache
    try { location.reload(true); } catch (e) { location.reload(); }
  });

  // Close modal events
  document.getElementById('closeModalBtn').addEventListener('click', closeModal);
  document.getElementById('pathModal').addEventListener('click', (e) => {
    if (e.target.id === 'pathModal') closeModal();
  });

  // Show curl
  document.getElementById('showCurlBtn').addEventListener('click', () => {
    const curl = generateCurlForCurrentForm();
    showScriptPanel(curl);
  });

  // Hide/copy script actions
  document.getElementById('hideCurlBtn').addEventListener('click', hideScriptPanel);
  document.getElementById('copyCurlBtn').addEventListener('click', async () => {
    const txt = document.getElementById('curlScript').textContent;
    try {
      await navigator.clipboard.writeText(txt);
      alert('Copied to clipboard');
    } catch (err) {
      const ta = document.createElement('textarea');
      ta.value = txt;
      document.body.appendChild(ta);
      ta.select();
      document.execCommand('copy');
      ta.remove();
      alert('Copied (fallback)');
    }
  });
});