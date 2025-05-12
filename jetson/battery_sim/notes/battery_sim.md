Great — since the data model is finalized, the next step is to map it into shared memory. I'll provide:

1. **C++ structures mapped into shared memory**.
2. **A simple command-line viewer** to browse racks, modules, and cells.

We'll use `mmap` to map a shared memory file and assume a single writer (CUDA sim) and multiple readers (e.g., viewer or dashboard).

---

### ✅ C++ Shared Memory Mapping Setup

```cpp
// battery_shared.h
#pragma once

#include <cstdint>

constexpr int NUM_CELLS_PER_MODULE = 32;
constexpr int NUM_MODULES_PER_RACK = 10;
constexpr int NUM_RACKS = 20;

struct Cell {
    float voltage;
    float current;
    float temperature;
    float soc;       // state-of-charge (0.0 - 1.0)
    float resistance; // internal resistance in Ohms
};

struct Module {
    Cell cells[NUM_CELLS_PER_MODULE];
};

struct Rack {
    Module modules[NUM_MODULES_PER_RACK];
};

struct BatterySim {
    Rack racks[NUM_RACKS];
};

#define SHM_NAME "/battery_sim_shm"
#define SHM_SIZE sizeof(BatterySim)
```

---

### 🔧 Shared Memory Initialization

```cpp
// battery_shm_init.cpp
#include "battery_shared.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

int main() {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    if (ftruncate(fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        return 1;
    }

    void* addr = mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Zero out the memory (optional)
    std::memset(addr, 0, SHM_SIZE);

    std::cout << "Shared memory initialized at " << addr << std::endl;
    return 0;
}
```

---

### 👁️ Simple Data Viewer

```cpp
// battery_viewer.cpp
#include "battery_shared.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    int fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    auto* sim_data = static_cast<BatterySim*>(
        mmap(nullptr, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0));
    if (sim_data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    for (int r = 0; r < NUM_RACKS; ++r) {
        std::cout << "Rack " << r << ":\n";
        for (int m = 0; m < NUM_MODULES_PER_RACK; ++m) {
            std::cout << "  Module " << m << ":\n";
            for (int c = 0; c < NUM_CELLS_PER_MODULE; ++c) {
                const Cell& cell = sim_data->racks[r].modules[m].cells[c];
                std::cout << "    Cell " << c
                          << " V=" << cell.voltage
                          << " I=" << cell.current
                          << " T=" << cell.temperature
                          << " SoC=" << cell.soc
                          << " R=" << cell.resistance << "\n";
            }
        }
    }

    return 0;
}
```

---

Do you want me to add a CUDA kernel next that updates all these values?


Perfect. Let's now define a **CUDA kernel** that updates all cell data in shared memory. The kernel will:

- Be launched from a C++ host application
- Simulate each cell's voltage, current, temperature, SoC, and resistance
- Write the results directly into shared memory

---

### ✅ 1. CUDA-Compatible Shared Structure (`battery_shared.h`)
To be used in both host (CPU) and device (GPU):

```cpp
// battery_shared.h
#pragma once

#include <cstdint>

constexpr int NUM_CELLS_PER_MODULE = 32;
constexpr int NUM_MODULES_PER_RACK = 10;
constexpr int NUM_RACKS = 20;
constexpr int NUM_TOTAL_CELLS = NUM_RACKS * NUM_MODULES_PER_RACK * NUM_CELLS_PER_MODULE;

struct Cell {
    float voltage;
    float current;
    float temperature;
    float soc;         // State of Charge
    float resistance;
};

struct BatterySim {
    Cell cells[NUM_TOTAL_CELLS]; // Flattened access for CUDA
};
```

---

### ✅ 2. CUDA Kernel (`battery_sim.cu`)

