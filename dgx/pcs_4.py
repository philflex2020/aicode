import asyncio
# Pymodbus 3.12.0 Specific Imports
from pymodbus.server import StartAsyncTcpServer
from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

async def run_pcs_sim(name, port, map_start, map_size):
    # Initialize Holding Registers (hr)
    # 3.12.0 allows direct initialization with a list of values
    block = ModbusSequentialDataBlock(map_start, [0] * map_size)
    store = ModbusSlaveContext(hr=block)
    context = ModbusServerContext(slaves=store, single=True)
    
    # Setup Device Identification
    identity = ModbusDeviceIdentification()
    identity.VendorName = 'AI-BMS-Sim'
    identity.ProductCode = name
    identity.ModelName = f"{name}_250kW_Inverter"
    
    print(f"🚀 {name} Simulator ready on port {port}")
    
    # The server call signature for 3.12.0
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
