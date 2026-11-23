"""
M5Dial PC Control Center - Windows Companion Server
Listens for Wi-Fi commands from the M5Dial device and executes system controls.
"""

import sys
import os
import json
import socket
import secrets
import argparse
from http.server import HTTPServer, ThreadingHTTPServer, BaseHTTPRequestHandler
from typing import Optional

current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

from controller import WindowsController

# Manually load .env if present
def load_env_file(filepath: str):
    if not os.path.exists(filepath):
        return
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    k, v = line.split("=", 1)
                    k = k.strip()
                    v = v.strip().strip('"').strip("'")
                    if k not in os.environ:
                        os.environ[k] = v
    except Exception:
        pass

load_env_file(os.path.join(current_dir, ".env"))
load_env_file(os.path.join(os.path.dirname(current_dir), ".env"))

# Fallback to python-dotenv if installed
try:
    from dotenv import load_dotenv
    load_dotenv(os.path.join(current_dir, ".env"))
    load_dotenv(os.path.join(os.path.dirname(current_dir), ".env"))
except ImportError:
    pass

DEFAULT_HOST = os.environ.get("PC_CONTROL_HOST", "0.0.0.0")
DEFAULT_PORT = int(os.environ.get("PC_CONTROL_PORT", "8765"))
DEFAULT_TOKEN = "M5Dial-PC-7f9C2mQ8vL4xN6pR5tY3kW1zH9aB"
AUTH_TOKEN = os.environ.get("PC_CONTROLLER_TOKEN", DEFAULT_TOKEN).strip()

controller = WindowsController()


def get_local_ipv4() -> str:
    """Determine the active local IPv4 address."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


class PCControlRequestHandler(BaseHTTPRequestHandler):
    """Handles HTTP requests from M5Dial and browser dashboard."""

    server_version = "M5DialPCControl/1.0"

    def _send_json(self, status_code: int, data: dict):
        """Send JSON response with CORS headers."""
        payload = json.dumps(data).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-Controller-Token, Authorization")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()
        self.wfile.write(payload)

    def _is_authenticated(self) -> bool:
        """Validate the security token against request headers."""
        global AUTH_TOKEN
        if not AUTH_TOKEN:
            return True

        header_token = self.headers.get("X-Controller-Token", "")
        clean_hdr = header_token.strip().strip('"').strip("'")
        clean_auth = AUTH_TOKEN.strip().strip('"').strip("'")

        if clean_hdr and secrets.compare_digest(clean_hdr, clean_auth):
            return True

        auth_header = self.headers.get("Authorization", "")
        if auth_header.startswith("Bearer "):
            bearer = auth_header[7:].strip().strip('"').strip("'")
            if secrets.compare_digest(bearer, clean_auth):
                return True

        return False

    def do_OPTIONS(self):
        """Handle CORS pre-flight requests."""
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-Controller-Token, Authorization")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.end_headers()

    def do_GET(self):
        """Handle health check, status, and dashboard."""
        path = self.path.split("?")[0].rstrip("/")

        if path in ("/api/status", "/api/health", "/api/state"):
            st = controller.get_status()
            self._send_json(200, {
                "status": "ready",
                "service": "M5Dial PC Control Center",
                "server_ip": get_local_ipv4(),
                "auth_required": bool(AUTH_TOKEN),
                "volume": st.get("volume", 50),
                "brightness": st.get("brightness", 50),
                "muted": st.get("muted", False)
            })
            return

        if path == "" or path == "/index.html":
            html_body = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>M5Dial PC Control Center</title>
    <style>
        body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #121212; color: #f0f0f0; margin: 40px; }}
        .card {{ background: #1e1e1e; border: 1px solid #333; border-radius: 8px; padding: 24px; max-width: 600px; margin-bottom: 24px; }}
        h1 {{ margin-top: 0; color: #4da6ff; }}
        .badge {{ background: #28a745; color: white; padding: 4px 8px; border-radius: 4px; font-size: 13px; font-weight: bold; }}
        .btn-grid {{ display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; margin-top: 16px; }}
        button {{ background: #2a2a2a; color: #fff; border: 1px solid #444; padding: 12px; border-radius: 6px; cursor: pointer; font-size: 14px; font-weight: 500; }}
        button:hover {{ background: #007acc; border-color: #007acc; }}
        #log {{ background: #0a0a0a; border: 1px solid #222; border-radius: 6px; padding: 12px; height: 160px; overflow-y: auto; font-family: monospace; font-size: 13px; margin-top: 16px; color: #a0ffa0; }}
        code {{ background: #2d2d2d; padding: 2px 6px; border-radius: 4px; }}
    </style>
</head>
<body>
    <div class="card">
        <h1>M5Dial PC Control Center <span class="badge">ONLINE</span></h1>
        <p><strong>Server IP:</strong> <code>{get_local_ipv4()}</code> | <strong>Port:</strong> <code>{self.server.server_port}</code></p>
        <p><strong>M5Dial Configuration:</strong><br>Ensure <code>main/apps/app_wifi_scan/pc_control_config.h</code> has:<br>
        <code>#define PC_CONTROL_BASE_URL "http://{get_local_ipv4()}:{self.server.server_port}"</code></p>
        
        <h3>Quick Test Buttons:</h3>
        <div class="btn-grid">
            <button onclick="sendCmd('volume_delta', 5)">Volume +5</button>
            <button onclick="sendCmd('volume_delta', -5)">Volume -5</button>
            <button onclick="sendCmd('mute', 0)">Toggle Mute</button>
            <button onclick="sendCmd('brightness_delta', 5)">Brightness +5</button>
            <button onclick="sendCmd('brightness_delta', -5)">Brightness -5</button>
            <button onclick="sendCmd('media_play_pause', 0)">Media Play/Pause</button>
            <button onclick="sendCmd('zoom_in', 0)">Browser Zoom In</button>
            <button onclick="sendCmd('zoom_out', 0)">Browser Zoom Out</button>
        </div>
        <div id="log">Ready for commands...</div>
    </div>
    <script>
        const token = "{AUTH_TOKEN}";
        function sendCmd(command, value) {{
            const log = document.getElementById("log");
            log.innerHTML += `\\nSending: ${{command}} (${{value}})...`;
            fetch("/api/command", {{
                method: "POST",
                headers: {{
                    "Content-Type": "application/json",
                    "X-Controller-Token": token
                }},
                body: JSON.stringify({{ command: command, value: value }})
            }})
            .then(res => res.json())
            .then(data => {{
                log.innerHTML += ` -> [${{data.status}}] ${{data.message || data.error}}`;
                log.scrollTop = log.scrollHeight;
            }})
            .catch(err => {{
                log.innerHTML += ` -> [FAIL] ${{err.message}}`;
                log.scrollTop = log.scrollHeight;
            }});
        }}
    </script>
</body>
</html>
"""
            payload = html_body.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return

        self._send_json(404, {"status": "error", "message": "Not Found"})

    def do_POST(self):
        """Handle incoming control commands."""
        path = self.path.split("?")[0].rstrip("/")

        if path != "/api/command":
            self._send_json(404, {"status": "error", "message": "Endpoint not found"})
            return

        # 1. Authenticate
        header_token = self.headers.get("X-Controller-Token", "")
        auth_header = self.headers.get("Authorization", "")
        client_ip = self.client_address[0]

        print(f"[INCOMING REQUEST] From {client_ip} | Method: POST /api/command")
        print(f"  -> Received Token: '{header_token}'")
        print(f"  -> Expected Token: '{AUTH_TOKEN}'")

        if not self._is_authenticated():
            print(f"[AUTH REJECTED] Tokens did not match!")
            self._send_json(401, {
                "status": "error",
                "message": "Unauthorized: invalid or missing X-Controller-Token"
            })
            return

        # 2. Parse request body
        try:
            content_length = int(self.headers.get("Content-Length", 0))
            if content_length <= 0 or content_length > 4096:
                self._send_json(400, {"status": "error", "message": "Invalid body length"})
                return

            raw_body = self.rfile.read(content_length).decode("utf-8")
            payload = json.loads(raw_body)
        except Exception as e:
            self._send_json(400, {"status": "error", "message": f"Malformed JSON: {str(e)}"})
            return

        command = payload.get("command")
        value = payload.get("value", 0)

        if not command or not isinstance(command, str):
            self._send_json(400, {"status": "error", "message": "Missing 'command' field"})
            return

        if not isinstance(value, int):
            try:
                value = int(value)
            except (ValueError, TypeError):
                value = 0

        # 3. Execute command
        success, message = controller.execute(command, value)
        st = controller.get_status()
        client_ip = self.client_address[0]
        status_str = "OK" if success else "FAILED"
        print(f"[{status_str}] From {client_ip}: '{command}' (val={value}) -> {message} | Vol: {st.get('volume')}% Br: {st.get('brightness')}%")

        if success:
            self._send_json(200, {
                "status": "ok",
                "command": command,
                "message": message,
                "volume": st.get("volume", 50),
                "brightness": st.get("brightness", 50),
                "muted": st.get("muted", False)
            })
        else:
            self._send_json(400 if "Unknown" in message else 500, {
                "status": "error",
                "command": command,
                "message": message,
                "volume": st.get("volume", 50),
                "brightness": st.get("brightness", 50),
                "muted": st.get("muted", False)
            })

    def log_message(self, format, *args):
        """Suppress default HTTP request spam to keep terminal clean."""
        pass


