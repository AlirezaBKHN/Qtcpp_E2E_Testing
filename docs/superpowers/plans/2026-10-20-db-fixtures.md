# DB Fixture System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single committed `database/signaltracker.db` with a committed fixture folder as the only source of truth, a per-test restore+migrate step, and a JSON test→fixture map.

**Architecture:** `automation/fixtures.py` (Python stdlib only) copies a named fixture over the disposable working db and applies `database/migrations/*.sql` to the COPY, guarded by `PRAGMA user_version`. The pytest `isolate_db` fixture restores the mapped fixture before every test (setup only — teardown restore dropped per owner). The app is untouched: it keeps opening `applicationDirPath()/../../database/signaltracker.db`.

**Tech Stack:** Python 3.12 stdlib (`sqlite3`, `shutil`, `json`, `re`), pytest, PowerShell scripts, Qt app unchanged.

**Spec:** `docs/superpowers/specs/2026-10-20-db-fixtures-design.md` (owner directives marked **[owner]** there; the plan argues from it).

## Global Constraints

- **[owner]** Every single test restores **and migrates** its fixture — never continues on a stale db. No restore-if-changed, no session-level reuse, no migrate-once-per-run: the restore+copy+migrate path runs per test, unconditionally (spec C5).
- Fixtures are plain `.db` files **committed in the repo**; the fixtures folder is the ONLY source of truth; `database/signaltracker.db` becomes git-ignored and disposable (spec G1/G2, owner option 1).
- Fixtures are NEVER mutated by any run — restore always copies then migrates the copy (spec G4).
- `automation/fixtures.py` uses Python stdlib only (no new dependencies); PowerShell scripts stay `.ps1`.
- The C++ app is untouched (spec G6): no `app/src` changes in this plan. `Db::init`'s self-seed stays as the product safety net but scripts must materialize the default fixture instead of letting a self-seeded working db appear (spec builder-check 3).
- Teardown restore is DROPPED (owner ruling 2026-10-20): restore before each test only; the `db_snapshot` session fixture is deleted.
- TDD throughout: watch each new test fail before implementing.

## Review Focus

1. **A run mutates a fixture file** (e.g. opens it in place and migrates it) — a reasonable person expects fixture bytes identical before/after any test run. → Pinned by Task 1's `test_restore_fixture_bytes_unchanged` (sha256 before/after `restore`).
2. **A migration applied twice or out of order** corrupts schema or data — expect `user_version` to gate application exactly once, in filename order. → Pinned by Task 1's `test_migrate_applies_in_order_sets_version` + `test_migrate_idempotent`.
3. **Unknown fixture name or missing `default` in the map** silently falls back to some dataset — expect a loud `FileNotFoundError`/`KeyError` naming the culprit. → Pinned by Task 1's `test_restore_unknown_fixture_raises` + `test_select_fixture_requires_default`.
4. **A crashed run leaves a stale/corrupt working db** — the NEXT test's restore must overwrite it wholesale. → Pinned by Task 3's verification step: write junk bytes into the working db, run TC001, expect PASS.
5. **`empty.db` wipes users along with cells** (a naive `rm` of rows) — sign-in must still work with 0 cells. → Pinned by Task 4's TC018 (logged-in session on `empty`, `Page 1 of 1`, `Showing 0 rows`).

---

### Task 1: `automation/fixtures.py` + unit tests

**Files:**
- Create: `automation/fixtures.py`
- Test: `automation/tests/test_fixtures_unit.py`

**Interfaces:**
- Consumes: nothing (stdlib only).
- Produces (exact signatures later tasks rely on):
  - `REPO_ROOT: Path`, `FIXTURES_DIR: Path` (= `REPO_ROOT/database/fixtures`), `WORKING_DB: Path` (= `REPO_ROOT/database/signaltracker.db`), `MIGRATIONS_DIR: Path` (= `REPO_ROOT/database/migrations`), `MAP_PATH: Path` (= `REPO_ROOT/database/fixtures.json`)
  - `load_map(path: Path = MAP_PATH) -> dict`
  - `select_fixture(nodeid: str, mapping: dict) -> str` — exact nodeid key wins, else `mapping["default"]`; missing default raises `KeyError("no default fixture in map")`
  - `migration_files(directory: Path = MIGRATIONS_DIR) -> list[tuple[int, Path]]` — files matching `^(\d+)_.+\.sql$`, sorted by number; others ignored
  - `migrate(db_path: Path, directory: Path = MIGRATIONS_DIR) -> int` — applies files with `number > current user_version` in order via `executescript`, then `PRAGMA user_version = <number>`; returns count applied
  - `restore(fixture: str, *, fixtures_dir: Path = FIXTURES_DIR, working_db: Path = WORKING_DB, migrations_dir: Path = MIGRATIONS_DIR) -> None` — kill `IranSignalTracker.exe` (best-effort `taskkill /F /IM`, captured output), `shutil.copyfile(fixture_db, working_db)`, `migrate(working_db)`; unknown fixture → `FileNotFoundError` naming it
  - `main(argv: list[str]) -> int` — CLI: optional fixture name arg, else `load_map()["default"]`; calls `restore`; used by scripts (`python automation\fixtures.py` and the `restore-fixture.ps1` wrapper)

