import asyncio
from pymodbus.server import StartAsyncTcpServer
from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext

async def run_pcs_sim(name, port, map_start, map_size):
    # Initialize a data block for each specific map
    store = ModbusSlaveContext(hr=ModbusSequentialDataBlock(map_start, [0]*map_size))
    context = ModbusServerContext(slaves=store, single=True)
    
    identity = ModbusDeviceIdentification()
    identity.VendorName = 'AI-BMS-Sim'
    identity.ProductCode = name
    
    print(f"Starting {name} Simulator on port {port}...")
    await StartAsyncTcpServer(context, address=("0.0.0.0", port), identity=identity)

async def main():
    # Launch all three simulators concurrently
    await asyncio.gather(
        run_pcs_sim("Dynapower", 5021, 0, 30000),
        run_pcs_sim("EPC_Power", 5022, 0, 1000),
        run_pcs_sim("Sungrow", 5023, 4000, 2000)
    )

if __name__ == "__main__":
    asyncio.run(main())
