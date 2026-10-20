"""Sign-in dialog page object (the app's first screen)."""
from __future__ import annotations

import time

from pageobjects.resolve_element import find


def read_visible_text(driver, control_ids) -> str:
    """Read the FIRST displayed control's text among `control_ids`.

    Hidden readouts (e.g. a register-mode status after the dialog switched
    back to login mode) stay findable by Name but carry no text - so only
    displayed elements may answer.
    """
    for control_id in control_ids:
        try:
            el = find(driver, control_id)
        except Exception:  # noqa: BLE001 - hidden/unknown control: try next
            continue
        try:
            if not el.is_displayed():
                continue
        except Exception:  # noqa: BLE001 - displayed check unsupported: keep
            pass
        text = _read_text(el)
        if text and text != control_id:
            return text
    return ""


def _read_text(el) -> str:
    """Read a control's displayed text through whatever UIA surface has it.

    Readout widgets keep the stable id in objectName/AutomationId and mirror
    the displayed text in the accessibleName - which is the UIA Name, the
    only readable surface WAD offers (probed: no attribute key can read a
    QLineEdit's content - Value/HelpText/Text all None). Plain QLabels
    (captions) expose their text as the Name directly. The Qt fallback for
    an ID-less QLineEdit is the class name, filtered here.
    """
    for attr in ("Value", "HelpText", "Name"):
        try:
            value = el.get_attribute(attr)
        except Exception:  # noqa: BLE001 - attribute may be unsupported
            value = None
        if value and value not in ("QLineEdit", "QLabel", "QTextEdit"):
            return value
    return ""


class SignInPage:
    """Owns every interaction on the signinWindow."""

    SIGNIN_XPATH = "//*[contains(@Name,'signinWindow') or contains(@AutomationId,'signinWindow')]"

    def __init__(self, driver):
        self._driver = driver

    def wait_visible(self, timeout: float = 30.0) -> "SignInPage":
        """Poll for the sign-in dialog (the launch-gate first screen)."""
        deadline = time.time() + timeout
        last_error = None
        while time.time() < deadline:
            try:
                self._el("signinWindow")
                return self
            except Exception as exc:  # noqa: BLE001
                last_error = exc
                time.sleep(0.5)
        raise AssertionError(
            f"sign-in window did not appear within {timeout}s") from last_error

    # -- lookup --------------------------------------------------------
    def _el(self, control: str):
        return find(self._driver, control)

    # -- actions (perform, no assertion) -------------------------------
    def fill(self, username: str, password: str) -> None:
        self._el("usernameField").send_keys(username)
        self._el("passwordField").send_keys(password)

    def click_login(self) -> None:
        self._el("loginButton").click()

    def click_register_toggle(self) -> None:
        self._el("registerToggle").click()

    def click_register(self) -> None:
        self._el("registerButton").click()

    # -- readers (return data; the TEST asserts) -----------------------
    def signin_message(self) -> str:
        """Login mode's status text ('' when no message shown)."""
        return read_visible_text(self._driver, ("signinStatus",))

    def register_message(self) -> str:
        """Register mode's status text; after a SUCCESS the dialog switched
        back to login mode carrying the pinned copy there, so try both - the
        displayed one wins."""
        return read_visible_text(self._driver, ("registerStatus", "signinStatus"))