- [ ] **Step 1: Write the failing unit tests** in `automation/tests/test_fixtures_unit.py` (plain pytest — requests no appium/session fixtures; every test builds its own tmp dirs and passes them as explicit parameters, so nothing touches the real `database/`):

```python
# automation/tests/test_fixtures_unit.py
# Unit tests for the fixture system (stdlib fixture store; no appium).
# Every test works in tmp dirs - the real database/ is never touched.
import hashlib
import sqlite3

import pytest

import fixtures


def _make_db(path, cells=5, users=("admin", "guest")):
    conn = sqlite3.connect(path)
    conn.execute("CREATE TABLE cells (id INTEGER PRIMARY KEY, v TEXT)")
    conn.executemany("INSERT INTO cells (v) VALUES (?)", [(f"c{i}",) for i in range(cells)])
    conn.execute("CREATE TABLE users (username TEXT PRIMARY KEY)")
    conn.executemany("INSERT INTO users (username) VALUES (?)", [(u,) for u in users])
    conn.commit()
    conn.close()


def _sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def test_restore_copies_fixture_and_leaves_it_unchanged(tmp_path):
    fx, work, mig = tmp_path / "fx", tmp_path / "work", tmp_path / "mig"
    fx.mkdir(); mig.mkdir()
    src = fx / "seed_full.db"
    _make_db(src, cells=5)
    before = _sha(src)
    fixtures.restore("seed_full", fixtures_dir=fx, working_db=work / "signaltracker.db",
                     migrations_dir=mig)
    assert _sha(src) == before                      # fixture never mutated
    assert _sha(work / "signaltracker.db") == before
    assert sqlite3.connect(work / "signaltracker.db") \
        .execute("SELECT count(*) FROM cells").fetchone()[0] == 5


def test_restore_unknown_fixture_raises(tmp_path):
    fx, work, mig = tmp_path / "fx", tmp_path / "work", tmp_path / "mig"
    fx.mkdir(); mig.mkdir()
    with pytest.raises(FileNotFoundError, match="no_such"):
        fixtures.restore("no_such", fixtures_dir=fx, working_db=work, migrations_dir=mig)


def test_migrate_applies_in_order_and_sets_version(tmp_path):
    work, mig = tmp_path / "work", tmp_path / "mig"
    work.parent.mkdir(parents=True, exist_ok=True); mig.mkdir()
    _make_db(work)
    (mig / "010_add_rating.sql").write_text(
        "ALTER TABLE cells ADD COLUMN rating INTEGER DEFAULT 0;", encoding="utf-8")
    (mig / "020_add_notes.sql").write_text(
        "ALTER TABLE users ADD COLUMN notes TEXT;", encoding="utf-8")
    (mig / "readme.txt").write_text("ignored", encoding="utf-8")  # non-matching
    applied = fixtures.migrate(work, mig)
    assert applied == 2
    conn = sqlite3.connect(work)
    assert conn.execute("PRAGMA user_version").fetchone()[0] == 20
    conn.execute("SELECT rating FROM cells").fetchone()   # schema present
    conn.execute("SELECT notes FROM users").fetchone()
    conn.close()


def test_migrate_idempotent_and_respects_existing_version(tmp_path):
    work, mig = tmp_path / "work", tmp_path / "mig"
    mig.mkdir()
    _make_db(work)
    (mig / "010_a.sql").write_text("ALTER TABLE cells ADD COLUMN a INTEGER DEFAULT 0;",
                                   encoding="utf-8")
    conn = sqlite3.connect(work)
    conn.execute("ALTER TABLE cells ADD COLUMN already INTEGER DEFAULT 1;")
    conn.execute("PRAGMA user_version = 10")   # fixture born at v10
    conn.commit(); conn.close()
    assert fixtures.migrate(work, mig) == 0    # 010 <= version: skipped
    assert fixtures.migrate(work, mig) == 0    # second run: still nothing


def test_select_fixture_nodeid_wins_default_falls_back(tmp_path):
    mapping = {"default": "seed_full",
               "tests/test_tc018_empty_fixture.py::test_tc018_empty_fixture": "empty"}
    assert fixtures.select_fixture(
        "tests/test_tc018_empty_fixture.py::test_tc018_empty_fixture", mapping) == "empty"
    assert fixtures.select_fixture("tests/test_tc001_launch.py::test_tc001_launch",
                                   mapping) == "seed_full"
    with pytest.raises(KeyError, match="default"):
        fixtures.select_fixture("anything", {})
```

