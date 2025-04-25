const math = require('mathjs');

// Time step and simulation steps
const dt = 1e-3;  // 1 ms
const steps = 10;

// Define nodes (0 = ground)
const nodes = ['0', '1', '2', '3'];
const nodeIndex = { '0': 0, '1': 1, '2': 2, '3': 3 };

// Components with transient state
const elements = [
  { type: 'resistor', from: '1', to: '0', value: 1000 },
  { type: 'capacitor', from: '1', to: '2', value: 1e-6, state: { voltage: 0.0 } },
  { type: 'inductor', from: '2', to: '3', value: 1e-3, state: { current: 0.0 } },
  { type: 'voltage', from: '3', to: '0', value: 5 }
];

const n = nodes.length - 1;
const m = elements.filter(e => e.type === 'voltage' || e.type === 'inductor').length;
const totalSize = n + m;

const voltageCount = elements.filter(e => e.type === 'voltage').length;
const inductorIndexOffset = voltageCount;

for (let step = 0; step < steps; step++) {
  let G = math.zeros(totalSize, totalSize);
  let I = math.zeros(totalSize);
  let vsIdx = 0;
  let inductorIdx = 0;

  elements.forEach((el, idx) => {
    const n1 = nodeIndex[el.from];
    const n2 = nodeIndex[el.to];

    if (el.type === 'resistor') {
      const g = 1 / el.value;
      if (n1 !== 0) G.set([n1 - 1, n1 - 1], G.get([n1 - 1, n1 - 1]) + g);
      if (n2 !== 0) G.set([n2 - 1, n2 - 1], G.get([n2 - 1, n2 - 1]) + g);
      if (n1 !== 0 && n2 !== 0) {
        G.set([n1 - 1, n2 - 1], G.get([n1 - 1, n2 - 1]) - g);
        G.set([n2 - 1, n1 - 1], G.get([n2 - 1, n1 - 1]) - g);
      }

    } else if (el.type === 'capacitor') {
      const g = el.value / dt;
      const Ieq = g * el.state.voltage;
      if (n1 !== 0) {
        G.set([n1 - 1, n1 - 1], G.get([n1 - 1, n1 - 1]) + g);
        I.set([n1 - 1], I.get([n1 - 1]) + Ieq);
      }
      if (n2 !== 0) {
        G.set([n2 - 1, n2 - 1], G.get([n2 - 1, n2 - 1]) + g);
        I.set([n2 - 1], I.get([n2 - 1]) - Ieq);
      }
      if (n1 !== 0 && n2 !== 0) {
        G.set([n1 - 1, n2 - 1], G.get([n1 - 1, n2 - 1]) - g);
        G.set([n2 - 1, n1 - 1], G.get([n2 - 1, n1 - 1]) - g);
      }

    } else if (el.type === 'inductor') {
      const g = dt / el.value;
      const row = n + inductorIndexOffset + inductorIdx;

      if (n1 !== 0) {
        G.set([row, n1 - 1], 1);
        G.set([n1 - 1, row], 1);
      }
      if (n2 !== 0) {
        G.set([row, n2 - 1], -1);
        G.set([n2 - 1, row], -1);
      }

      G.set([row, row], -g);
      I.set([row], -el.state.current * g);

      inductorIdx++;

    } else if (el.type === 'voltage') {
      const row = n + vsIdx;
      if (n1 !== 0) {
        G.set([row, n1 - 1], 1);
        G.set([n1 - 1, row], 1);
      }
      if (n2 !== 0) {
        G.set([row, n2 - 1], -1);
        G.set([n2 - 1, row], -1);
      }
      I.set([row], el.value);
      vsIdx++;
    }
  });

  const V = math.lusolve(G, I);

  // ✅ Correct state updates
  let inductorRow = 0;
  elements.forEach((el) => {
    if (el.type === 'capacitor') {
      const v1 = el.from === '0' ? 0 : V[nodeIndex[el.from] - 1]?.[0] ?? 0;
      const v2 = el.to === '0' ? 0 : V[nodeIndex[el.to] - 1]?.[0] ?? 0;
      el.state.voltage = v1 - v2;
    } else if (el.type === 'inductor') {
      const row = n + inductorIndexOffset + inductorRow++;
      el.state.current = V[row]?.[0] ?? 0;
    }
  });

  // 📊 Print results
  console.log(`Step ${step}:`);
  nodes.slice(1).forEach((name, idx) => {
    console.log(`  V(${name}) = ${V[idx][0].toFixed(4)} V`);
  });
}
