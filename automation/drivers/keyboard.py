"""OS-level keyboard input (ctypes keybd_event) for popup interactions.

WAD send_keys needs an element target and has hung sessions on combos
before (newline on a closed combo); this module types through the OS
instead, reaching whatever owns keyboard focus. Contract: the app is
OS-foreground (drivers/foreground.py) and the combo popup grabs keyboard
focus the moment it opens - so type-ahead + Enter land in the popup.
"""
from __future__ import annotations

import ctypes
import time

user32 = ctypes.windll.user32

_VK_SHIFT = 0x10
_VK_RETURN = 0x0D
_VK_LEFT = 0x25
_VK_RIGHT = 0x27
_KEYUP = 0x02  # KEYEVENTF_KEYUP
_KEYEXT = 0x01  # KEYEVENTF_EXTENDEDKEY (arrows need it; without it apps
                # may read the numpad glyph instead of the arrow)


def _press(vk: int, shift: bool, extended: bool = False) -> None:
    flags = 0
    if extended:
        flags |= _KEYEXT
    if shift:
        user32.keybd_event(_VK_SHIFT, 0, 0, 0)
    user32.keybd_event(vk, 0, flags, 0)
    user32.keybd_event(vk, 0, flags | _KEYUP, 0)
    if shift:
        user32.keybd_event(_VK_SHIFT, 0, _KEYUP, 0)


def type_text(text: str, per_char_s: float = 0.05) -> None:
    """Type `text` character by character (VkKeyScanW maps char -> vk)."""
    for ch in text:
        code = user32.VkKeyScanW(ord(ch))
        if code == -1:
            raise ValueError(f"character {ch!r} has no virtual-key mapping")
        _press(code & 0xFF, bool((code >> 8) & 1))
        time.sleep(per_char_s)


def press_enter() -> None:
    _press(_VK_RETURN, shift=False)


def press_left(times: int = 1) -> None:
    """Left-arrow presses; clamps at the first section of a date edit.

    Injected arrows NEED KEYEVENTF_EXTENDEDKEY (probed: without it Qt read
    the numpad glyph and section navigation was chaos).
    """
    for _ in range(times):
        _press(_VK_LEFT, shift=False, extended=True)
        time.sleep(0.02)


def press_right(times: int = 1) -> None:
    for _ in range(times):
        _press(_VK_RIGHT, shift=False, extended=True)
        time.sleep(0.02)