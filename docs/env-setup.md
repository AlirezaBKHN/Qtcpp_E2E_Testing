# Environment Setup — exact versions and paths

Stack provisioned 2026-10-19 by the coding agent (see plan Task 1).

| Component | Version | Path |
|---|---|---|
| Python | 3.12.8 (user-scope) | `C:\Users\user\AppData\Local\Programs\Python\Python312\python.exe` (shells opened before the install may not have it on PATH — scripts resolve it explicitly) |
| pip | 24.3.1 | same Python tree |
| CMake | 3.30.5 | `C:\Qt\Tools\CMake_64\bin\cmake.exe` |
| Ninja | (bundled) | `C:\Qt\Tools\Ninja\ninja.exe` |
| MinGW (gcc/g++) | 13.1.0 | `C:\Qt\Tools\mingw1310_64\bin\` |
| Qt | 6.8.3 win64_mingw (modules incl. `qtcharts`, `qtbase` with QtSql) | `C:\Qt\6.8.3\mingw_64` |
| Appium | 3.8.0 (npm global) | `appium` |
| Appium driver | `windows@6.3.1` (`appium-windows-driver`) | per-user `~\.appium` |
| WinAppDriver | 1.2.1 (MSI) | `C:\Program Files (x86)\Windows Application Driver\WinAppDriver.exe` |
| Allure CLI | 3.21.0 (npm) | `allure` |
| pytest / allure-pytest / appium-python-client | 9.1.1 / latest / latest | pip user-scope |

## Decisions worth knowing

- **Locator strategy (frozen from live discovery):** Qt 6.8.3's UIA bridge exposes each widget's accessibleName as UIA `Name` — use the NAME strategy for WinAppDriver/Appium lookups. AutomationId is prefixed (`QApplication.MainWindow.QWidget.<id>`) for nested widgets, so plain `id` matching fails except for top-level-exposed widgets (e.g. signalTable). Locators are name-first.
- **Qt runtime deployment is mandatory** before any automation (WinAppDriver launches the exe without developer PATH): run `C:\Qt\6.8.3\mingw_64\bin\windeployqt.exe --compiler-runtime app\build-mingw\IranSignalTracker.exe` after building. Missing libgcc_s_seh-1.dll/libstdc++-6.dll errors mean this step was skipped.
- **Appium house peering fix:** `~\.appium\node_modules` needs an `appium` junction pointing at the global npm package (`New-Item -ItemType Junction ...`) for `appium-windows-driver@6.3.1` to load under Appium 3.
- **Appium 3, not 2.** Current `appium-windows-driver@6.3.1` peer-depends on Appium ^3.0.0-rc.2; Appium 2.19 raised a compatibility warning. Appium 3.8.0 (stable, `latest`) matches. Still "Appium" per PRD.
- **Windows Developer Mode** was enabled (registry `HKLM\...\AppModelUnlock\AllowDevelopmentWithoutDevLicense = 1`); WinAppDriver exits 0x80004005 without it.
- Python was installed user-scope, no elevation.
- Smoke results: `WinAppDriver.exe 4729` → LISTENING (verified); WAD requires the port as a CLI arg when non-default.
- WAD speaks legacy JSONWP (top-level `sessionId`), enough that its own Root session CAN enumerate desktop windows spawned from the agent shell; the Appium windows driver handles protocol conversion for us.

## Ports

- Appium server: `4723`
- Direct WinAppDriver fallback: `4729`
