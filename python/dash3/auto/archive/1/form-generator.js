// Schemas for each modal type
const schemas = {
  accessPath: {
    title: "Add Access Path",
    fields: [
      { name: "access_type", label: "Access Type", type: "select", options: ["modbus", "can", "memory"], required: true },
      { name: "protocol", label: "Protocol", type: "text" },
      { name: "address", label: "Address", type: "text", required: true },
      { name: "unit_id", label: "Unit ID", type: "number" },
      { name: "read_only", label: "Read Only", type: "checkbox" },
      { name: "priority", label: "Priority", type: "number", default: 10 }
    ]
  },
  protection: {
    title: "Add Protection",
    fields: [
      { name: "protection_type", label: "Protection Type", type: "select", options: ["max", "min"], required: true },
      { name: "protection_level", label: "Protection Level", type: "select", options: [1, 2, 3], required: true },
      { name: "on_duration", label: "On Duration", type: "number", default: 3 },
      { name: "off_duration", label: "Off Duration", type: "number", default: 3 },
      { name: "histeresis", label: "Histeresis", type: "number", required: true },
      { name: "threshold", label: "Threshold", type: "number", required: true },
      { name: "enabled", label: "Enabled", type: "checkbox", default: true }
    ]
  },
  chart: {
    title: "Add to Chart",
    fields: [
      { name: "chart_name", label: "Chart Name", type: "text", required: true },
      { name: "color", label: "Color", type: "text" }
    ]
  },
  metrics: {
    title: "Add to Metrics",
    fields: [
      { name: "metric_name", label: "Metric Name", type: "text", required: true },
      { name: "aggregation", label: "Aggregation", type: "select", options: ["max", "min", "avg", "count"], required: true }
    ]
  },
  pub: {
    title: "Add to Pub",
    fields: [
      { name: "pub_name", label: "Publish Stream Name", type: "text", required: true },
      { name: "format", label: "Format Details", type: "text" }
    ]
  }
};

// Render form from schema
function renderForm(container, schema, data = {}) {
  container.innerHTML = '';

  schema.fields.forEach(field => {
    const wrapper = document.createElement('div');
    wrapper.className = 'form-group';

    const label = document.createElement('label');
    label.textContent = field.label;
    label.htmlFor = field.name;
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
        field.options.forEach(opt => {
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
        input.checked = data[field.name] ?? false;
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

// Collect form data
function collectFormData(formElement, schema) {
  const result = {};
  schema.fields.forEach(field => {
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

// Open modal with schema
function openModal(modalType, initialData = {}) {
  const modal = document.getElementById('pathModal');
  const form = document.getElementById('pathForm');
  const titleEl = document.getElementById('modalTitle');

  const schema = schemas[modalType];
  if (!schema) {
    alert(`Unknown modal type: ${modalType}`);
    return;
  }

  titleEl.textContent = schema.title;
  renderForm(form, schema, initialData);

  modal.style.display = 'flex';
  modal.setAttribute('aria-hidden', 'false');

  form.onsubmit = (e) => {
    e.preventDefault();
    const data = collectFormData(form, schema);
    const variableName = document.getElementById('variableSelect').value;
    data.variable_name = variableName;

    console.log(`[${modalType}] Submitting for variable "${variableName}":`, data);

    // TODO: Replace with real API call
    alert(`Saved ${modalType} for variable "${variableName}". Check console.`);

    closeModal();
  };
}

function closeModal() {
  const modal = document.getElementById('pathModal');
  modal.style.display = 'none';
  modal.setAttribute('aria-hidden', 'true');
}

// Event listeners
window.addEventListener('DOMContentLoaded', () => {
  document.querySelectorAll('.openModalBtn').forEach(btn => {
    btn.addEventListener('click', () => {
      const modalType = btn.getAttribute('data-modal');
      openModal(modalType);
    });
  });

  document.getElementById('closeModalBtn').addEventListener('click', closeModal);

  document.getElementById('pathModal').addEventListener('click', (e) => {
    if (e.target.id === 'pathModal') closeModal();
  });
});