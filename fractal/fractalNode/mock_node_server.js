// FractalNode Mock WS Server - Phase 1
const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 8080 });

console.log('Mock Node WS Server running on ws://localhost:8080');

// Global Graph Storage
let graph = {
  blocks: [],
  wires: []
};
let inputVariables = {};  // Global input variable store


// Internal Block Values
let blockValues = {};  // { blockID: value }
const fs = require('fs');
const path = require('path');

// Make sure graphs directory exists
const graphsDir = path.join(__dirname, 'graphs');
if (!fs.existsSync(graphsDir)) {
  fs.mkdirSync(graphsDir);
}


wss.on('connection', (ws) => {
  console.log('Client connected.');

  ws.on('message', (message) => {
    let parsed;
    try {
      parsed = JSON.parse(message);
    } catch (e) {
      console.error('Invalid JSON received:', e);
      return;
    }

    if (parsed.cmd === 'save_graph') {
        const filename = parsed.filename || 'saved_graph.json';
        const filepath = path.join(graphsDir, filename);
        fs.writeFileSync(filepath, JSON.stringify(parsed.graph, null, 2));
        console.log(`Saved graph as ${filename}`);
        ws.send(JSON.stringify({ cmd: 'save_graph_result', success: true, filename }));
    }
    else if (parsed.cmd === 'load_graph_file') {
        const filename = parsed.filename;
        const filepath = path.join(graphsDir, filename);
        if (fs.existsSync(filepath)) {
          const graph = JSON.parse(fs.readFileSync(filepath));
          ws.send(JSON.stringify({ cmd: 'load_graph_result', graph }));
          console.log(`Loaded graph ${filename}`);
        } else {
          ws.send(JSON.stringify({ cmd: 'load_graph_result', error: 'File not found' }));
        }
    }  
    else if (parsed.cmd === 'list_graph_files') {
        const files = fs.readdirSync(graphsDir).filter(f => f.endsWith('.json'));
        ws.send(JSON.stringify({ cmd: 'graph_list', files }));
    }

    else if (parsed.cmd === 'load_graph') {
        const filename = parsed.filename;
        const filepath = path.join(graphsDir, filename);
        if (fs.existsSync(filepath)) {
          const loadedGraph = JSON.parse(fs.readFileSync(filepath));
          graph.blocks = loadedGraph.blocks || [];
          graph.wires = loadedGraph.wires || [];
          inputVariables = loadedGraph.inputs || {};
          initializeBlockValues(graph.blocks);
          
          ws.send(JSON.stringify({
            cmd: "graph_loaded",
            graph: {
              blocks: graph.blocks,
              wires: graph.wires,
              inputs: inputVariables
            },
            filename: filename
          }));
          console.log(`Loaded graph ${filename}`);
        } else {
          ws.send(JSON.stringify({ cmd: "error", message: "File not found" }));
          console.error(`Load failed, file not found: ${filename}`);
        }
    }
    else if (parsed.cmd === 'run_step') {
        console.log('Running simulation step...');
        runStep(); // runs server-side

        ws.send(JSON.stringify({
            cmd: 'step_complete',
            blockValues: blockValues,  // <-- send block values!
            timestamp: Date.now()
        }));
        // ws.send(JSON.stringify({
        //   cmd: 'step_complete',
        //   inputs: inputVariables,  // send updated values if you want
        //   timestamp: Date.now()
        // }));
    }
    else if (parsed.cmd === 'set_input_variable') {
        inputVariables[parsed.id] = parsed.value;
        console.log(`Set input ${parsed.id} = ${parsed.value}`);
      }
    });

  ws.on('close', () => {
    console.log('Client disconnected.');
  });
});

// --- Helper Functions ---

function initializeBlockValues(blocks) {
  blockValues = {};
  blocks.forEach(block => {
    if (block.type === 'Constant') {
      blockValues[block.id] = block.config.value || 0;
    } else {
      blockValues[block.id] = 0; // Init others to zero
    }
  });
}

