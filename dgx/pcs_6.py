import asyncio

# Pymodbus 3.12.0 Specific Imports
from pymodbus.server import StartAsyncTcpServer
from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

async def run_pcs_sim(name, port, map_start, map_size):
    # In 3.12.0, SequentialDataBlock takes (address, values)
    # We initialize with a list of zeros for the requested size
    initial_values = [0] * map_size
    block = ModbusSequentialDataBlock(map_start, initial_values)
    
    # hr = Holding Registers (40001+)
    store = ModbusSlaveContext(hr=block)
    context = ModbusServerContext(slaves=store, single=True)
    
    # Identify the unit
    identity = ModbusDeviceIdentification()
    identity.VendorName = 'AI-BMS-Sim'
    identity.ProductCode = name
    identity.ModelName = f"{name}_250kW_Inverter"
    
    print(f"🚀 {name} Simulator ready on port {port}")
    
    # Signature for 3.12.0 Async Server
    return await StartAsyncTcpServer(
        context=context, 
        address=("0.0.0.0", port), 
        identity=identity
    )

async def main():
    # Use gather to run all three on the same event loop
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
