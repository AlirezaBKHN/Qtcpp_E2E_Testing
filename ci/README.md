# CI/CD Flow (documentation only)

Future pipeline shape — no pipeline code ships in this demo repo.

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
