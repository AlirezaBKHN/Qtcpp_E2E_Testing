"""Export result page object: verify a new CSV file was created."""
from __future__ import annotations

from pathlib import Path


class ExportResult:
    """Wraps the export outcome: `should_create_file()` asserts that a new
    CSV appeared in the export directory after the export click."""

    def __init__(self, export_dir: Path, files_before: set[str]):
        self._dir = export_dir
        self._before = files_before

    def _current(self) -> set[str]:
        self._dir.mkdir(parents=True, exist_ok=True)
        return {p.name for p in self._dir.glob("export_*.csv")}

    def should_create_file(self) -> "ExportResult":
        new_files = self._current() - self._before
        assert new_files, (
            f"no new export_*.csv appeared in {self._dir}; "
            f"files before: {sorted(self._before)}")
        return self

    def newest_file(self) -> Path | None:
        new_files = self._current() - self._before
        if not new_files:
            return None
        files = [p for p in self._dir.glob("export_*.csv") if p.name in new_files]
        return max(files, key=lambda p: p.stat().st_mtime) if files else None
