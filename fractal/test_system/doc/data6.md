
Let's fix the connection issues. Here's the corrected version with proper connection handling and debugging:

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Dashboard</title>
    <style>
        /* ... [Keep all previous styles] ... */
    </style>
</head>
<body>
    <form id="configForm" class="config-form">
        <div class="config-input-group">
            <input type="text" id="ipInput" class="config-input" placeholder="IP Address" value="localhost">
            <input type="text" id="portInput" class="config-input" placeholder="Port" value="9001">
            <button type="submit" class="connect-btn">Connect</button>
        </div>
    </form>
    <div id="tabs-container" class="tabs"></div>
    <div id="data-container" class="data-container"></div>
    <div id="title-container" class="title-container"></div>
    <div id="status-message" class="status-message"></div>

    <script>
        let ws = null;
        let activeTabID = null;
        let lastConfig = null;

        const testData = { 
            Configuration: {
                SystemInfo: {
                    Name: "Test Cluster",
                    Version: "2.1.4",
                    Architecture: "x86_64"
                },
                Network: {
                    IP: "192.168.1.100",
                    Subnet: "255.255.255.0",
                    DNS: ["8.8.8.8", "8.8.4.4"]
                }
            },
            RBMS: [
                {
                    RackID: 1,
                    Status: {
                        Health: "Normal",
                        Temperature: "22°C",
                        PowerUsage: "450W"
                    }
                }
            ]
        };

        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();
            initializeConnection();
        });

        function initializeConnection() {
            const ip = document.getElementById('ipInput').value;
            const port = document.getElementById('portInput').value;
            
            // Clear previous connection
            if(ws) {
                ws.close();
                ws = null;
            }

            // Reset UI
            tabsContainer.innerHTML = '';
            dataContainer.innerHTML = '';
            titleContainer.innerHTML = '';
            statusMessage.style.display = 'none';

            try {
                ws = new WebSocket(`ws://${ip}:${port}/ws`);
                let connected = false;

                const connectionTimeout = setTimeout(() => {
                    if (!connected) {
                        showStatus("Connection timeout. Using test data.");
                        loadTestData();
                    }
                }, 2000);

                ws.onopen = function() {
                    clearTimeout(connectionTimeout);
                    connected = true;
                    console.log("WebSocket connected");
                    statusMessage.style.display = 'none';
                    ws.send("config?");
                };

                ws.onmessage = function(event) {
                    console.log("Received data:", event.data);
                    try {
                        const data = JSON.parse(event.data);
                        if (data.Configuration) {
                            lastConfig = data.Configuration;
                            displayConfiguration(lastConfig);
                        }
                        if (data.RBMS) {
                            updateRacks(data.RBMS);
                        }
                    } catch (e) {
                        console.error("JSON parse error:", e);
                    }
                };

                ws.onerror = function(error) {
                    console.error("WebSocket error:", error);
                    showStatus("Connection error. Using test data.");
                    if (!connected) loadTestData();
                };

                ws.onclose = function() {
                    console.log("WebSocket closed");
                    if (!connected) {
                        showStatus("Connection closed. Using test data.");
                        loadTestData();
                    }
                };

            } catch (error) {
                console.error("Connection failed:", error);
                showStatus("Invalid connection parameters. Using test data.");
                loadTestData();
            }
        }

        function loadTestData() {
            console.log("Loading test data");
            displayConfiguration(testData.Configuration);
            updateRacks(testData.RBMS);
            showStatus("Connected to test data. Displaying simulated racks.", false);
        }

        function displayConfiguration(config) {
            titleContainer.innerHTML = '';
            const configContainer = document.createElement('div');
            renderNestedObject(config, configContainer);
            titleContainer.appendChild(configContainer);
            titleContainer.style.display = 'block';
        }

        function renderNestedObject(obj, container, depth = 0) {
            Object.entries(obj).forEach(([key, value]) => {
                const entryDiv = document.createElement('div');
                entryDiv.className = 'data-item';
                entryDiv.style.paddingLeft = `${depth * 15}px`;

                const titleDiv = document.createElement('div');
                titleDiv.className = 'data-title';
                titleDiv.textContent = key;

                const contentDiv = document.createElement('div');
                contentDiv.style.flexGrow = '1';

                if (typeof value === 'object' && value !== null) {
                    contentDiv.classList.add('nested-object');
                    renderNestedObject(value, contentDiv, depth + 1);
                } else {
                    contentDiv.textContent = Array.isArray(value) 
                        ? value.join(', ') 
                        : value;
                }

                entryDiv.appendChild(titleDiv);
                entryDiv.appendChild(contentDiv);
                container.appendChild(entryDiv);
            });
        }

        function updateRacks(racks) {
            tabsContainer.innerHTML = '';
            createTab('config', 'Configuration', () => displayConfiguration(lastConfig));
            racks.forEach((rack) => {
                createTab(`rack-${rack.RackID}`, `Rack ${rack.RackID}`, () => displayRackData(rack));
            });
            setActiveTab();
        }

        function createTab(id, label, onClick) {
            const tab = document.createElement('div');
            tab.id = id;
            tab.className = 'tab';
            tab.textContent = label;
            tab.onclick = () => {
                document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
                tab.classList.add('active');
                activeTabID = id;
                dataContainer.innerHTML = '';
                titleContainer.innerHTML = '';
                
                if (id === 'config') {
                    titleContainer.style.display = 'block';
                    dataContainer.style.display = 'none';
                } else {
                    titleContainer.style.display = 'none';
                    dataContainer.style.display = 'block';
                }
                
                onClick();
            };
            tabsContainer.appendChild(tab);
        }

        function setActiveTab() {
            const defaultTab = document.getElementById(activeTabID) || document.getElementById('config');
            if (defaultTab) defaultTab.click();
        }

        function displayRackData(rack) {
            dataContainer.innerHTML = '';
            Object.entries(rack).forEach(([key, value]) => {
                const dataItem = document.createElement('div');
                dataItem.className = 'data-item';
                
                const titleDiv = document.createElement('div');
                titleDiv.className = 'data-title';
                titleDiv.textContent = key;
                
                const contentDiv = document.createElement('div');
                contentDiv.style.flexGrow = '1';

                if (typeof value === 'object' && value !== null) {
                    contentDiv.classList.add('nested-object');
                    renderNestedObject(value, contentDiv);
                } else {
                    contentDiv.textContent = value;
                }

                dataItem.appendChild(titleDiv);
                dataItem.appendChild(contentDiv);
                dataContainer.appendChild(dataItem);
            });
        }

        function showStatus(message, isError = true) {
            statusMessage.textContent = message;
            statusMessage.style.display = 'block';
            statusMessage.style.backgroundColor = isError ? '#f8d7da' : '#d4edda';
            statusMessage.style.borderColor = isError ? '#f5c6cb' : '#c3e6cb';
        }

        // Initial connection attempt
        loadTestData();
    </script>
