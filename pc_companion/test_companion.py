"""
Automated Verification Suite for M5Dial PC Control Center
Tests authentication, validation, health check, and system commands.
"""

import sys
import os
import json
import time
import urllib.request
import urllib.error

SERVER_URL = os.environ.get("PC_CONTROL_URL", "http://127.0.0.1:8765")
TOKEN = os.environ.get("PC_CONTROLLER_TOKEN", "M5Dial-PC-7f9C2mQ8vL4xN6pR5tY3kW1zH9aB")

def send_request(endpoint: str, data: dict = None, token: str = None) -> tuple:
    """Helper to send HTTP request and return (status_code, response_json)."""
    url = f"{SERVER_URL}{endpoint}"
    headers = {"Content-Type": "application/json"}
    if token:
        headers["X-Controller-Token"] = token

    payload = json.dumps(data).encode("utf-8") if data is not None else None
    req = urllib.request.Request(url, data=payload, headers=headers, method="POST" if data else "GET")

    try:
        with urllib.request.urlopen(req, timeout=3) as resp:
            code = resp.getcode()
            body = resp.read().decode("utf-8")
            return code, json.loads(body)
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8")
        try:
            return e.code, json.loads(body)
        except Exception:
            return e.code, {"error": body}
    except Exception as e:
        return 0, {"error": str(e)}


def run_tests():
    print("=" * 60)
    print("   M5Dial Companion Server Verification Suite")
    print("=" * 60)
    print(f"Target: {SERVER_URL}")
    print(f"Token : {TOKEN[:6]}...{TOKEN[-4:]}")
    print("-" * 60)

    # 1. Test Health / Status
    print("[TEST 1/5] Checking GET /api/status...")
    code, res = send_request("/api/status")
    if code == 200 and res.get("status") == "ready":
        print(f"  PASS: Server ready. IP: {res.get('server_ip')}")
    else:
        print(f"  FAIL: Expected 200, got {code}: {res}")
        return False

    # 2. Test Missing / Invalid Token (Auth rejection)
    print("\n[TEST 2/5] Testing authentication rejection (Invalid Token)...")
    code, res = send_request("/api/command", {"command": "mute", "value": 0}, token="WRONG_TOKEN")
    if code == 401:
        print(f"  PASS: Server rejected invalid token with HTTP 401: {res.get('message')}")
    else:
        print(f"  FAIL: Expected 401, got {code}: {res}")

    # 3. Test Invalid Command
    print("\n[TEST 3/5] Testing validation of invalid command name...")
    code, res = send_request("/api/command", {"command": "nonexistent_action", "value": 0}, token=TOKEN)
    if code in (400, 500):
        print(f"  PASS: Server rejected invalid command with code {code}: {res.get('message')}")
    else:
        print(f"  FAIL: Expected 400/500, got {code}: {res}")

    # 4. Test Core Control Commands
    print("\n[TEST 4/5] Testing core control commands with valid token...")
    test_commands = [
        ("volume_delta", 5, "Volume +5"),
        ("volume_delta", -5, "Volume -5"),
        ("mute", 0, "Mute toggle"),
        ("media_play_pause", 0, "Media Play/Pause"),
        ("zoom_in", 0, "Browser Zoom In"),
        ("zoom_out", 0, "Browser Zoom Out"),
        ("brightness_delta", 5, "Brightness +5 (or supported report)")
    ]

    all_passed = True
    for cmd, val, desc in test_commands:
        code, res = send_request("/api/command", {"command": cmd, "value": val}, token=TOKEN)
        if code == 200:
            print(f"  PASS [{desc}]: {res.get('message')}")
        else:
            # Note: brightness might report not supported on desktop monitors
            if cmd == "brightness_delta" and code == 500 and "not supported" in str(res.get("message")):
                print(f"  PASS (Hardware Note) [{desc}]: {res.get('message')}")
            else:
                print(f"  FAIL [{desc}]: code={code}, res={res}")
                all_passed = False

    # 5. Summary
    print("\n" + "=" * 60)
    if all_passed:
        print("   ALL TESTS COMPLETED SUCCESSFULLY!")
    else:
        print("   SOME TESTS FAILED (Review output above)")
    print("=" * 60)
    return all_passed


if __name__ == "__main__":
    run_tests()
