"""
Windows System Controller for M5Dial PC Control Center
Handles screen brightness, system volume, browser zoom, media controls,
and window navigation on Windows 11.
"""

import sys
import os
import time
import subprocess
import ctypes
from typing import Dict, Any, Tuple

# Win32 Virtual-Key Codes
VK_SHIFT = 0x10
VK_CONTROL = 0x11
VK_MENU = 0x12       # Alt key
VK_TAB = 0x09
VK_VOLUME_MUTE = 0xAD
VK_VOLUME_DOWN = 0xAE
VK_VOLUME_UP = 0xAF
VK_MEDIA_NEXT_TRACK = 0xB0
VK_MEDIA_PREV_TRACK = 0xB1
VK_MEDIA_STOP = 0xB2
VK_MEDIA_PLAY_PAUSE = 0xB3
VK_OEM_PLUS = 0xBB   # '+' key
VK_OEM_MINUS = 0xBD  # '-' key
VK_0 = 0x30          # '0' key (zoom reset)

# Mouse event constants
MOUSEEVENTF_WHEEL = 0x0800
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_EXTENDEDKEY = 0x0001

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32


def _send_key(vk: int, extended: bool = False, delay_ms: float = 0.02) -> None:
    """Send a single keydown and keyup event."""
    flags_down = KEYEVENTF_EXTENDEDKEY if extended else 0
    flags_up = (KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP) if extended else KEYEVENTF_KEYUP
    user32.keybd_event(vk, 0, flags_down, 0)
    time.sleep(delay_ms)
    user32.keybd_event(vk, 0, flags_up, 0)


def _send_combo(modifier_vks: list, target_vk: int, delay_ms: float = 0.03) -> None:
    """Press modifiers, press target key, release target key, release modifiers."""
    for mod in modifier_vks:
        user32.keybd_event(mod, 0, 0, 0)
        time.sleep(0.01)

    user32.keybd_event(target_vk, 0, 0, 0)
    time.sleep(delay_ms)
    user32.keybd_event(target_vk, 0, KEYEVENTF_KEYUP, 0)
    time.sleep(0.01)

    for mod in reversed(modifier_vks):
        user32.keybd_event(mod, 0, KEYEVENTF_KEYUP, 0)
        time.sleep(0.01)


