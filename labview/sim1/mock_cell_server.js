
const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 8083 });

let user_current = 0;
const interval_ms = 500;
const cell_capacity_Ah = 2.5;  // 2500 mAh
const dt_hr = interval_ms / 3600000;

const cell = {
    soc: 100,
    voltage: 4.0,
    temp: 25.0,
    current: 0.0,
    capacity: 0.0,
};

function updateCell() {
    cell.current = user_current;

    const delta_soc = (cell.current * dt_hr * 100) / cell_capacity_Ah;

    // Charging or discharging
    cell.soc = Math.min(100, Math.max(0, cell.soc - delta_soc));

    // Voltage follows SoC linearly: 3.0V at 0%, 4.2V at 100%
    cell.voltage = 3.0 + (1.2 * (cell.soc / 100));

    // Capacity is cumulative discharge only (for tracking energy used)
    if (cell.current < 0) {
        cell.capacity += Math.abs(cell.current * dt_hr);  // accumulate discharge
    }

    // Simulate temperature changes (charge and discharge both heat)
    cell.temp += 0.02 * Math.abs(cell.current);

    // Slow cool when idle
    if (Math.abs(cell.current) < 0.01) {
        cell.temp -= 0.01;
    }

    // Clamp temp
    cell.temp = Math.max(20, Math.min(80, cell.temp));
}

function broadcast(ws) {
    updateCell();

    const packet = {
        time: Date.now() / 1000,
        cells: [{
            soc: cell.soc,
            temp: cell.temp,
            voltage: cell.voltage,
            current: cell.current,
            capacity: cell.capacity
        }]
    };

    ws.send(JSON.stringify(packet));
}

wss.on('connection', function connection(ws) {
    console.log("Client connected");
    ws.on('message', function incoming(message) {
        let msg = {};
        try {
            msg = JSON.parse(message);
        } catch (e) {
            console.error("Invalid JSON");
        }

        if (msg.cmd === "set_current") {
            user_current = msg.value;
        } else if (msg.cmd === "get_scales") {
            ws.send(JSON.stringify({
                cmd: "scales",
                scales: {
                    voltage: { min: 3.0, max: 4.5 },
                    soc: { min: 0, max: 100 },
                    current: { min: -40, max: 40 },
                    temp: { min: 0, max: 100 },
                    capacity: { min: 0, max: 5 }
                }
            }));
        }
    });

    const timer = setInterval(() => broadcast(ws), interval_ms);

    ws.on('close', () => {
        clearInterval(timer);
    });
});
