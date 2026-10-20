"""Fixture store: named .db fixtures are the single source of truth.

`restore(fixture)` copies `database/fixtures/<fixture>.db` over the
DISPOSABLE working database (`database/signaltracker.db`, git-ignored)
and then migrates THE COPY with the numbered scripts in
`database/migrations/` (applied when `PRAGMA user_version` < the file's
number). Fixtures themselves are NEVER mutated - restore is a copy plus
a migration of the copy, per spec 2026-10-20 (owner directive: every
test restores and migrates unconditionally; no stale-db continuation).

Stdlib only (sqlite3/shutil/json/re). CLI:
    python automation\\fixtures.py [fixture]   # default = map's "default"
"""
from __future__ import annotations

import json
import re
import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
FIXTURES_DIR = REPO_ROOT / "database" / "fixtures"
WORKING_DB = REPO_ROOT / "database" / "signaltracker.db"
MIGRATIONS_DIR = REPO_ROOT / "database" / "migrations"
MAP_PATH = REPO_ROOT / "database" / "fixtures.json"

_MIGRATION_RE = re.compile(r"^(\d+)_.+\.sql$")


def load_map(path: Path = MAP_PATH) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def select_fixture(nodeid: str, mapping: dict) -> str:
    """Exact nodeid override wins; anything unmapped uses the default."""
    if nodeid in mapping:
        return mapping[nodeid]
    if "default" not in mapping:
        raise KeyError("no default fixture in map")
    return mapping["default"]


def migration_files(directory: Path = MIGRATIONS_DIR) -> list[tuple[int, Path]]:
    """`<number>_<name>.sql` files, sorted by number; others ignored."""
    found: list[tuple[int, Path]] = []
    if not directory.is_dir():
        return found
    for path in directory.iterdir():
        m = _MIGRATION_RE.match(path.name)
        if m:
            found.append((int(m.group(1)), path))
    return sorted(found)


def migrate(db_path: Path, directory: Path = MIGRATIONS_DIR) -> int:
    """Apply pending migrations to the WORKING COPY; return count applied."""
    conn = sqlite3.connect(db_path)
    try:
        version = conn.execute("PRAGMA user_version").fetchone()[0]
        applied = 0
        for number, path in migration_files(directory):
            if number <= version:
                continue
            conn.executescript(path.read_text(encoding="utf-8"))
            # number comes from the validated filename regex, never input.
            conn.execute(f"PRAGMA user_version = {number}")
            conn.commit()
            applied += 1
        return applied
    finally:
        conn.close()


def _kill_app() -> None:
    """Best effort: the app must not hold the working db open during copy."""
    subprocess.run(["taskkill", "/F", "/IM", "IranSignalTracker.exe"],
                   capture_output=True)


def restore(fixture: str, *, fixtures_dir: Path = FIXTURES_DIR,
            working_db: Path = WORKING_DB,
            migrations_dir: Path = MIGRATIONS_DIR) -> None:
    src = fixtures_dir / f"{fixture}.db"
    if not src.is_file():
        raise FileNotFoundError(
            f"unknown fixture {fixture!r}: no such file {src}")
    _kill_app()
    working_db.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, working_db)
    migrate(working_db, migrations_dir)


def main(argv: list[str]) -> int:
    fixture = argv[0] if argv else load_map()["default"]
    restore(fixture)
    print(f"restored {fixture} -> {WORKING_DB}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
