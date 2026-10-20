"""The human-readable DSL facade. Tests must only speak this language.

Auth-first binding contract (auth-tabs-dashboards plan):

    tracker.launch()          # gates on the signin window (foreground forced)
    tracker.register(u, p)    # performs; register_message() reads the label
    tracker.login(u, p)       # performs; signin_message() reads the label
    tracker.logout()          # lands back on the sign-in dialog
    tracker.current_user()    # top-bar readout ('' before login)
    tracker.network()         # -> NetworkDsl (filters/table/chart/export/... )
    tracker.dashboard()       # -> DashboardDsl (kpi/trend_points/worst cells)
    tracker.reports()         # -> ReportsDsl
    tracker.users()           # -> UsersDsl, admin only (raises otherwise)

Actions perform; readers return data; the TESTS assert. No hybrid
login_should_fail-style methods.
"""
from __future__ import annotations

import time

from drivers import factory
from drivers.factory import quit_driver
from pageobjects import resolve_element
from pageobjects.resolve_element import find
from drivers.foreground import focus_app
from dsl.dashboard import DashboardDsl
from dsl.network import NetworkDsl
from dsl.reports import ReportsDsl
from dsl.users import UsersDsl
from pageobjects.signin_page import SignInPage
from pageobjects.tabs_bar import TabsBar

_SIGNIN_WINDOW_TITLE = factory._SIGNIN_WINDOW_TITLE
_MAIN_WINDOW_TITLE = factory._MAIN_WINDOW_TITLE


class SignalTrackerDsl:
    def __init__(self):
        self._driver = None
        self._signin = None
        self._tabs = None

    # -- phase plumbing -------------------------------------------------
    def _attach_phase(self, window_title: str) -> None:
        """Re-attach the session to a NEW top-level window phase.

        WAD pins a session to one window handle (see drivers/factory.py);
        every auth transition destroys the old window, so page objects are
        rebuilt on the fresh session after each attach. Cached element
        handles die with the old session - drop them.
        """
        self._driver = factory.attach(window_title)
        resolve_element.invalidate()
        self._signin = SignInPage(self._driver)
        self._tabs = TabsBar(self._driver)
        focus_app()

    # -- lifecycle ------------------------------------------------------
    def launch(self) -> "SignalTrackerDsl":
        """Open the app and land on the sign-in window.

        The app must be OS-foreground BEFORE any interaction (WAD sends
        keystrokes to the focused window, clicks to whatever occupies the
        element's screen coordinates - observed: test strings ended up in a
        chat window). launch() hard-fails if it cannot acquire focus.
        """
        self._attach_phase(_SIGNIN_WINDOW_TITLE)
        self._signin.wait_visible()
        focus_app()
        return self

    def quit(self) -> None:
        quit_driver()
        self._driver = None
        self._signin = None
        self._tabs = None

    # -- auth actions ---------------------------------------------------
    def register(self, username: str, password: str) -> None:
        """Switch to register mode in place, fill, submit. The outcome is
        read back through register_message()."""
        self._signin.click_register_toggle()
        self._signin.fill(username, password)
        self._signin.click_register()

    def register_message(self) -> str:
        """The register-mode status; after a SUCCESS the dialog switched
        back to login mode carrying the pinned copy there, so fall back to
        the login status readout."""
        try:
            return self._signin.register_message()
        except LookupError:
            return self._signin.signin_message()

    def toggle_register_mode(self) -> None:
        """Flip Login <-> Register in place (navigation only; a failed
        register leaves the dialog in register mode)."""
        self._signin.click_register_toggle()

    def login(self, username: str, password: str) -> None:
        """Fill the sign-in form and submit. The outcome is read back
        through signin_message(); success re-attaches the session to the
        tabbed main window (a new top-level phase)."""
        self._signin.fill(username, password)
        focus_app()
        self._signin.click_login()
        deadline = time.time() + 15
        while time.time() < deadline:
            if factory._find_window(_MAIN_WINDOW_TITLE):
                self._attach_phase(_MAIN_WINDOW_TITLE)
                return  # main window phase
            time.sleep(0.3)
        return  # dialog still there: login failed; read signin_message()

    def signin_message(self) -> str:
        return self._signin.signin_message()

    def logout(self) -> None:
        self._tabs.click_logout()
        # The main window is destroyed and the sign-in dialog reappears as
        # a NEW top-level window: re-attach to its phase.
        self._attach_phase(_SIGNIN_WINDOW_TITLE)
        self._signin.wait_visible()

    def current_user(self) -> str:
        return self._tabs.current_user()

    # -- sub-tab access -------------------------------------------------
    def _route_home(self, card: str) -> None:
        """Route to a tab via the Home card for that page.

        The cards live only on the homeTab page (hidden pages are pruned
        from the UIA control view), so routing goes Home (QTabBar) first -
        deterministic whether this is a fresh session or a mid-flow switch.
        """
        self._tabs.click_tab("homeTab")
        self._tabs.click_home_card(card)

    def network(self) -> NetworkDsl:
        return NetworkDsl(self._driver, self._route_home)

    def dashboard(self) -> DashboardDsl:
        return DashboardDsl(self._driver, self._route_home)

    def reports(self) -> ReportsDsl:
        return ReportsDsl(self._driver, self._route_home)

    def users(self) -> UsersDsl:
        """Admin-only: raises when the Users tab is not mounted.

        The gate reads the QTabBar item ('Users' exists only for admin
        sessions): the usersTab PAGE widget is a hidden tab page and WAD
        prunes hidden pages from an attached session's tree.
        """
        try:
            find(self._driver, "tabItem_usersTab")
        except LookupError as exc:
            raise AssertionError(
                "users tab unavailable (the usersTab page is mounted for "
                "admin sessions only)") from exc
        return UsersDsl(self._driver, self._route_home)
