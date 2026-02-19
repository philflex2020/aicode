import asyncio
import logging

# New locations for 3.x/4.x
from pymodbus.server import StartAsyncTcpServer
from pymodbus.server.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

# Optional: Enable logging to see the AI load bursts in the terminal
# logging.basicConfig(level=logging.INFO)

async def run_pcs_sim(name, port, map_start, map_size):
    # Initialize a data block for Holding Registers (hr)
    # Using a list of zeros for the specified size
    block = ModbusSequentialDataBlock(map_start, [0] * map_size)
    store = ModbusSlaveContext(hr=block)
    context = ModbusServerContext(slaves=store, single=True)
    
    identity = ModbusDeviceIdentification()
    identity.VendorName = 'AI-BMS-Sim'
    identity.ProductCode = name
    identity.ModelName = f"{name}_Inverter_250kW"
    
    print(f"🚀 {name} Simulator live on port {port} (Registers {map_start}-{map_start+map_size})")
    
    # In 3.x, the address is passed as a string/int tuple
    return await StartAsyncTcpServer(
        context=context, 
        address=("0.0.0.0", port), 
        identity=identity
    )

async def main():
    # Gather the coroutines
    await asyncio.gather(
        run_pcs_sim("Dynapower", 5021, 0, 30000),
        run_pcs_sim("EPC_Power", 5022, 0, 1000),
        run_pcs_sim("Sungrow", 5023, 4000, 2000)
    )

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutting down AI-PCS Simulators...")
