# Environment Setup — exact versions and paths

Stack provisioned 2026-10-19 by the coding agent (see plan Task 1).

| Component | Version | Path |
|---|---|---|
| Python | 3.12.8 (user-scope) | `C:\Users\user\AppData\Local\Programs\Python\Python312\python.exe` |
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

- **Appium 3, not 2.** Current `appium-windows-driver@6.3.1` peer-depends on Appium ^3.0.0-rc.2; Appium 2.19 raised a compatibility warning. Appium 3.8.0 (stable, `latest`) matches. Still "Appium" per PRD.
- **Windows Developer Mode** was enabled (registry `HKLM\...\AppModelUnlock\AllowDevelopmentWithoutDevLicense = 1`); WinAppDriver exits 0x80004005 without it.
- Python was installed user-scope, no elevation.
- Smoke results: `WinAppDriver.exe 4729` → LISTENING (verified); WAD requires the port as a CLI arg when non-default.

## Ports

- Appium server: `4723`
- Direct WinAppDriver fallback: `4729`
