import asyncio
import logging

# Universal 3.x/4.x Server Imports
from pymodbus.server import StartAsyncTcpServer
from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

async def run_pcs_sim(name, port, map_start, map_size):
    # Initialize a block of registers (40001+ / Holding Registers)
    # Using a list of 0s for the specified size
    data_block = ModbusSequentialDataBlock(map_start, [0] * map_size)
    store = ModbusSlaveContext(hr=data_block)
    context = ModbusServerContext(slaves=store, single=True)
    
    identity = ModbusDeviceIdentification()
    identity.VendorName = 'AI-BMS-Sim'
    identity.ProductCode = name
    identity.ModelName = f"{name}_Inverter_250kW"
    
    print(f"🚀 {name} Simulator live on port {port}")
    
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