</body>
</html>
```

Key improvements:
1. Added proper connection cleanup when reconnecting
2. Added console logging for debugging
3. Fixed test data loading flow
4. Immediate test data fallback on connection errors
5. Better error handling for JSON parsing
6. Added proper WebSocket cleanup
7. Reset UI state when connecting
8. Set default IP to "localhost" for easier testing

Now when you click Connect:
1. It will first clear any existing connection
2. Try to connect to the specified WebSocket server
3. If no connection after 2 seconds, shows test data
4. If connection fails for any reason, shows test data
5. Console shows detailed connection status

To test:
1. Without any server - should show test data after 2 seconds
2. With invalid IP/port - shows test data immediately
3. With valid server - shows real data

The test data will always load if no connection is established, and the tabs/configuration should display properly in all cases.

Here's the enhanced version with IP/port configuration and improved complex object handling:

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Dashboard</title>
    <style>
        /* ... [Keep previous styles] ... */
        .config-form {
            margin-bottom: 20px;
            padding: 15px;
            background-color: #fff;
            border-radius: 5px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .config-input-group {
            display: flex;
            gap: 10px;
            margin-bottom: 10px;
        }
        .config-input {
            padding: 8px;
            border: 1px solid #90caf9;
            border-radius: 4px;
            width: 200px;
        }
        .connect-btn {
            padding: 8px 20px;
            background-color: #2196f3;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            transition: background-color 0.2s;
        }
        .connect-btn:hover {
            background-color: #1976d2;
        }
        .nested-object {
            padding-left: 20px;
            border-left: 2px solid #90caf9;
            margin: 8px 0;
        }
        .config-value {
            font-family: monospace;
            white-space: pre-wrap;
        }
    </style>
</head>
<body>
    <form id="configForm" class="config-form">
        <div class="config-input-group">
            <input type="text" id="ipInput" class="config-input" placeholder="IP Address" value="192.168.86.209">
            <input type="text" id="portInput" class="config-input" placeholder="Port" value="9001">
            <button type="submit" class="connect-btn">Connect</button>
        </div>
    </form>
    <div id="tabs-container" class="tabs"></div>
    <div id="data-container" class="data-container"></div>
    <div id="title-container" class="title-container"></div>
    <div id="status-message" class="status-message"></div>

    <script>
        let ws = null;
        let activeTabID = null;
        let lastConfig = null;

        const testData = {
            Configuration: {
                SystemInfo: {
                    Name: "Test Cluster",
                    Version: "2.1.4",
                    Architecture: "x86_64"
                },
                Network: {
                    IP: "192.168.1.100",
                    Subnet: "255.255.255.0",
                    DNS: ["8.8.8.8", "8.8.4.4"]
                },
                Hardware: {
                    Racks: 4,
                    NodesPerRack: 8,
                    PowerSupply: {
                        Type: "Dual 2400W",
                        Voltage: "230V"
                    }
                }
            },
            RBMS: [
                {
                    RackID: 1,
                    Status: {
                        Health: "Normal",
                        Temperature: "22°C",
                        PowerUsage: "450W"
                    },
                    Sensors: {
                        Voltage: "230V ±2%",
                        Current: "2.1A",
                        FanSpeeds: ["2400 RPM", "2350 RPM", "2450 RPM"]
                    }
                }
            ]
        };

        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();
            initializeConnection();
        });

        function initializeConnection() {
            const ip = document.getElementById('ipInput').value;
            const port = document.getElementById('portInput').value;
            
            if(ws) ws.close();
            
            try {
                ws = new WebSocket(`ws://${ip}:${port}/ws`);
                let connected = false;

                const connectionTimeout = setTimeout(() => {
                    if (!connected) {
                        showStatus("Connection timeout. Using test data.");
                        loadTestData();
                    }
                }, 3000);

                ws.onopen = function() {
                    clearTimeout(connectionTimeout);
                    connected = true;
                    statusMessage.style.display = 'none';
                    ws.send("config?");
                };

                ws.onmessage = function(event) {
                    const data = JSON.parse(event.data);
                    if (data.Configuration) {
                        lastConfig = data.Configuration;
                        displayConfiguration(lastConfig);
                    }
                    if (data.RBMS) {
                        updateRacks(data.RBMS);
                    }
                };

                ws.onerror = function(error) {
                    showStatus("Connection error. Using test data.");
                    loadTestData();
                };

            } catch (error) {
                showStatus("WebSocket initialization failed. Using test data.");
                loadTestData();
            }
        }

        function displayConfiguration(config) {
            titleContainer.innerHTML = '';
            const configContainer = document.createElement('div');
            renderNestedObject(config, configContainer);
            titleContainer.appendChild(configContainer);
            titleContainer.style.display = 'block';
        }

        function renderNestedObject(obj, container, depth = 0) {
            Object.entries(obj).forEach(([key, value]) => {
                const entryDiv = document.createElement('div');
                entryDiv.className = 'data-item';
                entryDiv.style.paddingLeft = `${depth * 15}px`;

                const titleDiv = document.createElement('div');
                titleDiv.className = 'data-title';
                titleDiv.textContent = key;

                const contentDiv = document.createElement('div');
                contentDiv.style.flexGrow = '1';

                if (typeof value === 'object' && value !== null) {
                    contentDiv.classList.add('nested-object');
                    renderNestedObject(value, contentDiv, depth + 1);
                } else {
                    contentDiv.textContent = Array.isArray(value) 
                        ? value.join(', ') 
                        : value;
                }

                entryDiv.appendChild(titleDiv);
                entryDiv.appendChild(contentDiv);
                container.appendChild(entryDiv);
            });
        }

        /* ... [Keep other functions same as previous version] ... */
    </script>
</body>
</html>
```

Key enhancements:
1. Added connection configuration form at the top
2. Improved handling of nested objects in configuration data
3. Added visual indentation for nested objects
4. Better array display (comma-separated values)
5. Proper WebSocket connection management
6. Reconnect capability with new IP/port

Features:
- Input fields for IP address and port
- Connect/reconnect button
- Recursive rendering of nested configuration objects
- Visual indentation for nested data
- Array formatting
- Connection status tracking
- Proper WebSocket cleanup on reconnect

To use:
1. Enter desired IP/port in the input fields
2. Click Connect
3. If connection fails, test data will be shown automatically
4. Complex configuration objects will display with nested indentation
5. Arrays will display as comma-separated lists

The configuration display now handles:
- Multi-level nested objects
- Arrays
- Complex data structures
- Recursive rendering with visual indentation
- Different formatting for different data types