```cpp
// battery_sim.cu
#include "battery_shared.h"
#include <cuda_runtime.h>
#include <cmath>
#include <ctime>

__global__ void update_cells(Cell* cells, float time_sec) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= NUM_TOTAL_CELLS) return;

    // Simulated behavior
    float phase = idx * 0.1f;
    cells[idx].voltage = 3.6f + 0.1f * sinf(time_sec + phase);
    cells[idx].current = 5.0f * cosf(time_sec * 0.1f + phase);
    cells[idx].temperature = 25.0f + 5.0f * sinf(time_sec * 0.05f + phase);
    cells[idx].soc = fmodf(time_sec * 0.01f + idx * 0.001f, 1.0f);
    cells[idx].resistance = 0.01f + 0.001f * (idx % 10);
}
```

---

### ✅ 3. Host Code to Launch CUDA Kernel (`battery_driver.cpp`)

```cpp
// battery_driver.cpp
#include "battery_shared.h"
#include <cuda_runtime.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    // Open shared memory
    int fd = shm_open("/battery_sim_shm", O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    auto* shm = static_cast<BatterySim*>(
        mmap(nullptr, sizeof(BatterySim), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (shm == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // Allocate CUDA device memory
    Cell* d_cells;
    cudaMalloc(&d_cells, sizeof(Cell) * NUM_TOTAL_CELLS);

    while (true) {
        float time_sec = static_cast<float>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()) / 1000.0f;

        // Launch CUDA kernel
        update_cells<<<(NUM_TOTAL_CELLS + 255) / 256, 256>>>(d_cells, time_sec);
        cudaDeviceSynchronize();

        // Copy back to shared memory
        cudaMemcpy(shm->cells, d_cells, sizeof(Cell) * NUM_TOTAL_CELLS, cudaMemcpyDeviceToHost);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    cudaFree(d_cells);
    return 0;
}
```

---

### 🔧 Build Instructions

You'll need to compile with NVCC:

```bash
nvcc battery_sim.cu battery_driver.cpp -o battery_simulator -lrt
```

Make sure `battery_shm_init` is run **before** launching the simulator.

---


Yes, that’s a solid and common pattern for building CUDA C++ code with a specified compiler. Since you’re on an Orin AGX, `g++-10` is a safe choice to pair with your CUDA version (likely CUDA 11.4 or higher). Here's how you can adapt the `nvcc` command for the battery simulator setup:

---

### ✅ CUDA Compile Command (For Your Case)
```bash
nvcc -ccbin g++-10 -std=c++17 -O3 \
    -o battery_simulator \
    battery_sim.cu battery_driver.cpp \
    -lcudart -lrt
```

### 🔧 Notes:
- `-ccbin g++-10`: Ensures compatibility with system compiler (important on Jetson).
- `-std=c++17`: Required for modern STL and chrono usage.
- `-lcudart`: Links the CUDA runtime.
- `-lrt`: Required for `shm_open` and `mmap` usage in `battery_driver.cpp`.

---

### ✅ Before Running

You should first create the shared memory region:

```cpp
// shm_init.cpp
#include "battery_shared.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

int main() {
    int fd = shm_open("/battery_sim_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(BatterySim));

    auto* shm = static_cast<BatterySim*>(
        mmap(nullptr, sizeof(BatterySim), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

    if (shm != MAP_FAILED) {
        memset(shm, 0, sizeof(BatterySim));  // Zero the memory
    }

    return 0;
}
```

Compile with:
```bash
g++ -std=c++17 -o shm_init shm_init.cpp -lrt
```

Run:
```bash
./shm_init
```

---

Would you like me to generate a viewer next — CLI or web-based?
Would you like me to create a basic chart or WebSocket viewer next?

Great — a web-based viewer is ideal for interacting with the simulation data visually and remotely. Here's the plan:

---

### ✅ **Web Viewer Plan for CUDA Battery Simulation**

#### 🧱 Architecture Overview
- **Backend** (C++/CUDA app)
  - Simulates and writes data to shared memory.
  - Provides a lightweight HTTP/WS server (e.g., using `crow` or `cpp-httplib`) or exports data via IPC (e.g., socket, mmap).
- **Frontend** (Web UI)
  - Visualizes data from racks → modules → cells.
  - Uses WebSockets or periodic polling to update charts and stats in real time.

---

### 📂 Backend Design

