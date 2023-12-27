import socket
import json

def start_broadcast_listener(listen_port=5001):
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Enable reuse address/port
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    # Bind to the broadcast port
    sock.bind(('', listen_port))

    print(f"Listening for broadcasts on port {listen_port}")

    try:
        while True:
            # Receive broadcast message
            message, address = sock.recvfrom(1024)  # Buffer size is 1024 bytes
            # Decode the message
            decoded_message = message.decode()
            
            try:
                # Attempt to parse the message as JSON
                status = json.loads(decoded_message)
                # Process the received status message
                print(f"Received status: {status} from {address}")
            except json.JSONDecodeError:
                # Handle case where message is not JSON or is malformed
                print(f"Received non-JSON message: {decoded_message} from {address}")

    except KeyboardInterrupt:
        # Handle graceful shutdown if Ctrl+C is pressed
        print("Broadcast listener stopped")
    finally:
        sock.close()

if __name__ == "__main__":
    start_broadcast_listener()