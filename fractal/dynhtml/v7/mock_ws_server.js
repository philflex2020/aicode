const WebSocket = require('ws');
const fs = require('fs');
const path = require('path');

const PORT = process.argv[2] ? parseInt(process.argv[2]) : 8080;
const SYSTEMS_DIR = path.join(__dirname, 'systems');
const DEFAULT_SYSTEM = 'default';

const wss = new WebSocket.Server({ port: PORT });

console.log(`WebSocket server listening on ws://localhost:${PORT}`);

//--------------------------------------
// Utility Functions
//--------------------------------------

function ensureDir(dirPath) {
  if (!fs.existsSync(dirPath)) {
    fs.mkdirSync(dirPath, { recursive: true });
  }
}

function ensureDefaultSystem() {
  const defaultPath = path.join(SYSTEMS_DIR, DEFAULT_SYSTEM);
  const structureFile = path.join(defaultPath, 'structure.json');
  const templateFile = path.join(defaultPath, 'template.json');

  ensureDir(defaultPath);

  if (!fs.existsSync(structureFile)) {
    fs.writeFileSync(structureFile, JSON.stringify([], null, 2)); // Empty structure
    console.log('Created default structure.json');
  }

  if (!fs.existsSync(templateFile)) {
    fs.writeFileSync(templateFile, JSON.stringify({}, null, 2)); // Empty templates
    console.log('Created default template.json');
  }
}

function listSystems() {
  ensureDir(SYSTEMS_DIR);
  return fs.readdirSync(SYSTEMS_DIR).filter(entry => {
    return fs.statSync(path.join(SYSTEMS_DIR, entry)).isDirectory();
  });
}

function loadSystem(systemName) {
  const systemPath = path.join(SYSTEMS_DIR, systemName);
  const structureFile = path.join(systemPath, 'structure.json');
  const templateFile = path.join(systemPath, 'template.json');

  if (!fs.existsSync(structureFile) || !fs.existsSync(templateFile)) {
    return null;
  }

  const structure = JSON.parse(fs.readFileSync(structureFile));
  const templates = JSON.parse(fs.readFileSync(templateFile));
  return { structure, templates };
}

function saveSystem(systemName, structure, templates) {
  const systemPath = path.join(SYSTEMS_DIR, systemName);
  ensureDir(systemPath);

  const structureFile = path.join(systemPath, 'structure.json');
  const templateFile = path.join(systemPath, 'template.json');

  fs.writeFileSync(structureFile, JSON.stringify(structure, null, 2));
  fs.writeFileSync(templateFile, JSON.stringify(templates, null, 2));
}

//--------------------------------------
// Initialize
//--------------------------------------
ensureDir(SYSTEMS_DIR);
ensureDefaultSystem();

//--------------------------------------
// WebSocket Message Handling
//--------------------------------------
wss.on('connection', function connection(ws) {
  console.log('Client connected.');

  ws.on('message', function incoming(message) {
    console.log('Received:', message.toString());

    try {
      const msg = JSON.parse(message);

      if (msg.cmd === 'request_module') {
        if (!msg.module_name) {
          ws.send(JSON.stringify({ cmd: "error", message: "Missing module_name." }));
          return;
        }
      
        const moduleFile = path.join(__dirname, msg.module_name + '.js');
      
        if (fs.existsSync(moduleFile)) {
          const code = fs.readFileSync(moduleFile, 'utf-8');
          ws.send(JSON.stringify({ cmd: "upload_module", module_name: msg.module_name, code }));
        } else {
          ws.send(JSON.stringify({ cmd: "error", message: `Module '${msg.module_name}.js' not found.` }));
        }
      }
      
      else if (msg.cmd === 'list_systems') {
        try {
          const systemsList = fs.readdirSync(SYSTEMS_DIR)
            .filter(name => fs.existsSync(path.join(SYSTEMS_DIR, name, 'structure.json')))
            .sort();
          
            ws.send(JSON.stringify({ cmd: "list_systems_done", systems: systemsList, forSaveAs: msg.forSaveAs || false }));
        } catch (err) {
          ws.send(JSON.stringify({ cmd: "error", message: `Failed to list systems: ${err.message}` }));
        }
    //   }
      
    //   else if (msg.cmd === 'list_systems') {
    //     const systems = listSystems();
    //     ws.send(JSON.stringify({ cmd: 'list_systems_result', systems }));

      } else if (msg.cmd === 'load_system') {
        const data = loadSystem(msg.system_name);
        if (data) {
          ws.send(JSON.stringify({ cmd: 'load_system_result', system: msg.system_name, structure: data.structure, templates: data.templates }));
        } else {
          ws.send(JSON.stringify({ cmd: 'error', message: `System '${msg.system_name}' not found.` }));
        }

      } else if (msg.cmd === 'save_system' || msg.cmd === 'saveas_system') {
        if (!msg.system_name) {
            console.log("save system error: system_name");
        }
        if (!msg.structure ) {
            console.log("save system error: structure");
        }
        if (!msg.templates) {
            console.log("save system error: templates");
        }
        if (!msg.system_name || !msg.structure || !msg.templates) {
            console.log("save system error");
            ws.send(JSON.stringify({ cmd: 'error', message: 'Invalid save_system/saveas_system payload.' }));
          return;
        }
        saveSystem(msg.system_name, msg.structure, msg.templates);
        console.log("system saved");
        ws.send(JSON.stringify({ cmd: 'save_system_result', system: msg.system_name }));

      } else {
        ws.send(JSON.stringify({ cmd: 'error', message: 'Unknown command.' }));
        console.log("system save error");
      }

    } catch (e) {
      console.error('Invalid message:', e);
      ws.send(JSON.stringify({ cmd: 'error', message: 'Invalid JSON.' }));
    }
  });

  ws.on('close', () => {
    console.log('Client disconnected.');
  });
});
