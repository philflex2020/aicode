# target_client.py
import json
import websocket
import threading
import time

class TargetClient:
    """WebSocket client for communicating with embedded target"""
    
    def __init__(self, ip, port, secure=False):
        self.ip = ip
        self.port = port
        self.secure = secure
        self.ws = None
        self.seq_counter = 0
        self.pending_responses = {}  # seq -> response
        self.lock = threading.Lock()
        self.connected = False
        self.ws_thread = None

    def connect(self):
        """Connect to target WebSocket server"""
        protocol = "wss" if self.secure else "ws"
        url = f"{protocol}://{self.ip}:{self.port}"
        
        self.ws = websocket.WebSocketApp(
            url,
            on_message=self._on_message,
            on_error=self._on_error,
            on_close=self._on_close,
            on_open=self._on_open
        )
        
        # Run in background thread
        self.ws_thread = threading.Thread(target=self.ws.run_forever)
        self.ws_thread.daemon = True
        self.ws_thread.start()
        
        # Wait for connection (up to 5 seconds)
        for _ in range(50):
            if self.connected:
                print(f"[TargetClient] Connected to {url}")
                return True
            time.sleep(0.1)
        
        print(f"[TargetClient] Failed to connect to {url}")
        return False

    def _on_open(self, ws):
        self.connected = True

    def _on_message(self, ws, message):
        try:
            resp = json.loads(message)
            seq = resp.get("seq")
            if seq is not None:
                with self.lock:
                    self.pending_responses[seq] = resp
        except Exception as e:
            print(f"[TargetClient] Error parsing message: {e}")

    def _on_error(self, ws, error):
        print(f"[TargetClient] WebSocket error: {error}")

    def _on_close(self, ws, close_status_code, close_msg):
        self.connected = False
        print(f"[TargetClient] Connection closed: {close_status_code} {close_msg}")

    def send_command(self, action, sm_name, reg_type, offset, num=None, data=None, timeout=5):
        """Send a command and wait for response"""
        if not self.connected:
            raise Exception("Not connected to target")

        with self.lock:
            self.seq_counter += 1
            seq = self.seq_counter

        cmd = {
            "action": action,
            "seq": seq,
            "sm_name": sm_name,
            "reg_type": reg_type,
            "offset": str(offset)
        }
        if num is not None:
            cmd["num"] = num
        if data is not None:
            cmd["data"] = data

        self.ws.send(json.dumps(cmd))

        # Wait for response
        start = time.time()
        while time.time() - start < timeout:
            with self.lock:
                if seq in self.pending_responses:
                    resp = self.pending_responses.pop(seq)
                    return resp
            time.sleep(0.05)

        raise TimeoutError(f"No response for seq {seq} within {timeout}s")

    def get_data(self, sm_name, reg_type, offset, num):
        """Get data from target"""
        return self.send_command("get", sm_name, reg_type, offset, num=num)

    def set_data(self, sm_name, reg_type, offset, data):
        """Set data on target"""
        return self.send_command("set", sm_name, reg_type, offset, data=data)

    def close(self):
        """Close connection"""
        if self.ws:
            self.ws.close()
        self.connected = False