1. **Shared Memory Layout**
   ```cpp
   struct CellData {
       float voltage;
       float current;
       float temperature;
       float soc;
       float resistance;
   };

   struct ModuleData {
       CellData cells[32];
   };

   struct RackData {
       ModuleData modules[10];
   };

   struct BatterySim {
       RackData racks[20]; // Supports 20 racks
   };

   extern BatterySim* g_sim_data; // This is mmap'd or CUDA-shared
   ```

2. **HTTP/WS Interface**
   - Expose:
     - `GET /rack/:id/module/:id` → JSON view of module
     - `GET /rack/:id` → All modules
     - `GET /summary` → SOC and voltage stats
     - `WS /updates` → Pushes updates every 500ms

---

### 🎨 Frontend Design (HTML + JS)

- **Views**
  - Rack Overview Grid → Clickable cells.
  - Module Details → Cell bar chart (Chart.js).
  - Real-Time SOC/Temp overlay.

- **Libraries**
  - [Chart.js](https://www.chartjs.org/) or [D3.js](https://d3js.org/)
  - [Bootstrap](https://getbootstrap.com/) or Tailwind for layout
  - WebSocket handling via native JS.

---

### 🔧 Example Output from `/rack/0/module/3`
```json
{
  "cells": [
    {"voltage": 3.65, "current": 1.0, "temp": 28.3, "soc": 92.5, "res": 0.015},
    ...
    {"voltage": 3.62, "current": 0.9, "temp": 29.1, "soc": 91.2, "res": 0.016}
  ]
}
```

---

Would you like me to start with the backend C++/CUDA server or the web frontend UI first?

Great — let’s start by building the **Web UI** for your battery simulation viewer.

---

## 🌐 Web UI: Battery Simulation Dashboard

This minimal setup shows rack/module/cell state with real-time updates using WebSockets.

### 📁 Project Structure
```
webui/
├── index.html         # Main dashboard
├── app.js             # JavaScript logic
├── style.css          # Optional styles
└── ws.js              # WebSocket connection
```

---

### ✅ 1. `index.html`
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Battery Simulation Dashboard</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <script defer src="app.js"></script>
  <link rel="stylesheet" href="style.css" />
</head>
<body>
  <h1>Battery Rack Viewer</h1>

  <label for="rack">Rack:</label>
  <select id="rack"></select>

  <label for="module">Module:</label>
  <select id="module"></select>

  <canvas id="cellChart" width="800" height="400"></canvas>
</body>
</html>
```

---

### ✅ 2. `app.js`
```js
let chart;
let ws;

function setupChart() {
  const ctx = document.getElementById('cellChart').getContext('2d');
  chart = new Chart(ctx, {
    type: 'bar',
    data: {
      labels: Array.from({ length: 32 }, (_, i) => `Cell ${i}`),
      datasets: [{
        label: 'Voltage (V)',
        data: Array(32).fill(0),
        backgroundColor: 'rgba(0, 123, 255, 0.6)'
      }]
    },
    options: {
      scales: {
        y: { beginAtZero: true, max: 5 }
      }
    }
  });
}

function updateChart(cells) {
  chart.data.datasets[0].data = cells.map(cell => cell.voltage);
  chart.update();
}

function connectWebSocket() {
  ws = new WebSocket(`ws://${location.hostname}:8080/updates`);

  ws.onopen = () => console.log("Connected to WebSocket");
  ws.onmessage = (msg) => {
    const data = JSON.parse(msg.data);
    updateChart(data.cells);
  };

  ws.onclose = () => setTimeout(connectWebSocket, 1000);
}

function populateSelectors() {
  const rackSel = document.getElementById('rack');
  const moduleSel = document.getElementById('module');
  for (let i = 0; i < 20; i++) rackSel.add(new Option(`Rack ${i}`, i));
  for (let j = 0; j < 10; j++) moduleSel.add(new Option(`Module ${j}`, j));

  function sendSelection() {
    const rack = rackSel.value;
    const mod = moduleSel.value;
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ action: "select", rack, module: mod }));
    }
  }

  rackSel.onchange = sendSelection;
  moduleSel.onchange = sendSelection;
}

