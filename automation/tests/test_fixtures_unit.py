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


def test_duplicate_migration_numbers_raise(tmp_path):
    # Two files sharing a number would silently skip the second (after the
    # first sets user_version, `number <= version` gates it off) - the
    # store must reject the folder loudly instead (Review Focus 2).
    mig = tmp_path / "mig"
    mig.mkdir()
    (mig / "010_a.sql").write_text("SELECT 1;", encoding="utf-8")
    (mig / "010_b.sql").write_text("SELECT 2;", encoding="utf-8")
    with pytest.raises(ValueError, match="010"):
        fixtures.migration_files(mig)


def test_select_fixture_nodeid_wins_default_falls_back(tmp_path):
    mapping = {"default": "seed_full",
               "tests/test_tc018_empty_fixture.py::test_tc018_empty_fixture": "empty"}
    assert fixtures.select_fixture(
        "tests/test_tc018_empty_fixture.py::test_tc018_empty_fixture", mapping) == "empty"
    assert fixtures.select_fixture("tests/test_tc001_launch.py::test_tc001_launch",
                                   mapping) == "seed_full"
    with pytest.raises(KeyError, match="default"):
        fixtures.select_fixture("anything", {})
