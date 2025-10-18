import concurrent.futures
from http.server import BaseHTTPRequestHandler, HTTPServer
import os
from ping3 import ping
import sqlite3
import threading
import time
from urllib.parse import urlparse, parse_qs

# --- Database setup ---
DB_NAME = 'ping_data.db'

def setup_database():
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS ping_results (
            id INTEGER PRIMARY KEY,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            ip_address TEXT NOT NULL,
            response_time REAL
        )
    ''')
    conn.commit()
    conn.close()

# --- Network scan logic ---
def check_host(ip_address):
    """Pings a single IP address and returns its status, or None if down."""
    response_time = ping(ip_address, timeout=1)
    if response_time is not None:
        return ip_address, response_time
    return None, None

def run_ping_sweep():
    """Runs a ping sweep, stores results in the database, and waits."""
    while True:
        subnet = "192.168.86."
        ip_addresses = [f"{subnet}{i}" for i in range(1, 255)]
        
        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        
        with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
            future_to_ip = {executor.submit(check_host, ip): ip for ip in ip_addresses}
            for future in concurrent.futures.as_completed(future_to_ip):
                ip, response_time = future.result()
                if ip:
                    cursor.execute(
                        "INSERT INTO ping_results (ip_address, response_time) VALUES (?, ?)",
                        (ip, response_time)
                    )
        
        conn.commit()
        conn.close()
        print(f"Ping sweep completed and results stored. Sleeping for 5 seconds...")
        time.sleep(5)

# --- Web server logic ---
class MyHTTPRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed_path = urlparse(self.path)
        path = parsed_path.path
        query = parse_qs(parsed_path.query)
        
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        
        if path == '/':
            self.serve_main_page()
        elif path == '/chart':
            if 'ip' in query and query['ip']:
                self.serve_chart_page(query['ip'][0])
            else:
                self.send_error(400, "Bad Request: Missing 'ip' parameter")
        else:
            self.send_error(404, "Page Not Found")

    def serve_main_page(self):
        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        cursor.execute('''
            SELECT ip_address, MIN(response_time), MAX(response_time), AVG(response_time)
            FROM ping_results
            GROUP BY ip_address
            ORDER BY ip_address
        ''')
        aggregated_data = cursor.fetchall()

        html = ' <head> <meta http-equiv="refresh" content="10"></head>'
        html += "<h1>Ping Dashboard</h1>"
        html += "<h2>Aggregated Ping Statistics</h2>"
        html += "<table border='1'><tr><th>IP Address</th><th>Min Ping (s)</th><th>Max Ping (s)</th><th>Avg Ping (s)</th><th>Details</th></tr>"
        for ip, min_ping, max_ping, avg_ping in aggregated_data:
            html += f"""
            <tr>
                <td>{ip}</td>
                <td>{min_ping:.3f}</td>
                <td>{max_ping:.3f}</td>
                <td>{avg_ping:.3f}</td>
                <td><a href="/chart?ip={ip}">View Chart</a></td>
            </tr>
            """
        html += "</table>"
        
        self.wfile.write(html.encode('utf-8'))
        conn.close()

    def serve_chart_page(self, ip_address):
        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        cursor.execute(
            "SELECT timestamp, response_time FROM ping_results WHERE ip_address = ? ORDER BY timestamp DESC LIMIT 50",
            (ip_address,)
        )
        data = cursor.fetchall()
        conn.close()

        # Extracting data for JavaScript, reversing order for chart
        timestamps = [row[0] for row in reversed(data)]
        ping_times = [row[1] for row in reversed(data)]
        
        html = f"<h1>Ping History for {ip_address}</h1>"
        html += '<a href="/">Back to Dashboard</a>'
        html += '<canvas id="myChart" width="800" height="400"></canvas>'
        
        # JavaScript for drawing the chart using HTML Canvas (no libraries)
        js = f"""
        <script>
            const timestamps = {timestamps};
            const ping_times = {ping_times};
            
            const canvas = document.getElementById('myChart');
            const ctx = canvas.getContext('2d');
            
            const margin = 50;
            const width = canvas.width - 2 * margin;
            const height = canvas.height - 2 * margin;
            
            // Function to draw text
            function drawText(text, x, y, align = 'left') {{
                ctx.fillStyle = 'black';
                ctx.font = '12px Arial';
                ctx.textAlign = align;
                ctx.fillText(text, x, y);
            }}

            // Find min/max values for scaling
            const maxPing = Math.max(...ping_times) * 1.1; // 10% padding
            const minPing = 0;
            
            // Draw x-axis and labels
            ctx.beginPath();
            ctx.moveTo(margin, height + margin);
            ctx.lineTo(width + margin, height + margin);
            ctx.stroke();
            const numLabels = 5;
            for (let i = 0; i < numLabels; i++) {{
                const index = Math.floor(i * (timestamps.length - 1) / (numLabels - 1));
                const timestamp = new Date(timestamps[index]).toLocaleTimeString();
                const x = margin + i * width / (numLabels - 1);
                drawText(timestamp, x, height + margin + 20, 'center');
            }}

            // Draw y-axis and labels
            ctx.beginPath();
            ctx.moveTo(margin, margin);
            ctx.lineTo(margin, height + margin);
            ctx.stroke();
            const numYLabels = 5;
            for (let i = 0; i <= numYLabels; i++) {{
                const y = margin + height * (numYLabels - i) / numYLabels;
                const value = (minPing + (maxPing - minPing) * i / numYLabels).toFixed(2);
                drawText(value + 's', margin - 10, y + 5, 'right');
            }}
            
            // Draw the line graph
            ctx.beginPath();
            ctx.strokeStyle = 'blue';
            for (let i = 0; i < ping_times.length; i++) {{
                const x = margin + (i / (ping_times.length - 1)) * width;
                const y = margin + height - (ping_times[i] - minPing) / (maxPing - minPing) * height;
                if (i === 0) {{
                    ctx.moveTo(x, y);
                }} else {{
                    ctx.lineTo(x, y);
                }}
            }}
            ctx.stroke();
        </script>
        """
        html += js
        self.wfile.write(html.encode('utf-8'))

def start_server():
    server_address = ('', 8000)
    httpd = HTTPServer(server_address, MyHTTPRequestHandler)
    print("Web server started at http://localhost:8000")
    httpd.serve_forever()

if __name__ == '__main__':
    setup_database()
    
    # Create threads for the scanner and the web server
    scanner_thread = threading.Thread(target=run_ping_sweep)
    server_thread = threading.Thread(target=start_server)
    
    # Start the threads
    scanner_thread.daemon = True
    server_thread.daemon = True
    scanner_thread.start()
    server_thread.start()
    
    # Keep the main thread alive so the daemon threads can run
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down.")