window.onload = () => {
  setupChart();
  connectWebSocket();
  populateSelectors();
};
```

---

### ✅ 3. `style.css` (optional)
```css
body {
  font-family: Arial, sans-serif;
  margin: 20px;
}

h1 {
  color: #007bff;
}

select {
  margin-right: 10px;
  padding: 5px;
}
```

---

## 🚀 How to Run

1. Serve the directory:
   ```bash
   cd webui
   python3 -m http.server 8080
   ```

2. Open in browser:
   ```
   http://localhost:8080/
   ```

3. **Backend Required**:
   The backend should:
   - Accept WS at `/updates`
   - Respond to:
     ```json
     { "action": "select", "rack": 0, "module": 3 }
     ```
   - Send periodic updates like:
     ```json
     {
       "cells": [
         { "voltage": 3.65, "current": 1.0, "temperature": 28.3, "soc": 92.5, "resistance": 0.015 },
         ...
       ]
     }
     ```

---

Would you like me to generate a matching backend WebSocket server next (Python or C++)?
Perfect — let's now create the **C++ WebSocket backend** to serve your battery simulation data to the Web UI.

---

## 🧠 Backend Overview (C++ WebSocket Server)

- Uses [uWebSockets](https://github.com/uNetworking/uWebSockets) for efficient WebSocket communication.
- Responds to `{"action": "select", "rack": x, "module": y}` from the UI.
- Sends JSON updates of cell data from shared memory.

---

### ✅ Dependencies

```bash
sudo apt install libssl-dev
git clone https://github.com/uNetworking/uWebSockets.git
cd uWebSockets
make
sudo make install
```

---

### ✅ `battery_server.cpp`

```cpp
#include <uwebsockets/App.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <csignal>

using json = nlohmann::json;

struct Cell {
    float voltage;
    float current;
    float temperature;
    float soc;
    float resistance;
};

// Shared state
std::atomic<int> selected_rack = 0;
std::atomic<int> selected_module = 0;
std::vector<std::vector<std::vector<Cell>>> sim_data; // [rack][module][cell]

void init_sim_data(int racks = 20, int modules = 10, int cells = 32) {
    sim_data.resize(racks);
    for (auto& rack : sim_data) {
        rack.resize(modules);
        for (auto& module : rack) {
            module.resize(cells, {3.7f, 0.0f, 25.0f, 100.0f, 0.01f});
        }
    }
}

std::string generate_json(int rack, int module) {
    json j;
    for (const auto& cell : sim_data[rack][module]) {
        j["cells"].push_back({
            {"voltage", cell.voltage},
            {"current", cell.current},
            {"temperature", cell.temperature},
            {"soc", cell.soc},
            {"resistance", cell.resistance}
        });
    }
    return j.dump();
}