class WindowsController:
    """Dispatches and executes PC control operations on Windows 11."""

    def __init__(self):
        self._sbc_available = False
        self._pycaw_available = False
        self._current_brightness = 100
        self._check_optional_libraries()

    def _check_optional_libraries(self):
        """Check for optional libraries dynamically."""
        try:
            import screen_brightness_control as sbc  # noqa
            self._sbc_available = True
        except Exception:
            self._sbc_available = False

        try:
            from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume  # noqa
            from comtypes import CLSCTX_ALL  # noqa
            self._pycaw_available = True
        except Exception:
            self._pycaw_available = False

    # ---------------------------------------------------------
    # 1. System Volume Controls
    # ---------------------------------------------------------
    def volume_delta(self, delta: int) -> Tuple[bool, str]:
        """Adjust system volume up or down."""
        self._check_optional_libraries()
        if self._pycaw_available:
            try:
                from pycaw.pycaw import AudioUtilities
                devices = AudioUtilities.GetSpeakers()
                epv = getattr(devices, "EndpointVolume", None)
                if epv is None and hasattr(devices, "Activate"):
                    from pycaw.pycaw import IAudioEndpointVolume
                    from comtypes import CLSCTX_ALL
                    interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
                    epv = ctypes.cast(interface, ctypes.POINTER(IAudioEndpointVolume))
                if epv is not None:
                    current = epv.GetMasterVolumeLevelScalar()
                    new_vol = max(0.0, min(1.0, current + (delta / 100.0)))
                    epv.SetMasterVolumeLevelScalar(new_vol, None)
                    if delta > 0 and epv.GetMute():
                        epv.SetMute(False, None)
                    return True, f"System volume set to {int(round(new_vol * 100))}%"
            except Exception:
                pass

        vk = VK_VOLUME_UP if delta > 0 else VK_VOLUME_DOWN
        steps = max(1, abs(delta) // 2)
        for _ in range(steps):
            _send_key(vk, extended=True, delay_ms=0.01)
        direction = "up" if delta > 0 else "down"
        return True, f"System volume adjusted {direction} ({steps} steps)"

    def mute(self, _val: int = 0) -> Tuple[bool, str]:
        """Toggle audio mute state."""
        self._check_optional_libraries()
        if self._pycaw_available:
            try:
                from pycaw.pycaw import AudioUtilities
                devices = AudioUtilities.GetSpeakers()
                epv = getattr(devices, "EndpointVolume", None)
                if epv is None and hasattr(devices, "Activate"):
                    from pycaw.pycaw import IAudioEndpointVolume
                    from comtypes import CLSCTX_ALL
                    interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
                    epv = ctypes.cast(interface, ctypes.POINTER(IAudioEndpointVolume))
                if epv is not None:
                    current_mute = epv.GetMute()
                    epv.SetMute(not current_mute, None)
                    status = "unmuted" if current_mute else "muted"
                    return True, f"System audio {status}"
            except Exception:
                pass

        _send_key(VK_VOLUME_MUTE, extended=True)
        return True, "Audio mute toggled"

    def get_status(self) -> Dict[str, Any]:
        """Query real-time Windows master volume, mute state, and screen brightness."""
        vol = 50
        muted = False
        self._check_optional_libraries()
        if self._pycaw_available:
            try:
                from pycaw.pycaw import AudioUtilities
                speakers = AudioUtilities.GetSpeakers()
                if hasattr(speakers, "volume_percent") and speakers.volume_percent is not None:
                    vol = int(round(speakers.volume_percent))
                elif hasattr(speakers, "EndpointVolume"):
                    vol = int(round(speakers.EndpointVolume.GetMasterVolumeLevelScalar() * 100))
                if hasattr(speakers, "EndpointVolume"):
                    muted = bool(speakers.EndpointVolume.GetMute())
            except Exception:
                pass

        br = self._current_brightness
        if self._sbc_available:
            try:
                import screen_brightness_control as sbc
                b = sbc.get_brightness()
                curr_val = b[0] if isinstance(b, list) and b else b
                if curr_val is not None:
                    br = int(curr_val)
                    self._current_brightness = br
            except Exception:
                pass
        else:
            try:
                ps_script = "(Get-CimInstance -Namespace root/wmi -ClassName WmiMonitorBrightness).CurrentBrightness"
                res = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", ps_script], capture_output=True, text=True, timeout=3)
                out = res.stdout.strip()
                if out.isdigit():
                    br = int(out)
                    self._current_brightness = br
            except Exception:
                pass

        return {
            "volume": vol,
            "brightness": br,
            "muted": muted
        }

    # ---------------------------------------------------------
    # 2. Screen Brightness Controls
    # ---------------------------------------------------------
    def _set_gamma_brightness(self, level: int) -> bool:
        """Universal Windows display brightness using GDI Gamma Ramp."""
        level = max(10, min(100, level))
        ramp = (ctypes.c_ushort * 768)()
        for i in range(256):
            val = int(i * 256 * (level / 100.0))
            val = min(65535, max(0, val))
            ramp[i] = val          # Red
            ramp[256 + i] = val    # Green
            ramp[512 + i] = val    # Blue

        hdc = user32.GetDC(None)
        if not hdc:
            return False
        success = gdi32.SetDeviceGammaRamp(hdc, ramp)
        user32.ReleaseDC(None, hdc)
        return bool(success)

    def brightness_delta(self, delta: int) -> Tuple[bool, str]:
        """Adjust screen brightness using the best available Windows method."""
        self._check_optional_libraries()

        # Method 1: screen-brightness-control package (if installed)
        if self._sbc_available:
            try:
                import screen_brightness_control as sbc
                current = sbc.get_brightness()
                curr_val = current[0] if isinstance(current, list) and current else current
                if curr_val is not None:
                    new_val = max(0, min(100, int(curr_val) + delta))
                    sbc.set_brightness(new_val)
                    self._current_brightness = new_val
                    self._trigger_osd(new_val)
                    return True, f"Hardware brightness set to {new_val}% (sbc)"
            except Exception:
                pass

        # Method 2: Official Windows 11 PowerShell CIM / WMI Invocation
        try:
            ps_script = (
                "$ErrorActionPreference = 'Stop'; "
                "try { "
                "$m = Get-CimInstance -Namespace root/wmi -ClassName WmiMonitorBrightnessMethods; "
                "$b = Get-CimInstance -Namespace root/wmi -ClassName WmiMonitorBrightness; "
                "if ($m -and $b) { "
                "$cur = [int]$b.CurrentBrightness; "
                f"$target = [int][math]::Max(0, [math]::Min(100, $cur + ({delta}))); "
                "Invoke-CimMethod -InputObject $m -MethodName WmiSetBrightness -Arguments @{ Timeout = [uint32]1; Brightness = [byte]$target } | Out-Null; "
                "$after = (Get-CimInstance -Namespace root/wmi -ClassName WmiMonitorBrightness).CurrentBrightness; "
                'Write-Output "OK:$after"; '
                "} else { "
                'Write-Output "FAIL:NO_WMI_MONITOR"; '
                "} "
                "} catch { "
                'Write-Output "FAIL:$($_.Exception.Message)"; '
                "}"
            )
            res = subprocess.run(
                ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", ps_script],
                capture_output=True, text=True, timeout=5
            )
            out = res.stdout.strip()
            if out.startswith("OK:"):
                val = int(out.split(":", 1)[1])
                self._current_brightness = val
                self._trigger_osd(val)
                return True, f"Screen brightness set to {val}%"
        except Exception:
            pass

        # Method 3: Universal Windows GDI Display Gamma Ramp (runs on 100% of screens)
        try:
            new_level = max(10, min(100, self._current_brightness + delta))
            ok = self._set_gamma_brightness(new_level)
            if ok:
                self._current_brightness = new_level
                self._trigger_osd(new_level)
                return True, f"Screen brightness set to {self._current_brightness}% (Gamma)"
        except Exception as e:
            return False, f"Brightness error: {str(e)}"

        return False, "Could not adjust screen brightness"

    def _trigger_osd(self, val: int) -> None:
        """Summon the brightness on-screen display (OSD) flyout if enabled in config."""
        if os.environ.get("ENABLE_BRIGHTNESS_OSD", "false").lower() not in ("true", "1", "yes"):
            return
        try:
            try:
                from .osd import get_brightness_osd
            except ImportError:
                from pc_companion.osd import get_brightness_osd
            get_brightness_osd().show(val)
        except Exception:
            pass

    # ---------------------------------------------------------
    # 3. Media Playback Controls
    # ---------------------------------------------------------
    def media_play_pause(self, _val: int = 0) -> Tuple[bool, str]:
        """Toggle media play/pause."""
        _send_key(VK_MEDIA_PLAY_PAUSE, extended=True)
        return True, "Media play/pause toggled"

    def media_next(self, _val: int = 0) -> Tuple[bool, str]:
        """Skip to next media track."""
        _send_key(VK_MEDIA_NEXT_TRACK, extended=True)
        return True, "Next track requested"

    def media_previous(self, _val: int = 0) -> Tuple[bool, str]:
        """Go to previous media track."""
        _send_key(VK_MEDIA_PREV_TRACK, extended=True)
        return True, "Previous track requested"

    # ---------------------------------------------------------
    # 4. Browser Tab Zoom Controls
    # ---------------------------------------------------------
    def zoom_in(self, _val: int = 0) -> Tuple[bool, str]:
        """Zoom in active browser/window (Ctrl + Plus)."""
        _send_combo([VK_CONTROL], VK_OEM_PLUS)
        return True, "Browser zoom in (Ctrl + Plus)"

    def zoom_out(self, _val: int = 0) -> Tuple[bool, str]:
        """Zoom out active browser/window (Ctrl + Minus)."""
        _send_combo([VK_CONTROL], VK_OEM_MINUS)
        return True, "Browser zoom out (Ctrl + Minus)"

    def zoom_reset(self, _val: int = 0) -> Tuple[bool, str]:
        """Reset browser zoom (Ctrl + 0)."""
        _send_combo([VK_CONTROL], VK_0)
        return True, "Browser zoom reset (Ctrl + 0)"

    # ---------------------------------------------------------
    # 5. Browser Navigation & System Utilities
    # ---------------------------------------------------------
    def browser_tab_next(self, _val: int = 0) -> Tuple[bool, str]:
        """Switch to next browser tab (Ctrl + Tab)."""
        _send_combo([VK_CONTROL], VK_TAB)
        return True, "Next tab (Ctrl + Tab)"

    def browser_tab_previous(self, _val: int = 0) -> Tuple[bool, str]:
        """Switch to previous browser tab (Ctrl + Shift + Tab)."""
        _send_combo([VK_CONTROL, VK_SHIFT], VK_TAB)
        return True, "Previous tab (Ctrl + Shift + Tab)"

    def app_switch(self, _val: int = 0) -> Tuple[bool, str]:
        """Switch active window (Alt + Tab)."""
        _send_combo([VK_MENU], VK_TAB)
        return True, "App switched (Alt + Tab)"

    def scroll_up(self, _val: int = 0) -> Tuple[bool, str]:
        """Simulate mouse wheel scroll up."""
        user32.mouse_event(MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
        return True, "Scrolled up"

    def scroll_down(self, _val: int = 0) -> Tuple[bool, str]:
        """Simulate mouse wheel scroll down."""
        user32.mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -120, 0)
        return True, "Scrolled down"

    # ---------------------------------------------------------
    # Dispatcher
    # ---------------------------------------------------------
    def execute(self, command: str, value: int = 0) -> Tuple[bool, str]:
        """Dispatch command name to the corresponding method."""
        handlers = {
            "volume_delta": lambda: self.volume_delta(value),
            "mute": lambda: self.mute(value),
            "brightness_delta": lambda: self.brightness_delta(value),
            "media_play_pause": lambda: self.media_play_pause(value),
            "media_next": lambda: self.media_next(value),
            "media_previous": lambda: self.media_previous(value),
            "zoom_in": lambda: self.zoom_in(value),
            "zoom_out": lambda: self.zoom_out(value),
            "zoom_reset": lambda: self.zoom_reset(value),
            "browser_tab_next": lambda: self.browser_tab_next(value),
            "browser_tab_previous": lambda: self.browser_tab_previous(value),
            "app_switch": lambda: self.app_switch(value),
            "scroll_up": lambda: self.scroll_up(value),
            "scroll_down": lambda: self.scroll_down(value),
        }

        handler = handlers.get(command)
        if not handler:
            return False, f"Unknown command: '{command}'"

        try:
            return handler()
        except Exception as e:
            return False, f"Execution error for '{command}': {str(e)}"