function evaluateGraph() {
  const idToBlock = {};
  graph.blocks.forEach(b => { idToBlock[b.id] = b; });

  // 2-Pass simple eval
  graph.blocks.forEach(block => {
    if (block.type === 'Input') {
        blockValues[block.id] = inputVariables[block.idName] || 0;
    }
  });
  graph.blocks.forEach(block => {
    if (block.type === 'Constant') {
      blockValues[block.id] = block.config.value || 0;
    }
  });

  graph.blocks.forEach(block => {
    if (block.type === 'Add' || block.type === 'Subtract' ||
        block.type === 'Multiply' || block.type === 'Divide') {
      
      const inputs = findInputs(block.id);
      const a = blockValues[inputs[0]] || 0;
      const b = blockValues[inputs[1]] || 0;

      let result = 0;
      if (block.type === 'Add') result = a + b;
      else if (block.type === 'Subtract') result = a - b;
      else if (block.type === 'Multiply') result = a * b;
      else if (block.type === 'Divide') result = (b !== 0) ? a / b : 0;

      blockValues[block.id] = result;
    }
  });

  // Outputs just hold value of first input
  graph.blocks.forEach(block => {
    if (block.type === 'Output') {
      const inputs = findInputs(block.id);
      const a = blockValues[inputs[0]] || 0;
      blockValues[block.id] = a;
    }
  });
}

function findInputs(targetId) {
  return graph.wires
    .filter(w => w.to === targetId)
    .map(w => w.from);
}
function runStep() {
    const blockMap = {};
    graph.blocks.forEach(block => blockMap[block.id] = block);
  
    const values = {};
  
    // Initialize inputs and constants
    graph.blocks.forEach(block => {
      if (block.type === 'Input') {
        values[block.id] = inputVariables[block.idName] || 0;
      }
      if (block.type === 'Constant') {
        values[block.id] = block.config.value || 0;
      }
    });
  
    // Process compute blocks
    graph.blocks.forEach(block => {
      if (['Add', 'Subtract', 'Multiply', 'Divide'].includes(block.type)) {
        const inputWires = graph.wires.filter(w => w.to === block.id);
  
        let in1 = 0, in2 = 0;
        inputWires.forEach(wire => {
          if (wire.input === 1) in1 = values[wire.from] || 0;
          if (wire.input === 2) in2 = values[wire.from] || 0;
        });
  
        if (block.type === 'Add') values[block.id] = in1 + in2;
        if (block.type === 'Subtract') values[block.id] = in1 - in2;
        if (block.type === 'Multiply') values[block.id] = in1 * in2;
        if (block.type === 'Divide') values[block.id] = (in2 !== 0) ? in1 / in2 : 0;
      }
    });
  
    // Handle Outputs
    graph.blocks.forEach(block => {
      if (block.type === 'Output') {
        const inputWires = graph.wires.filter(w => w.to === block.id);
        if (inputWires.length > 0) {
          const inputWire = inputWires[0];
          values[block.id] = values[inputWire.from] || 0;
        }
      }
    });
  
    blockValues = values;  // <-- Save updated block values globally
    console.log("Server Step values:", blockValues);
  }
  
function xxrunStep() {
    const blockMap = {};
    graph.blocks.forEach(block => blockMap[block.id] = block);
  
    const values = {};
  
    // Initialize inputs and constants
    graph.blocks.forEach(block => {
      if (block.type === 'Input') {
        values[block.id] = inputVariables[block.idName] || 0;
      }
      if (block.type === 'Constant') {
        values[block.id] = block.config.value || 0;
      }
    });
  
    // Process compute blocks
    graph.blocks.forEach(block => {
      if (['Add', 'Subtract', 'Multiply', 'Divide'].includes(block.type)) {
        const inputWires = graph.wires.filter(w => w.to === block.id);
  
        let in1 = null, in2 = null;
        inputWires.forEach(wire => {
          if (wire.toInput === 1) in1 = values[wire.fromBlock];
          if (wire.toInput === 2) in2 = values[wire.fromBlock];
        });
  
        if (in1 !== null && in2 !== null) {
          if (block.type === 'Add') values[block.id] = in1 + in2;
          if (block.type === 'Subtract') values[block.id] = in1 - in2;
          if (block.type === 'Multiply') values[block.id] = in1 * in2;
          if (block.type === 'Divide') values[block.id] = (in2 !== 0) ? in1 / in2 : 0;
        }
      }
    });
  
    // Handle Outputs
    graph.blocks.forEach(block => {
      if (block.type === 'Output') {
        const inputWires = graph.wires.filter(w => w.toBlock === block.id);
        if (inputWires.length > 0) {
          const inputBlock = inputWires[0].fromBlock;
          values[block.id] = values[inputBlock] || 0;
        }
      }
    });
  
    // Update inputVariables if needed
    graph.blocks.forEach(block => {
      if (block.type === 'Output') {
        inputVariables[block.idName] = values[block.id];
      }
    });
  
    console.log("Server Step values:", values);
  }

  