def run_server(host: str = DEFAULT_HOST, port: int = DEFAULT_PORT):
    """Start the PC companion HTTP server."""
    global AUTH_TOKEN

    local_ip = get_local_ipv4()

    print("=" * 60, flush=True)
    print("   M5Dial PC Control Center - Companion Server", flush=True)
    print("=" * 60, flush=True)
    print(f"Listening on : http://{host}:{port}", flush=True)
    print(f"Local IPv4   : http://{local_ip}:{port}", flush=True)
    print(f"Web Dashboard: http://localhost:{port}", flush=True)
    print(f"Auth Token   : {AUTH_TOKEN[:6]}...{AUTH_TOKEN[-4:]}", flush=True)

    print("\nM5Dial Configuration:", flush=True)
    print(f"  URL:   http://{local_ip}:{port}", flush=True)
    print(f"  Token: {AUTH_TOKEN}", flush=True)
    print("-" * 60, flush=True)
    print("STATUS: SERVER ONLINE & WAITING FOR COMMANDS", flush=True)
    print("KEEP THIS WINDOW OPEN while using PC Control on M5Dial!", flush=True)
    print("-" * 60, flush=True)
    print("Press Ctrl+C to stop the server.\n", flush=True)

    httpd = ThreadingHTTPServer((host, port), PCControlRequestHandler)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server gracefully...", flush=True)
        httpd.server_close()
        print("Server stopped.", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="M5Dial Windows Companion Server")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Host/IP to bind (default 0.0.0.0)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Port to listen on (default 8765)")
    args = parser.parse_args()

    run_server(args.host, args.port)
