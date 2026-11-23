"""
Modern Windows 11-style On-Screen Display (OSD) Flyout for Screen Brightness.
Renders a fluent, dark-mode pill indicator at bottom-center of the monitor
with a sun icon, percentage text, and animated progress track.
"""

import threading
import ctypes
from typing import Optional

user32 = ctypes.windll.user32
dwmapi = ctypes.windll.dwmapi

DWMWA_WINDOW_CORNER_PREFERENCE = 33
DWMWCP_ROUND = 2

class BrightnessOSD:
    """Thread-safe, non-blocking On-Screen Display for brightness changes."""

    def __init__(self):
        self._root = None
        self._canvas = None
        self._timer = None
        self._thread: Optional[threading.Thread] = None
        self._lock = threading.Lock()
        self._ready = threading.Event()
        self._target_val = 50
        self._active = False

    def _ui_loop(self):
        """Dedicated Tkinter GUI event loop in a daemon thread."""
        try:
            import tkinter as tk
        except ImportError:
            return

        try:
            self._root = tk.Tk()
            self._root.title("M5Dial_Brightness_OSD")
            self._root.overrideredirect(True)
            self._root.attributes("-topmost", True)
            self._root.attributes("-alpha", 0.94)
            self._root.configure(bg="#202020")

            # Dimensions matching Windows 11 OSD pill
            width, height = 280, 56
            screen_w = self._root.winfo_screenwidth()
            screen_h = self._root.winfo_screenheight()
            x = (screen_w - width) // 2
            y = screen_h - height - 85  # Bottom center, right above taskbar

            self._root.geometry(f"{width}x{height}+{x}+{y}")
            self._root.update_idletasks()

            hwnd = self._root.winfo_id()

            # Enable Windows 11 native rounded corners
            try:
                corner_pref = ctypes.c_int(DWMWCP_ROUND)
                dwmapi.DwmSetWindowAttribute(
                    hwnd,
                    DWMWA_WINDOW_CORNER_PREFERENCE,
                    ctypes.byref(corner_pref),
                    ctypes.sizeof(corner_pref)
                )
            except Exception:
                pass

            # Window styles: Do not activate (never steal focus), topmost, tool window
            GWL_EXSTYLE = -20
            WS_EX_NOACTIVATE = 0x08000000
            WS_EX_TOPMOST = 0x00000008
            WS_EX_TOOLWINDOW = 0x00000080
            try:
                ex_style = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
                user32.SetWindowLongW(hwnd, GWL_EXSTYLE, ex_style | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW)
            except Exception:
                pass

            self._canvas = tk.Canvas(self._root, width=width, height=height, bg="#202020", highlightthickness=0)
            self._canvas.pack(fill="both", expand=True)

            self._redraw(self._target_val)
            self._ready.set()
            self._root.withdraw()  # Start hidden
            self._root.mainloop()
        except Exception:
            self._ready.set()

    def _redraw(self, val: int):
        """Draw sun icon, progress bar, and percentage label."""
        if not self._canvas:
            return
        self._canvas.delete("all")

        # Sun icon (crisp vector-drawn)
        cx, cy = 30, 28
        self._canvas.create_oval(cx - 6, cy - 6, cx + 6, cy + 6, fill="#FFFFFF", outline="")
        rays = [
            (cx, cy - 11, cx, cy - 8),
            (cx, cy + 8, cx, cy + 11),
            (cx - 11, cy, cx - 8, cy),
            (cx + 8, cy, cx + 11, cy),
            (cx - 8, cy - 8, cx - 6, cy - 6),
            (cx + 6, cy + 6, cx + 8, cy + 8),
            (cx - 8, cy + 8, cx - 6, cy + 6),
            (cx + 6, cy - 6, cx + 8, cy - 8),
        ]
        for x1, y1, x2, y2 in rays:
            self._canvas.create_line(x1, y1, x2, y2, fill="#FFFFFF", width=2)

        # Progress bar track
        bar_x1, bar_y1, bar_x2, bar_y2 = 56, 24, 215, 32
        self._canvas.create_rectangle(bar_x1, bar_y1, bar_x2, bar_y2, fill="#404040", outline="")

        # Progress fill (Windows 11 accent cyan/blue)
        fill_w = int((bar_x2 - bar_x1) * (max(0, min(100, val)) / 100.0))
        if fill_w > 0:
            self._canvas.create_rectangle(bar_x1, bar_y1, bar_x1 + fill_w, bar_y2, fill="#60CDFF", outline="")

        # Percentage Text
        self._canvas.create_text(245, 28, text=f"{val}%", fill="#FFFFFF", font=("Segoe UI", 11, "bold"))

    def show(self, val: int):
        """Display or update the OSD flyout with the current brightness value."""
        try:
            with self._lock:
                self._target_val = val
                if self._thread is None:
                    self._thread = threading.Thread(target=self._ui_loop, daemon=True)
                    self._thread.start()
                    self._ready.wait(timeout=1.5)

            if self._root:
                def _update():
                    try:
                        self._redraw(val)
                        self._root.deiconify()
                        self._root.lift()
                        if self._timer:
                            self._root.after_cancel(self._timer)
                        self._timer = self._root.after(1600, self._root.withdraw)
                    except Exception:
                        pass

                self._root.after(0, _update)
        except Exception:
            pass


# Global singleton
_global_osd = None
_osd_lock = threading.Lock()

def get_brightness_osd() -> BrightnessOSD:
    global _global_osd
    with _osd_lock:
        if _global_osd is None:
            _global_osd = BrightnessOSD()
        return _global_osd
