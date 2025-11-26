// schemas.js
// version 1.0
// Put this BEFORE form-generator.js in index.html
// Exposes globals: SCHEMAS, FEATURES, ENDPOINT_MAP

window.SCHEMAS = {
  accessPath: {
    title: "Add Access Path",
    nested: "reference",
    fields: [
      { name: "access_type", label: "Access Type", type: "select", options: ["modbus", "can", "string"], required: true },
      { name: "protocol", label: "Protocol", type: "text" },
      { name: "address", label: "Address", type: "text", required: true },
      { name: "unit_id", label: "Unit ID", type: "number" },
      { name: "read_only", label: "Read Only", type: "checkbox" },
      { name: "priority", label: "Priority", type: "number", default: 10 },
      // Reference nested fields
    { name: "reference_table", label: "Reference Table", type: "text", required: true, default: "oper:sbmu:input" },
    { name: "reference_offset", label: "Reference Offset", type: "number", required: true, default: 100 },
    { name: "reference_size", label: "Reference Size", type: "number", required: true, default: 2 },
    { name: "reference_num", label: "Reference Num", type: "number", required: true, default: 1 }
  
    ]
  },

  protection: {
    title: "Add Protection",
    fields: [
      { name: "protection_type", label: "Protection Type", type: "select", options: ["overvoltage", "undervoltage", "overcurrent"], required: true },
      { name: "threshold", label: "Threshold", type: "number", required: true },
      { name: "enabled", label: "Enabled", type: "checkbox", default: true }
    ]
  },

  chart: {
    title: "Add to Chart",
    fields: [
      { name: "chart_name", label: "Chart Name", type: "text", required: true },
      { name: "color", label: "Color (hex or name)", type: "text" }
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
      { name: "format", label: "Format Details (e.g. json/simple)", type: "text" }
    ]
  }
};

// Features list (used to render the buttons)
window.FEATURES = [
  { key: "accessPath", label: "Add Access Path", schemaKey: "accessPath" },
  { key: "protection", label: "Add Protection", schemaKey: "protection" },
  { key: "chart", label: "Add to Chart", schemaKey: "chart" },
  { key: "metrics", label: "Add to Metrics", schemaKey: "metrics" },
  { key: "pub", label: "Add to Pub", schemaKey: "pub" }
];
// Base server URL
window.SERVER_BASE = 'http://192.168.86.51:8902';
// Endpoint mapping -- change to your real server paths if needed
window.ENDPOINT_MAP = {
  accessPath: '/api/access-paths',
  protection: '/api/protections',
  chart: '/api/charts',
  metrics: '/api/metrics',
  pub: '/api/pubs'
};