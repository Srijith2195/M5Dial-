# M5Dial PC Control Center - Windows Companion Server

Lightweight Windows 11 companion service for M5Stack M5Dial. Receives control commands over the local Wi-Fi network and executes system actions including screen brightness, system audio volume, browser tab zoom, media playback, and window navigation.

## Architecture & API

- **Protocol**: HTTP/1.1 REST
- **Default Port**: `8765`
- **Default Binding**: `0.0.0.0` (accessible over local Wi-Fi subnet)
- **Authentication**: `X-Controller-Token` header (or `Authorization: Bearer <token>`)

### Endpoints

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/` | Web status and testing dashboard |
| `GET` | `/api/status` | Health check and IP discovery |
| `POST` | `/api/command` | Execute PC control command |

### Command Payloads

POST payload schema:
```json
{
  "command": "<command_name>",
  "value": 0
}
```

Supported Commands:
- `volume_delta`: Value `+5` or `-5` (Windows system master volume)
- `mute`: Value `0` (Toggle system audio mute)
- `brightness_delta`: Value `+5` or `-5` (Display brightness)
- `media_play_pause`: Value `0` (Toggle media playback)
- `media_next`: Value `0` (Next media track)
- `media_previous`: Value `0` (Previous media track)
- `zoom_in`: Value `0` (Active window zoom in `Ctrl + +`)
- `zoom_out`: Value `0` (Active window zoom out `Ctrl + -`)
- `zoom_reset`: Value `0` (Active window zoom reset `Ctrl + 0`)
- `browser_tab_next`: Value `0` (Next tab `Ctrl + Tab`)
- `browser_tab_previous`: Value `0` (Previous tab `Ctrl + Shift + Tab`)
- `app_switch`: Value `0` (Switch window `Alt + Tab`)
- `scroll_up`: Value `0` (Mouse wheel up)
- `scroll_down`: Value `0` (Mouse wheel down)

## Installation & Launch

1. Optional dependencies for high-precision volume and brightness:
   ```powershell
   pip install screen-brightness-control pycaw comtypes
   ```
   *(Note: The server includes built-in fallbacks via Windows `ctypes.windll.user32` standard library, so it functions even without optional packages!)*

2. Configure token:
   Copy `.env.example` to `.env` and set your `PC_CONTROLLER_TOKEN`.

3. Launch server:
   ```powershell
   python app.py
   # or
   powershell -File pc_companion/run_server.ps1
   ```
