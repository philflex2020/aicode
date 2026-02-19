import asyncio
import logging

# Pymodbus 3.12.0 Absolute Import Paths
from pymodbus.server import StartAsyncTcpServer
from pymodbus.server.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

async def run_pcs_sim(name, port, map_start, map_size):
    # Initialize Holding Registers (hr) with zeros
    # 3.x expects (address, values_list)
    initial_data = [0] * map_size
    block = ModbusSequentialDataBlock(map_start, initial_data)
    store = ModbusSlaveContext(hr=block)
    context = ModbusServerContext(slaves=store, single=True)
    
    # Setup Device Identification
    identity = ModbusDeviceIdentification()
    identity.VendorName = 'AI-BMS-Sim'
    identity.ProductCode = name
    identity.ModelName = f"{name}_250kW_Inverter"
    
    print(f"🚀 {name} Simulator ready on port {port}")
    
    # Standard async server call for 3.12.0
    return await StartAsyncTcpServer(
        context=context, 
        address=("0.0.0.0", port), 
        identity=identity
    )

async def main():
    # Run all three simulators concurrently
    await asyncio.gather(
        run_pcs_sim("Dynapower", 5021, 0, 30000),
        run_pcs_sim("EPC_Power", 5022, 0, 1000),
        run_pcs_sim("Sungrow", 5023, 0, 6000)
    )

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutting down AI-PCS Simulators...")
