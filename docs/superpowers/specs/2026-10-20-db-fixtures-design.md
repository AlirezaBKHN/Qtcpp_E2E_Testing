# DB Fixture System — Design (2026-10-20)

Status: draft for builder check. Owner directives are marked **[owner]**.

## Intent and why

Today the repo has exactly one dataset: the committed `database/signaltracker.db`.
It is simultaneously (a) the app's default store, (b) the per-test E2E restore
source, and (c) a file whose bytes must never change — a fragile tripod. Two
pressures break it:

1. **Different clients want to test against their own databases.** There is no
   way to drop a client DB into the stack and run the suite against it.
2. **Migrations.** Once the schema evolves, an old client DB must be brought up
   to the current schema *by the test stack*, so the app can be tested against
   realistic data that predates the current code.

The fix is a deliberately small fixture system — the kind modern E2E stacks
have, without any test-management machinery: named `.db` files in the repo, a
JSON map from test to fixture, and a restore step that materializes the working
database from a fixture and applies pending migrations. **[owner]** Fixtures are
plain `.db` files committed in the repo. **[owner]** The fixtures folder becomes
the *only* source of truth; the working database is a disposable git-ignored
scratch copy.

## Goals

- G1: `database/fixtures/` holds every dataset as a committed `.db` file. First
  two: `seed_full.db` (today's 10080-row deterministic dataset, renamed from the
  committed db) and `empty.db` (schema, zero rows, seeded users).
- G2: `database/signaltracker.db` becomes a git-ignored working copy. It is
  materialized by the restore step and nobody ever commits it again.
- G3: A JSON map (`database/fixtures.json`) assigns each test a fixture, with a
  default for anything unmapped.
- G4: A restore step: copy fixture → working path, then apply any pending
  migrations to the *copy*. Fixtures themselves are never mutated — a fixture's
  bytes stay exactly as committed.
- G5: A migrations folder (`database/migrations/`, numbered SQL scripts) with a
  version marker in each DB (`PRAGMA user_version`), applied idempotently.
  Folder starts empty (the baseline schema is born in `Db::init`); the
  mechanism exists for the first real migration.
- G6: The C++ app is untouched: it still opens
  `applicationDirPath()/../../database/signaltracker.db`. The whole system is
  scaffolding *around* that path.

## Non-goals

- No test-management system, no fixture registry UI, no per-fixture metadata
  beyond the filename.
- No C++ smoke/db_test changes — they already use their own scratch DBs and
  never touch the working path's contents meaningfully.
- No migration tooling inside the app at runtime: `Db::init` keeps creating the
  current schema; migrations are a test-stack concern (the restore step), not a
  product feature. If the product later needs runtime migration, that is a new
  design.
- No fixture generation framework: generating a fixture = running `app_dbgen`
  (or a documented one-off SQL command for `empty.db`) and committing the file.

## Constraints

- C1: The seeder is deterministic except for its date window (7 days ending
  "today"). A fixture is regenerated **deliberately** (owner runs `app_dbgen`,
  reviews, commits once); tests never regenerate. Era drift between fixtures is
  acceptable and visible in the commit.
- C2: The committed-DB byte-clean ritual (AGENTS.md) is **replaced**, not
  dropped: byte-cleanliness moves to `database/fixtures/` (never mutated), while
  `database/signaltracker.db` may be dirtied freely by any run.
- C3: Windows/MinGW/Qt 6.8.3 stack; scripts stay PowerShell; migration logic
  lives in Python (`sqlite3` is stdlib, PowerShell has no SQLite driver).
