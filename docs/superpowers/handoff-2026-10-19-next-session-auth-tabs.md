# Session Handoff — Implement the Auth + Tabs + Dashboards Plan (native, next session)

> Read AGENTS.md first (repo conventions are normative). This handoff is the bridge into the
> next session; it carries everything the grilling session settled so no decision is re-litigated.

## What this session is

Implement, end-to-end and locally, the plan at
**`docs/superpowers/plans/2026-10-19-iran-signal-tracker-auth-tabs-dashboards.md`** — the
executed plan document is authoritative; this handoff only adds session context.

- Execution method chosen by the owner: **native inline implementation** → REQUIRED SUB-SKILL
  `superpowers:executing-plans`. TDD is mandatory: every C++ change starts as a RED
  db_test/smoke slot; every E2E change starts as a RED pytest file. No skipping the RED run.
- Update the SDD ledger `.superpowers/sdd/2026-10-19-iran-signal-tracker-e2e/progress.md`
  (git-ignored) after each task: ruling lines + completion lines (see its current format).

## Grilling outcome (owner confirmed verbatim — do NOT re-decide)

- **Scope cut**: register/login + tabbed app only. NO dark theme, NO PNG export this round.
- **Register-first UX**: a *fresh user must register first and then log in* to see the app.
  Blocking dialog before the main window. NO roles column, plaintext passwords (demo-grade).
- **Register validation, ONLY**: password must be non-empty, username must not already exist.
- **One dialog** toggling Login ⇄ Register in place (ids pinned in the plan header).
- **Users tab** (list + remove user) is visible only when username == `admin` (literal special
  case, no roles). Admin can remove any user *except itself*.
- **Post-login main window**: Home tab = routing cards (`homeCardNetwork|Dashboard|Reports`,
  `homeCardUsers` for admin) + top bar `currentUserLabel` = `user: <name>` + `logoutButton`.
  All-row dumps (10080 rows) do NOT belong on Home — Network Data lives in its own tab.
- **Dataset**: 7-day deterministic era — owner approved "7 × 1500/day (~10.5k)"; plan pins
  **10080 = 1440/day × 7 exactly** (clean 7 trend buckets — same intent).
- **Test isolation** (owner requirement): restore the committed `database/signaltracker.db`
  from a session-start temp snapshot BEFORE EVERY test + clear `reports/export/*.csv` so
  sequential tests cannot observe each other; git tree must stay clean across runs.
- **DSL contract change (owner emphasized: "that's the whole point")**: actions perform,
  readers return, tests assert. `tracker.login(u,p)` performs; `tracker.signin_message()`
  returns the inline label; the TEST asserts equality with the pinned copy. No
  `login_should_fail`-style action+assertion hybrids.
- All 10 existing E2E tests are rewritten (auth precondition + `.network()` prefix); new
  TC011–TC015 per the plan's catalog; `docs/test-cases/` updated in-kind; 15 total green.

## Critical environment facts (from the previous session — trust these)

- Quirks baked into the product are in AGENTS.md; the newest three (keep in mind):
  1. **Every launch must force the app to the OS foreground** — WAD keystrokes go to the
     focused window and clicks to whatever sits at the element's screen coordinates. Without
     `drivers/foreground.py` enforcement, automation typed into the operator's chat window
     and clicked other apps (owner observed). `launch()` hard-fails if it can't acquire focus.
  2. **Combos select via keyboard type-ahead** (`el.send_keys(value)`, no ENTER — it would
     re-open the popup). Popup-item/table-cell Name ambiguity makes element-click selection
     non-deterministic.
  3. **Chart contract**: persistent axes; categories set BEFORE attach (attached empty axis
     auto-generates "1..n" slots); `@value` label tag (a `"%.1f"` format renders literally
     and gets dropped); explicit black `labelBrush` (theme picks near-white for blue bars);
     bars encode `(avg dBm + 120)/0.7` = 0..100 quality because the bar canvas draws bars
     from the implied zero line (invisible in a negative-only dBm domain); splitter 380/260.
- Three seeded users exist in the committed DB **after Task 1 lands** — until then only WAD
  quirks above apply.
- WinAppDriver direct fallback: `DRIVER_MODE=direct` (port 4729); Appium 3.8.0 on 4723 with
  the `appium-windows-driver`; all pinned in `docs/env-setup.md`.
- Python: `& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe ...` (store stub shadows
  PATH in fresh shells).

## Repo state at handoff

- Branch `main`, clean tree, HEAD `b161255` ("docs: record chart quality-scale contract …").
- ctest 2/2 (`app_smoke` incl. chart-value-label pins, `app_dbtest`) and pytest **10/10 green**
  (TC001–TC010) as of the last run — the baseline the plan's Task 8 builds on.
- Allure results/report present under `reports/` (regenerate after Task 8's fresh run).
- The Appium background server may or may not still be alive; `scripts\run-appium.ps1` to
  restart it for pytest runs.

## Suggested session flow

1. Invoke `superpowers:executing-plans` skill; work through the plan's Tasks 1–9 in order.
2. Housekeeping per task: kill app processes before rebuilds, run the FULL relevant suite
   (`run-ctest.ps1`; all of `automation/tests`) before claiming a task done, commit
   incrementally with conventional messages.
3. Final reviewer subagent on the whole-branch diff; fix pass; push `origin main` (owner
   approved push in the original plan).
4. Report back: 15/15 pytest, ctest X/X, Allure regenerated, ledger updated.
