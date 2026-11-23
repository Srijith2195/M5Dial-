"""
M5Dial PC Control Center Companion Server - Root Launcher
Delegates directly to pc_companion/server.py
"""

import sys
import os

# Ensure unbuffered stdout so prints appear immediately in any console or redirect
try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(line_buffering=True)
except Exception:
    pass

if __name__ == "__main__":
    companion_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pc_companion")
    if companion_dir not in sys.path:
        sys.path.insert(0, companion_dir)

    from server import run_server
    run_server()

