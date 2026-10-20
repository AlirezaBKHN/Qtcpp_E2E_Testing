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
_KEYUP = 0x02  # KEYEVENTF_KEYUP


def _press(vk: int, shift: bool) -> None:
    if shift:
        user32.keybd_event(_VK_SHIFT, 0, 0, 0)
    user32.keybd_event(vk, 0, 0, 0)
    user32.keybd_event(vk, 0, _KEYUP, 0)
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