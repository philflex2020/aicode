import asyncio
import RPi.GPIO as GPIO
import websockets

# GPIO setup
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

# Define GPIO pins to control
PINS = [17, 18, 27, 22]

for pin in PINS:
    GPIO.setup(pin, GPIO.OUT)
    GPIO.output(pin, GPIO.LOW)

async def gpio_handler(websocket, path):
    async for message in websocket:
        print(f"Received message: {message}")
        command, pin_str = message.split()
        pin = int(pin_str)
        
        if command == "ON" and pin in PINS:
            GPIO.output(pin, GPIO.HIGH)
            await websocket.send(f"Pin {pin} turned ON")
        elif command == "OFF" and pin in PINS:
            GPIO.output(pin, GPIO.LOW)
            await websocket.send(f"Pin {pin} turned OFF")
        else:
            await websocket.send(f"Invalid command or pin")

start_server = websockets.serve(gpio_handler, "0.0.0.0", 8765)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()
