"""Central configuration for the Iran Signal Tracker automation stack.

Every path/port is overridable through env variables so the repo can be
cloned anywhere without edits (see docs/env-setup.md for pinned versions).
"""
from __future__ import annotations

import os
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# The application under test.
APP_EXE = Path(os.environ.get("IST_APP_EXE", REPO_ROOT / "app" / "build-mingw" / "IranSignalTracker.exe"))

# Export CSVs land here (the app writes relative to its own location; the
# repo-local directory is the canonical one the tests assert against).
EXPORT_DIR = Path(os.environ.get("IST_EXPORT_DIR", REPO_ROOT / "reports" / "export"))

# Allure result directory (written by allure-pytest).
ALLURE_RESULTS = Path(os.environ.get("IST_ALLURE_RESULTS", REPO_ROOT / "reports" / "allure-results"))

# Endpoints.
APPIUM_URL = os.environ.get("IST_APPIUM_URL", "http://127.0.0.1:4723")
DIRECT_URL = os.environ.get("IST_DIRECT_URL", "http://127.0.0.1:4729")

# DRIVER_MODE: "appium" (default, Appium 3 server in the loop) | "direct"
# (tests talk straight to WinAppDriver; hidden fallback).
DRIVER_MODE = os.environ.get("DRIVER_MODE", "appium").strip().lower()

# WinAppDriver element-location strategy names are the Selenium "id"/"name"
# constants; we keep them local to avoid importing selenium in config.
BY_ID = "id"          # UIA AutomationId
BY_NAME = "name"      # UIA Name (accessible name)
BY_XPATH = "xpath"

# 2s: most finds hit in <0.3s; misses (popup-item polls, phase churn) must
# not burn 10s each - probed 2026-10-20: attached-session finds answer in
# ~0.2s and the popup settle sleep covers the draw latency.
implicit_timeout_ms = 2_000

# -- Auth + tabbed app (auth-tabs-dashboards plan) --------------------------
# Seeded demo accounts (owner-confirmed; plaintext demo-grade).
E2E_USER = "e2e_user"
E2E_PASSWORD = "e2e123"
ADMIN_USER = "admin"
ADMIN_PASSWORD = "admin"

# The DISPOSABLE working db the fixture system materializes per test
# (fixtures/ is the source of truth; see automation/fixtures.py).
DB_PATH = Path(os.environ.get(
    "IST_DB_PATH", REPO_ROOT / "database" / "signaltracker.db"))

SIGNIN_IDS = (
    "signinWindow", "usernameField", "passwordField",
    "loginButton", "registerToggle", "registerButton",
    "signinStatus", "registerStatus",
)
TAB_IDS = (
    "mainTabs", "homeTab", "networkTab", "dashboardTab",
    "reportsTab", "usersTab",
)
HOME_CARD_IDS = (
    "homeCardNetwork", "homeCardDashboard", "homeCardReports", "homeCardUsers",
)
TOP_BAR_IDS = ("currentUserLabel", "logoutButton")
