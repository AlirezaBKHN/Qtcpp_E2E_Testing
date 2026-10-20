"""Sign-in dialog page object (the app's first screen)."""
from __future__ import annotations

import time

from pageobjects.resolve_element import find, read_text


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
        text = read_text(el)
        if text and text != control_id:
            return text
    return ""


def _set_text(el, text: str) -> None:
    """REPLACE a line-edit's content with `text`.

    send_keys APPENDS (probed via TC012: a second fill accumulated the
    previous attempts' text, so the 'valid' login actually submitted a
    mangled username), so each fill clears first. clear() rides the UIA
    ValuePattern; the keystroke fallback (select-all + delete) needs the
    field focused, hence the click.
    """
    try:
        el.clear()
    except Exception:  # noqa: BLE001 - clear unsupported: keystroke path
        from selenium.webdriver.common.keys import Keys

        el.click()
        el.send_keys(Keys.CONTROL + "a")
        el.send_keys(Keys.DELETE)
    el.send_keys(text)


class SignInPage:
    """Owns every interaction on the signinWindow."""

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
        _set_text(self._el("usernameField"), username)
        _set_text(self._el("passwordField"), password)

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
