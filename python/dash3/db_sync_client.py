# db_sync_client.py
# Sync client that watches for changes and replicates them
import asyncio
import websockets
import requests
import json
import argparse
from datetime import datetime
from typing import Optional

class SyncClient:
    """Client that syncs variables from a primary server to a secondary server"""
    
    def __init__(self, primary_url: str, secondary_url: str, name: str = "sync-client"):
        self.primary_url = primary_url.rstrip('/')
        self.secondary_url = secondary_url.rstrip('/')
        self.name = name
        self.ws_url = primary_url.replace('http://', 'ws://').replace('https://', 'wss://').rstrip('/') + '/ws/sync'
        self.running = False
        self.stats = {
            "created": 0,
            "updated": 0,
            "deleted": 0,
            "errors": 0
        }
    
    def log(self, message: str, level: str = "INFO"):
        """Log with timestamp"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        colors = {
            "INFO": "\033[94m",
            "SUCCESS": "\033[92m",
            "WARNING": "\033[93m",
            "ERROR": "\033[91m"
        }
        reset = "\033[0m"
        color = colors.get(level, "")
        print(f"{color}[{timestamp}] [{self.name}] {level}: {message}{reset}")
    
    async def initial_sync(self):
        """Perform initial sync of all variables"""
        self.log("Starting initial sync...")
        
        try:
            # Get all variables from primary
            response = requests.get(f"{self.primary_url}/api/variables")
            response.raise_for_status()
            primary_vars = response.json()
            
            # Get all variables from secondary
            response = requests.get(f"{self.secondary_url}/api/variables")
            response.raise_for_status()
            secondary_vars = response.json()
            
            # Build lookup
            secondary_lookup = {v['name']: v for v in secondary_vars}
            
            synced = 0
            for var in primary_vars:
                if var['name'] not in secondary_lookup:
                    # Create on secondary
                    await self.create_variable(var)
                    synced += 1
                else:
                    # Check if update needed
                    sec_var = secondary_lookup[var['name']]
                    if sec_var['variable_version'] < var['variable_version']:
                        await self.update_variable(var)
                        synced += 1




            # 4) FINAL STEP: Force secondary's global current_version to match primary's
            try:
                prim_ver_resp = requests.get(f"{self.primary_url}/api/system/version")
                prim_ver_resp.raise_for_status()
                primary_sys_ver = prim_ver_resp.json().get("current_version")
                
                if primary_sys_ver is not None:
                    # Call a small internal sync endpoint on secondary to set system version
                    resp2 = requests.post(
                        f"{self.secondary_url}/api/system/version/sync",
                        json={"current_version": primary_sys_ver},
                    )
                    if resp2.status_code == 200:
                        self.log(
                            f"Initial sync: forced secondary current_version -> {primary_sys_ver}",
                            "SUCCESS",
                        )
                    else:
                        self.log(
                            f"Initial sync: failed to set secondary current_version "
                            f"({resp2.status_code}): {resp2.text}",
                            "ERROR",
                        )
                else:
                    self.log("Initial sync: primary /api/system/version returned no version", "WARNING")
            except Exception as e:
                self.log(f"Initial sync: error syncing system version: {e}", "ERROR")

            
            self.log(f"Initial sync complete: {synced} variables synced", "SUCCESS")
        
        except Exception as e:
            self.log(f"Initial sync failed: {e}", "ERROR")
    
    async def create_variable(self, var_data: dict):
        """Create variable on secondary server with exact version"""
        try:
            # Use the sync endpoint to preserve versions
            response = requests.post(
                f"{self.secondary_url}/api/sync/variable",
                json=var_data
            )
            
            if response.status_code == 200:
                result = response.json()
                action = result.get('action')
                
                if action == 'created':
                    self.stats['created'] += 1
                    self.log(
                        f"✓ Created: {var_data['name']} "
                        f"(cat={var_data['category']}, loc={var_data.get('locator', 'none')}, "
                        f"v{var_data['variable_version']}/sys{var_data['system_version']})",
                        "SUCCESS"
                    )
                elif action == 'updated':
                    self.stats['updated'] += 1
                    self.log(f"✓ Updated: {var_data['name']}", "SUCCESS")
                elif action == 'skipped':
                    self.log(f"⚠ Skipped: {var_data['name']} - {result.get('reason')}", "WARNING")
            else:
                self.stats['errors'] += 1
                self.log(f"✗ Create failed: {var_data['name']} - {response.text}", "ERROR")
        
        except Exception as e:
            self.stats['errors'] += 1
            self.log(f"✗ Create error: {var_data['name']} - {e}", "ERROR")
    
    async def update_variable(self, var_data: dict):
        """Update variable on secondary server with exact version"""
        try:
            # Use the sync endpoint to preserve versions
            response = requests.post(
                f"{self.secondary_url}/api/sync/variable",
                json=var_data
            )
            
            if response.status_code == 200:
                result = response.json()
                action = result.get('action')
                
                if action == 'updated':
                    self.stats['updated'] += 1
                    self.log(
                        f"✓ Updated: {var_data['name']} "
                        f"(v{var_data['variable_version']}/sys{var_data['system_version']})",
                        "SUCCESS"
                    )
                elif action == 'created':
                    self.stats['created'] += 1
                    self.log(f"✓ Created (was missing): {var_data['name']}", "SUCCESS")
                elif action == 'skipped':
                    self.log(f"⚠ Skipped: {var_data['name']} - {result.get('reason')}", "WARNING")
            else:
                self.stats['errors'] += 1
                self.log(f"✗ Update failed: {var_data['name']} - {response.text}", "ERROR")
        
        except Exception as e:
            self.stats['errors'] += 1
            self.log(f"✗ Update error: {var_data['name']} - {e}", "ERROR")
    
    async def delete_variable(self, var_name: str, system_version: int):
        """Delete variable on secondary server and sync version"""
        try:
            response = requests.delete(
                f"{self.secondary_url}/api/sync/variable/{var_name}",
                params={"system_version": system_version}
            )
            
            if response.status_code == 200:
                result = response.json()
                if result.get('action') == 'deleted':
                    self.stats['deleted'] += 1
                    self.log(f"✓ Deleted: {var_name} (sys{system_version})", "SUCCESS")
                else:
                    self.log(f"⚠ Delete skipped: {var_name}", "WARNING")
            else:
                self.stats['errors'] += 1
                self.log(f"✗ Delete failed: {var_name} - {response.text}", "ERROR")
        
        except Exception as e:
            self.stats['errors'] += 1
            self.log(f"✗ Delete error: {var_name} - {e}", "ERROR")
    
    async def handle_change(self, message: dict):
        """Handle a change notification from primary server"""
        event_type = message.get('type')
        var_data = message.get('variable')
        system_version = message.get('system_version')
        
        if not var_data:
            return
        
        if event_type == 'created':
            # Fetch full variable data
            try:
                response = requests.get(f"{self.primary_url}/api/variables/{var_data['name']}")
                if response.status_code == 200:
                    full_var = response.json()
                    await self.create_variable(full_var)
            except Exception as e:
                self.log(f"✗ Failed to fetch variable {var_data['name']}: {e}", "ERROR")
        
        elif event_type == 'updated':
            # Fetch full variable data
            try:
                response = requests.get(f"{self.primary_url}/api/variables/{var_data['name']}")
                if response.status_code == 200:
                    full_var = response.json()
                    await self.update_variable(full_var)
            except Exception as e:
                self.log(f"✗ Failed to fetch variable {var_data['name']}: {e}", "ERROR")
        
        elif event_type == 'deleted':
            await self.delete_variable(var_data['name'], system_version)
    
    async def run(self):
        """Run the sync client"""
        self.running = True
        self.log(f"Starting sync client...")
        self.log(f"Primary:   {self.primary_url}")
        self.log(f"Secondary: {self.secondary_url}")
        
        # Initial sync
        await self.initial_sync()
        
        # Connect to WebSocket for real-time updates
        self.log("Connecting to WebSocket for real-time sync...")
        
        retry_delay = 1
        max_retry_delay = 60
        
        while self.running:
            try:
                async with websockets.connect(self.ws_url) as websocket:
                    self.log("✓ WebSocket connected", "SUCCESS")
                    retry_delay = 1  # Reset retry delay on successful connection
                    
                    # Send ping periodically
                    async def send_ping():
                        while self.running:
                            try:
                                await websocket.send(json.dumps({"type": "ping"}))
                                await asyncio.sleep(30)
                            except:
                                break
                    
                    ping_task = asyncio.create_task(send_ping())
                    
                    # Listen for messages
                    while self.running:
                        try:
                            message = await websocket.recv()
                            data = json.loads(message)
                            
                            if data.get('type') in ['created', 'updated', 'deleted']:
                                await self.handle_change(data)
                        
                        except websockets.exceptions.ConnectionClosed:
                            self.log("WebSocket connection closed", "WARNING")
                            break
                    
                    ping_task.cancel()
            
            except Exception as e:
                self.log(f"WebSocket error: {e}", "ERROR")
                self.log(f"Retrying in {retry_delay}s...", "WARNING")
                await asyncio.sleep(retry_delay)
                retry_delay = min(retry_delay * 2, max_retry_delay)
    
    def print_stats(self):
        """Print sync statistics"""
        self.log("=" * 60)
        self.log("Sync Statistics:")
        self.log(f"  Created: {self.stats['created']}")
        self.log(f"  Updated: {self.stats['updated']}")
        self.log(f"  Deleted: {self.stats['deleted']}")
        self.log(f"  Errors:  {self.stats['errors']}")
        self.log("=" * 60)


async def main():
    parser = argparse.ArgumentParser(description='Variable Registry Sync Client')
    parser.add_argument('--primary', type=str, required=True,
                        help='Primary server URL (e.g., http://localhost:8902)')
    parser.add_argument('--secondary', type=str, required=True,
                        help='Secondary server URL (e.g., http://localhost:8903)')
    parser.add_argument('--name', type=str, default='sync-client',
                        help='Client name for identification')
    
    args = parser.parse_args()
    
    client = SyncClient(args.primary, args.secondary, args.name)
    
    try:
        await client.run()
    except KeyboardInterrupt:
        print("\n")
        client.log("Shutting down...", "WARNING")
        client.running = False
        client.print_stats()


if __name__ == "__main__":
    asyncio.run(main())