- C4: Per-test isolation semantics are preserved: every E2E test starts from
  its mapped fixture, restored fresh (same guarantee as today's `isolate_db`).

## The parts the owner decided

- **[owner]** Fixtures are plain `.db` files, committed.
- **[owner]** Option 1 — fixtures folder is the only source of truth; the
  working db is a disposable scratch copy that the restore step materializes.
- **[owner]** The system is "a simple fixture system used in modern e2e
  testing, without a whole test management system".

## Left to the builder (my calls unless the owner objects)

- **Map location and shape**: `database/fixtures.json`:
  `{"default": "seed_full", "<pytest nodeid>": "<fixture-name>"}`. Keys are
  full nodeids (`tests/test_tc016_pagination.py::test_tc016_page_navigation`);
  anything unmapped uses `default`. Fixture name = filename stem in
  `database/fixtures/`.
- **Restore implementation**: one Python module, `automation/fixtures.py`,
  exposing `restore(fixture: str)`: kill lingering `IranSignalTracker.exe`,
  copy `database/fixtures/<name>.db` over `database/signaltracker.db`, open the
  copy with `sqlite3`, apply `database/migrations/*.sql` in filename order
  while `user_version < N`, set `user_version`, close. Consumed by:
  - the `isolate_db` pytest fixture (replaces its `git checkout --` body),
  - `scripts/restore-fixture.ps1 -Fixture <name>` — thin wrapper for humans/CI
    ("restore + migrate, then I run tests myself").
- **`empty.db`**: created once via a documented `sqlite3`/python command
  (schema through `app_dbgen` then `DELETE FROM cells` + `VACUUM`), then
  committed. No generation script kept.
- **`app_dbgen`**: keeps accepting an output path; docs/scripts updated to
  regenerate `database/fixtures/seed_full.db`. A stamp of the latest migration
  version on freshly generated DBs is only needed once the migrations folder is
  non-empty — builder may add it then (one PRAGMA write) or leave it to the
  restore step (which migrates the copy anyway). Either is fine; prefer the
  restore-step-only variant (zero app changes).
- **Repo surgery**: `git rm --cached database/signaltracker.db` + `.gitignore`
  entry; AGENTS.md sections rewritten (commands, DB-isolation convention,
  app_dbgen regeneration target); README/test-cases docs touched in the same
  change.
- **Sequencing**: this is a separate deliverable from the in-flight pagination
  plan. Pagination (Tasks 2–4) finishes first on the current ritual; then this
  lands as its own TDD plan (pytest tests for `fixtures.py`: copy semantics,
  migration ordering, idempotency, unknown-fixture error; smoke untouched).

## Builder check (2026-10-20) — resolutions

Builder-verified facts (now assumed by this design): `IST_DB_PATH` already
overrides the working path in `automation/config.py`; the app has no
date-relative query logic outside the seeder/export filenames, so fixture era
drift cannot break assertions; TC014 is the only E2E test pinning exact dataset
size (`'10080 cells'`).

1. **Landing map scope (my call):** the landing `fixtures.json` is
   `{"default": "seed_full"}` plus exactly ONE override — a new E2E test
   (empty-dataset contract: empty table, zeroed KPIs, pager "Page 1 of 1")
   mapped to `empty`. That proves the mechanism end-to-end at landing without
   auditing all 15 existing tests for hidden data dependence; remapping more
   tests is follow-up work, done one at a time.
2. **First launch after surgery (my call):** `run-app.ps1` and
   `run-tests.ps1` materialize the default fixture when
   `database/signaltracker.db` is absent, so no script ever causes a silent
   self-seeded working DB. `Db::init`'s self-seed stays as the product's
   safety net but is documented as "not how the working DB should appear".
3. **Teardown restore [owner]:** dropped. Per-test restore happens before each
   test only; the last test leaves the working db as-is (git-ignored,
   disposable). `db_snapshot` (session-scoped committed-DB ritual) is deleted;
   AGENTS.md's byte-clean requirement moves to `database/fixtures/` only.

## Success check

1. `scripts/restore-fixture.ps1 -Fixture empty` → working db exists with 0
   cells, 3 users; app launches and shows an empty table.
2. A fake migration script in `database/migrations/` is applied exactly once to
   the working copy; the fixture file's bytes are unchanged (hash before/after).
3. Full E2E suite green with `isolate_db` sourcing fixtures via the JSON map;
   `git status` never lists `database/signaltracker.db` again.