- [ ] **Step 2: RED** — `& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe -m pytest automation\tests\test_fixtures_unit.py -v` from repo root. Expected: collection ERROR — `ModuleNotFoundError: No module named 'fixtures'` (automation/ is on sys.path via rootdir; if the import path differs, run from `automation\` with `tests\test_fixtures_unit.py`).

- [ ] **Step 3: Implement `automation/fixtures.py`** — module docstring states the contract ("restore copies a fixture over the disposable working db, then migrates the COPY; fixtures are never mutated"), then the constants, `load_map`, `select_fixture`, `migration_files`, `migrate`, `restore`, `ensure_working_db`, `main` per the signatures above. Body notes:
  - `_kill_app()`: `subprocess.run(["taskkill", "/F", "/IM", "IranSignalTracker.exe"], capture_output=True)` — best effort, return value ignored (the app must not hold the working db open during copy).
  - `migrate`: version read via `PRAGMA user_version`; `conn.executescript(sql_text)`; then `conn.execute(f"PRAGMA user_version = {num}")` (number validated by the filename regex, never user input); `conn.commit()` per applied file.
  - `restore`: `working_db.parent.mkdir(parents=True, exist_ok=True)` before the copy.
  - `main`: prints `restored <name> -> <working_db>` on success, returns 0; missing arg → `load_map()["default"]`. No `ensure_working_db` helper — the scripts do their own `Test-Path` guard; the CLI always restores explicitly.

- [ ] **Step 4: GREEN** — same command. Expected: 5 passed.

- [ ] **Step 5: Commit**

```bash
git add automation/fixtures.py automation/tests/test_fixtures_unit.py
git commit -m "feat(automation): fixture store - restore copies fixture, migrates the copy (user_version-gated)"
```

---

### Task 2: Fixture artifacts + repo surgery

**Files:**
- Create: `database/fixtures/seed_full.db` (byte copy of the current committed `database/signaltracker.db` — 10080 rows, users admin/e2e_user/guest)
- Create: `database/fixtures/empty.db` (same schema + seeded users, ZERO cells)
- Create: `database/migrations/README.md` (one line: "Numbered SQL migrations `<number>_<name>.sql`, applied by automation/fixtures.py to the working copy when `PRAGMA user_version` < `<number>`. Folder starts empty: the baseline schema is born in Db::init.")
- Create: `database/fixtures.json` — exactly `{"default": "seed_full"}`
- Create: `scripts/restore-fixture.ps1` (thin human/CI wrapper per the spec's success check: `param([string]$Fixture = '')` then `& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe (Join-Path $PSScriptRoot '..\automation\fixtures.py') $Fixture`)
- Modify: `.gitignore:23-27` (append `database/signaltracker.db` under the scratch section)
- Remove from index: `database/signaltracker.db` (file stays on disk locally)

**Interfaces:**
- Consumes: Task 1's `restore`/`migrate` (not called here — this task only produces artifacts).
- Produces: `database/fixtures/seed_full.db` (10080 cells, 3 users, `PRAGMA user_version` 0), `database/fixtures/empty.db` (0 cells, 3 users), `database/fixtures.json` with `default`.

- [ ] **Step 1: Create the artifacts** (kill the app first):

```powershell
taskkill /F /IM IranSignalTracker.exe 2>$null
New-Item -ItemType Directory -Force database\fixtures, database\migrations | Out-Null
Copy-Item database\signaltracker.db database\fixtures\seed_full.db
Copy-Item database\signaltracker.db database\fixtures\empty.db
& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe -c "import sqlite3; c = sqlite3.connect(r'database\fixtures\empty.db'); c.execute('DELETE FROM cells'); c.commit(); c.execute('VACUUM'); c.close()"
```

Then write `database/migrations/README.md` and `database/fixtures.json` (`{"default": "seed_full"}` exactly, no nodeid overrides yet).

- [ ] **Step 2: Verify the artifacts** — expect `seed_full` = (10080, 3), `empty` = (0, 3), both `user_version` 0:

```powershell
& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe -c "import sqlite3; [print(p, sqlite3.connect(p).execute('SELECT (SELECT count(*) FROM cells), (SELECT count(*) FROM users), user_version FROM cells LIMIT 1').fetchone()) for p in (r'database\fixtures\seed_full.db', r'database\fixtures\empty.db')]"
```

- [ ] **Step 3: Repo surgery** — append `database/signaltracker.db` to `.gitignore` (same section as the scratch dbs), then `git rm --cached database/signaltracker.db`. The working file stays on disk (now ignored).

- [ ] **Step 4: Create the wrapper + commit** — write `scripts/restore-fixture.ps1` per the Files block, then:

```bash
git add .gitignore database/fixtures database/migrations database/fixtures.json scripts/restore-fixture.ps1
git commit -m "feat(db): fixture folder as single source of truth (seed_full + empty); working db becomes disposable"
```

---

### Task 3: `isolate_db` rewiring (per-test restore+migrate, no teardown)

**Files:**
- Modify: `automation/conftest.py:40-72` (delete `db_snapshot`; rewrite `isolate_db`)
- Modify: `automation/config.py:49-51` (comment rewrite: `DB_PATH` is the disposable working db the fixture system materializes)
- Modify: `scripts/run-tests.ps1` (materialize default fixture when the working db is absent, before pytest)
- Modify: `scripts/run-app.ps1` (same materialize step before `Start-Process`)

**Interfaces:**
- Consumes: Task 1's `restore(fixture, ...)`, `load_map()`, `select_fixture(nodeid, mapping)`; Task 2's `database/fixtures.json` + fixtures. `cfg.DB_PATH` (config.py:50, honors `IST_DB_PATH`) is passed as `working_db`.
- Produces: `isolate_db(request)` — pytest fixture, setup-only; every E2E test's DB guarantee.

- [ ] **Step 1: Rewrite the fixtures in `automation/conftest.py`** — delete `db_snapshot` (lines 40-50) and its `shutil` import if now unused; replace `isolate_db` with:

```python
@pytest.fixture(autouse=False)
def isolate_db(request):
    """Per-test isolation (owner requirement, spec 2026-10-20 C5): restore
    AND migrate the test's mapped fixture before every test - never
    continue on a stale db. No teardown restore: the working db is
    git-ignored and disposable; the next test's setup is the restore point
    for a crashed run."""

    nodeid = request.node.nodeid
    mapping = fixtures.load_map()
    fixtures.restore(fixtures.select_fixture(nodeid, mapping),
                     working_db=cfg.DB_PATH)
    cfg.EXPORT_DIR.mkdir(parents=True, exist_ok=True)
    for csv_file in cfg.EXPORT_DIR.glob("*.csv"):
        csv_file.unlink()
    yield
```

  (add `import fixtures` next to the existing `import config as cfg`; keep the exports purge.)

- [ ] **Step 2: Materialize step in the scripts** — in both `run-tests.ps1` and `run-app.ps1`, immediately after the `$ErrorActionPreference = 'Stop'` line:

```powershell
# Fixtures are the only source of truth: materialize the working db if absent.
if (-not (Test-Path (Join-Path (Split-Path $PSScriptRoot -Parent) 'database\signaltracker.db'))) {
    & (Join-Path $PSScriptRoot 'restore-fixture.ps1')
}
```

  (no arg = the map's default; run-app.ps1 has no python resolution of its own, so reusing the wrapper keeps one resolution point.)

- [ ] **Step 3: Verify** — first the spec's success check 1 via the wrapper, then crash recovery (Review Focus 4):

```powershell
pwsh -File scripts\restore-fixture.ps1 -Fixture empty
& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe -c "import sqlite3; print(sqlite3.connect(r'database\signaltracker.db').execute('SELECT (SELECT count(*) FROM cells), (SELECT count(*) FROM users)').fetchone())"
# expect (0, 3)
Set-Content -Path database\signaltracker.db -Value "junk bytes - not a database" -Encoding Ascii
& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe -m pytest automation\tests\test_tc001_launch.py automation\tests\test_tc012_login.py -v
```

  Expected: the readout prints `(0, 3)`; then 2 passed — `isolate_db` overwrote the junk with `seed_full`. Then the full suite: `pwsh -ExecutionPolicy Bypass -File scripts\run-tests.ps1` → **17 passed**.

- [ ] **Step 4: Commit**

```bash
git add automation/conftest.py automation/config.py scripts/run-tests.ps1 scripts/run-app.ps1
git commit -m "feat(automation): per-test fixture restore+migrate via fixtures.json; db_snapshot/teardown restore dropped"
```

---

### Task 4: TC018 (empty fixture E2E) + docs + final verification + push

**Files:**
- Create: `automation/tests/test_tc018_empty_fixture.py`
- Create: `docs/test-cases/TC018.md`
- Modify: `database/fixtures.json` — add the TC018 nodeid override
- Modify: `README.md` (DB bullet + fixture-system paragraph; test list becomes 18)
- Modify: `AGENTS.md` (DB-isolation convention rewrite: fixtures folder = source of truth, working db disposable, per-test restore+migrate unconditional, no teardown, `db_snapshot` gone, `app_dbgen` target is `database\fixtures\seed_full.db`, migrations folder note, `restore-fixture` CLI note)

**Interfaces:**
- Consumes: `signed` fixture (Task 3's `isolate_db` + login), DSL `network()`, `page_label()`, `row_count()`, `table().realized_row_count()`.
- Produces: TC018 mapped to `empty`; 18-green suite.

- [ ] **Step 1: Write the failing test** (NO map entry yet — the RED proves the test detects a wrong mapping):

```python
# automation/tests/test_tc018_empty_fixture.py
# TC018 - empty fixture: the mapped database has the seeded USERS (sign-in
# works) but ZERO cells: empty table, 'Page 1 of 1', 'Showing 0 rows'.
def test_tc018_empty_fixture(signed):
    net = signed.network()
    assert net.page_label() == "Page 1 of 1"
    assert net.row_count() == 0
    assert net.table().realized_row_count() == 0
```

- [ ] **Step 2: RED** — `& $env:LOCALAPPDATA\Programs\Python\Python312\python.exe -m pytest automation\tests\test_tc018_empty_fixture.py -v` (Appium up). Expected: FAIL — `assert 51 ...` / `Page 1 of 51` and `row_count == 10080`: `isolate_db` fell back to `default` (no map entry yet), proving the test detects the mapping.

- [ ] **Step 3: Map it** — `database/fixtures.json` becomes:

```json
{
  "default": "seed_full",
  "tests/test_tc018_empty_fixture.py::test_tc018_empty_fixture": "empty"
}
```

- [ ] **Step 4: GREEN** — same command. Expected: PASS.

- [ ] **Step 5: Docs** — README: replace the `database/` bullet (line ~23 "committed signaltracker.db") and the DB bullet (line ~114 "the seeded file is also committed under `database/`") with the fixture story (fixtures folder = source of truth; working db disposable + materialized by scripts; `python automation\fixtures.py [<name>]` restores manually; migrations folder); test list heading/count → 18, append `TC018` (empty fixture). AGENTS.md: rewrite the DB-isolation ritual — committed-DB byte-clean rule now lives on `database/fixtures/` (never mutated); `database/signaltracker.db` is git-ignored and disposable; `isolate_db` restores+migrates the mapped fixture per test UNCONDITIONALLY (owner C5), no teardown; `db_snapshot` no longer exists; seeder bullet's regeneration target becomes `& app\build-mingw\app_dbgen.exe database\fixtures\seed_full.db` (regenerate deliberately, review, commit — era drift is visible in the commit); note the empty `database/migrations/` mechanism and the `IST_DB_PATH`-aware `cfg.DB_PATH`.

- [ ] **Step 6: Full verification** — `pwsh -ExecutionPolicy Bypass -File scripts\run-ctest.ps1` → 2/2 (smoke self-seeds the working db if absent — safety net; C++ tests are unaffected), then `pwsh -ExecutionPolicy Bypass -File scripts\run-tests.ps1` → **18 passed**, then `pwsh -ExecutionPolicy Bypass -File scripts\run-allure.ps1`. `git status` must never list `database/signaltracker.db` (ignored) and must list no fixture changes.

- [ ] **Step 7: Commit + push** (push pre-approved per session convention)

```bash
git add automation/tests/test_tc018_empty_fixture.py docs/test-cases/TC018.md database/fixtures.json README.md AGENTS.md
git commit -m "feat(automation): TC018 empty-fixture contract; fixture-system docs (18 green)"
git push origin main
```
