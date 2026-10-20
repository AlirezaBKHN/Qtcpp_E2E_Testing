"""Foreground enforcement for the app under test.

WinAppDriver delivers keyboard input to whatever window currently has focus
and clicks to whatever sits at the element's screen coordinates. If another
window (a terminal, a chat, anything the operator has open) is in front of
the app after launch, automation types into THAT window and clicks across
the desktop — observed live ("Tehran" landed in a chat window).

So every session must force the app window to the OS foreground before any
interaction happens. SetForegroundWindow alone is refused by Windows
focus-stealing rules; the standard unlock is a synthetic ALT press before
the call (the OS permits activation shortly after a menu key event).
"""
from __future__ import annotations

import ctypes
import time

APP_WINDOW_TITLE = "Iran Signal Tracker"
SIGNIN_WINDOW_TITLE = "Sign in - Iran Signal Tracker"  # launch gate (Task 3)

APPLE_APP_WINDOW_TITLES = (SIGNIN_WINDOW_TITLE, APP_WINDOW_TITLE)

_SetForegroundWindow = ctypes.windll.user32.SetForegroundWindow
_FindWindowW = ctypes.windll.user32.FindWindowW
_GetForegroundWindow = ctypes.windll.user32.GetForegroundWindow
_keybd_event = ctypes.windll.user32.keybd_event


def _hwnd() -> int:
    for title in APPLE_APP_WINDOW_TITLES:
        hwnd = _FindWindowW(None, title)
        if hwnd:
            return hwnd
    return 0


def is_foreground() -> bool:
    hwnd = _hwnd()
    return bool(hwnd) and _GetForegroundWindow() == hwnd


def focus_app(retries: int = 5) -> None:
    """Bring the app window to the foreground, deterministically.

    Raises if the window is not present at all — callers rely on it having
    succeeded, since every later keyboard/click event lands on the
    foreground window.
    """
    for attempt in range(retries):
        hwnd = _hwnd()
        if not hwnd:
            if attempt == retries - 1:
                raise RuntimeError(f"window '{APP_WINDOW_TITLE}' not found")
            time.sleep(0.5)
            continue
        # ALT press/release unlocks SetForegroundWindow for this process.
        _keybd_event(0x12, 0, 0, 0)   # VK_MENU down
        _SetForegroundWindow(hwnd)
        _keybd_event(0x12, 0, 2, 0)   # VK_MENU up (KEYEVENTF_KEYUP)
        if is_foreground():
            return
        time.sleep(0.3)
    raise RuntimeError(
        f"could not bring '{APP_WINDOW_TITLE}' to the foreground "
        "after %d attempts" % retries)
