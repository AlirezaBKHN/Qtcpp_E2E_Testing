# CI/CD Flow

Pipeline code now ships (design 2026-10-21, grilled and confirmed with the owner):

- `.github/workflows/ci.yml` — PRs to `main`: build + native ctest gate.
- `.github/workflows/e2e-nightly.yml` — nightly cron + manual: full E2E suite
  (TC019 deselected) with `DRIVER_MODE=direct` on a hosted Windows runner.
- `.github/workflows/tags.yml` — pushed tags: native + full E2E, artifacts
  only (demo app, no release publishing).

Runner prereqs codified in the workflows: Qt 6.8.3 win64_mingw + MinGW 13.1.0
(install-qt-action, wired via `IST_*`), WinAppDriver 1.2.1 MSI + Developer
Mode registry key, WinAppDriver started on :4729 before pytest (conftest's
`stack_available` probes the port before the factory can start it).

The historical sketch below predates the implementation.

```text
Commit
   ↓
Build App            (cmake configure + build of app/, ctest smoke + db_test gate)
   ↓
Launch Appium        (appium server on :4723 with the windows driver)
   ↓
Launch Application   (driver factory starts IranSignalTracker.exe via caps)
   ↓
Execute Tests        (python -m pytest automation/tests)
   ↓
Generate Allure      (allure generate reports/allure-results -o reports/allure-report)
   ↓
Publish Report       (allure open / host allure-report artefact + allure-results per build)
```

CI notes for the future pipeline author:

- Cache `C:\Qt`, MinGW, Node and Python dirs — restore by directory hash of the version pins in `docs/env-setup.md`.
- Runner needs Windows + Developer Mode enabled + WinAppDriver installed (see `docs/env-setup.md`).
- Screenshots on failure land in `reports/allure-results` and are attached by the pytest fixture.
- Failure convention: any failing test fails the job; still always publish the Allure artefacts (`if: always()`).