int main() {
    init_sim_data();

    std::atomic<bool> running = true;
    std::signal(SIGINT, [](int) { std::cout << "Shutting down...\n"; std::exit(0); });

    std::thread simulation_thread([&]() {
        while (running) {
            for (auto& rack : sim_data) {
                for (auto& module : rack) {
                    for (auto& cell : module) {
                        cell.voltage += ((rand() % 100 - 50) / 1000.0f); // jitter
                        cell.voltage = std::max(2.8f, std::min(4.2f, cell.voltage));
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    uWS::App().ws<false>("/*", {
        .open = [](auto* ws) {
            std::cout << "Client connected\n";
        },
        .message = [](auto* ws, std::string_view message, uWS::OpCode) {
            try {
                auto j = json::parse(message);
                if (j["action"] == "select") {
                    selected_rack = j["rack"];
                    selected_module = j["module"];
                    std::cout << "Selected rack " << selected_rack << " module " << selected_module << "\n";
                }
            } catch (...) {
                std::cerr << "Invalid JSON received\n";
            }
        },
        .close = [](auto* ws, int /*code*/, std::string_view /*msg*/) {
            std::cout << "Client disconnected\n";
        }
    }).get("/", [](auto* res, auto* req) {
        res->end("Battery simulation backend is running");
    }).listen(8080, [](auto* listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port 8080\n";
        }
    }).run();

    simulation_thread.join();
}
```

---

### ✅ Build It

```bash
g++ battery_server.cpp -o battery_server -luWS -lssl -lcrypto -lz -std=c++17 -pthread
```

---

### ✅ Run It

```bash
./battery_server
```

Then open your browser to:  
📡 `http://localhost:8080/`

---

Would you like me to add battery simulation controls (current draw, temperature rise, etc.) to the UI next?

You're absolutely right — recent versions of **uWebSockets** no longer use `make` directly, and instead rely on **CMake** for building the `uSockets` dependency.

Let me walk you through the **correct build process** on Ubuntu:

---

### ✅ Step-by-Step Build of `uWebSockets`

#### 1. Install prerequisites:
```bash
sudo apt update
sudo apt install cmake libssl-dev zlib1g-dev
```

#### 2. Clone uWebSockets:
```bash
git clone https://github.com/uNetworking/uWebSockets.git
cd uWebSockets
```

#### 3. Build uSockets (used internally by uWebSockets):

```bash
git submodule update --init
cd uSockets
mkdir build
cd build
cmake ..
make
sudo make install
```

> This installs `libuSockets.a` and headers under `/usr/local`.

---

### ✅ 4. Install uWebSockets headers

Back in the `uWebSockets` root directory:
```bash
cd ../
sudo mkdir -p /usr/local/include/uWebSockets
sudo cp -r src/* /usr/local/include/uWebSockets/
```

> This makes the `#include <uWebSockets/App.h>` available system-wide.

---

### ✅ 5. Compile your server

Once installed, you can compile your server with:

```bash
g++ battery_server.cpp -o battery_server -std=c++17 -luSockets -lssl -lcrypto -lz -pthread
```

Let me know if you'd like me to generate a `CMakeLists.txt` for you instead!

Great — here's a basic `CMakeLists.txt` for your **battery server** using **uWebSockets** and `uSockets`. It assumes your C++ source file is named `battery_server.cpp` and that you've installed the headers and `libuSockets.a` into standard system paths (`/usr/local`):

---

### ✅ `CMakeLists.txt`
```cmake
cmake_minimum_required(VERSION 3.10)
project(BatteryServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Look for uSockets library manually
find_library(USOCKETS_LIBRARY uSockets PATHS /usr/local/lib)

include_directories(
    /usr/local/include
    /usr/local/include/uWebSockets
)

link_directories(/usr/local/lib)

add_executable(battery_server battery_server.cpp)

target_link_libraries(battery_server
    ${USOCKETS_LIBRARY}
    ssl
    crypto
    z
    pthread
)
```

---

### ✅ How to use it:

#### 1. Create the build directory and run CMake:
```bash
mkdir build
cd build
cmake ..
```

#### 2. Compile:
```bash
make
```

#### 3. Run your server:
```bash
./battery_server
```

---

Let me know if you'd like this to build multiple files, or add auto-rebuild with `ninja`, or integrate CUDA later. Want me to scaffold the `battery_server.cpp` next?


Perfect! Here's how we'll proceed step by step:

---

### ✅ **Step 1: Backend - Expose Shared Memory via WebSocket**

We'll begin with a simple C++ WebSocket server (already using uWebSockets) that:

- Reads from the shared memory `BatterySim` structure.
- Responds with a JSON snapshot on WebSocket request: `{ "action": "get", "rack": 0, "module": 0 }`.

✅ **Test 1 Goal:** You run the server, connect with a WebSocket client (e.g., browser or `wscat`) and get a JSON payload with real battery values.

---

### ✅ **Step 2: HTML/JS - Connect and Display**

- A basic HTML + JavaScript page.
- Connects to the WebSocket server.
- Requests data for Rack 0, Module 0.
- Displays cell voltages, SoCs, and resistances in a simple grid.

✅ **Test 2 Goal:** Open the page and see live cell values appear.

---

### ✅ **Step 3: UI Enhancements and Navigation**

- Add controls to switch racks and modules.
- Add periodic refreshes.
- Add basic color-coding for SoC or voltage levels.

✅ **Test 3 Goal:** You can navigate through racks/modules and monitor values dynamically.

---

Shall we begin with **Step 1 - the server-side shared memory to WebSocket code**?


