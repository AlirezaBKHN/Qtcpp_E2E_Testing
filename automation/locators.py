"""Stable locator candidates per control.

Every entry is an ordered list of (by, value) candidates; the page objects
walk the list from strongest to weakest. No coordinates anywhere
(Review Focus #4).

Frozen from live discovery (automation/tools/inspect_ui.py, Appium session
2026-10-19): Qt exposes accessibleName as UIA `Name`, which exactly equals
the stable identifier. AutomationId is PREFIXED for nested widgets
(`QApplication.MainWindow.QWidget.provinceCombo`), so plain `id` matching
only works for some widgets (e.g. signalTable) and stays as fallback.

Readout widgets (READOUT_IDS) are the exception: objectName keeps their id
(AutomationId is stable) but the accessibleName mirrors the DISPLAYED text
(the only readable surface, via get_attribute("Name"), after the probe
proved no attribute can read a QLineEdit's value) - so they are addressed
by an AutomationId contains-XPath instead of Name.
"""
from __future__ import annotations

import config as C

# READOUT widgets (signinStatus, registerStatus, currentUserLabel, kpi*):
# their objectName (== AutomationId) keeps the stable id, but the
# accessibleName mirrors the DISPLAYED TEXT (that is the only readable
# surface WAD offers - probed: no attribute can read a QLineEdit's value),
# so Name-based locators cannot address them: AutomationId XPath first,
# name match kept as a legacy fallback.
XPATH_BY_AID = "//Edit[contains(@AutomationId,'%s')]"

READOUT_IDS = (
    "signinStatus", "registerStatus", "currentUserLabel",
    "kpiAvgQuality", "kpiCells", "kpiBestOperator", "kpiWorstOperator",
)

CONTROLS: dict[str, list[tuple[str, str]]] = {}
for _key in (
    "provinceCombo", "cityCombo", "technologyCombo", "operatorCombo",
    "searchButton", "refreshButton", "resetButton", "exportButton",
    "signalTable", "signalChart", "chartGroupBy",
    # Sign-in dialog.
    "signinWindow", "usernameField", "passwordField",
    "loginButton", "registerToggle", "registerButton",
    # Tabs + top bar + home cards.
    "mainTabs",
    "homeTab", "networkTab", "dashboardTab", "reportsTab", "usersTab",
    "logoutButton",
    "homeCardNetwork", "homeCardDashboard", "homeCardReports", "homeCardUsers",
    # Dashboard.
    "trendChart", "worstCellsList",
    # Reports.
    "reportGroupBy", "reportTable", "dailyTrendChart", "exportReportButton",
    # Users.
    "usersTable", "removeUserButton",
    # Named E2E SECTIONS (small UIA subtrees the finds scope into; the app
    # wraps the filter row and the top bar in transparent containers -
    # app/src/mainwindow.cpp. Probed 2026-10-20: a scoped find answers in
    # 0.02s vs 11-38s from the window root, whose walk carries the
    # 10,080-row table).
    "filterSection", "headerSection",
):
    CONTROLS[_key] = [(C.BY_NAME, _key), (C.BY_ID, _key)]
for _key in READOUT_IDS:
    CONTROLS[_key] = [(C.BY_XPATH, XPATH_BY_AID % _key),
                      (C.BY_NAME, _key), (C.BY_ID, _key)]

# Status-bar readout ('Showing N rows'): its Name mirrors the live text, so
# it is addressed by an XPath prefix-match; the handle is cached like every
# other control and get_attribute("Name") reads the CURRENT text (live
# property, probed 0.01s via a cached ref).
CONTROLS["statusText"] = [
    (C.BY_XPATH, "//Window//*[contains(@Name,'Showing')]")]

# QTabBar items expose the tab TITLE (not the page id) and are stable for
# the window's lifetime: cache them like any control (their first find on
# the fat Network tab costs ~11s, after that it is free).
for _page, _title in {
    "homeTab": "Home",
    "networkTab": "Network Data",
    "dashboardTab": "Dashboard",
    "reportsTab": "Reports",
    "usersTab": "Users",
}.items():
    CONTROLS[f"tabItem_{_page}"] = [
        (C.BY_XPATH,
         f"//Window[@Name='Iran Signal Tracker']//TabItem[@Name='{_title}']")]


def candidates(control: str) -> list[tuple[str, str]]:
    return CONTROLS[control]
