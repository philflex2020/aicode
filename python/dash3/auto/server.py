# server.py
import http.server
import socketserver
import os

PORT = 8911

# Serve files from current directory
Handler = http.server.SimpleHTTPRequestHandler

# Optional: Print the local address
print(f"Serving at http://0.0.0.0:{PORT}")

# Start server
with socketserver.TCPServer(("0.0.0.0", PORT), Handler) as httpd:
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        httpd.shutdown()