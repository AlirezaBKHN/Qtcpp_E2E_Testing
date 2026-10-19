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

implicit_timeout_ms = 10_000
