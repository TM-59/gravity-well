# Copyright 2026 TM-59
"""Local initial-download gate; no third-party packages. Python 3.14."""

import sys
from pathlib import Path

LIMIT_BYTES = 10_000_000


class DownloadTooLargeError(SystemExit):
    """The files in dist/ add up to more than the initial-download limit."""

    def __init__(self) -> None:
        """Exit with the limit in the message."""
        super().__init__(f"Initial download exceeds {LIMIT_BYTES:,} B")


files = sorted(p for p in Path("dist").rglob("*") if p.is_file())
total = sum(p.stat().st_size for p in files)
for p in files:
    sys.stdout.write(f"{p}: {p.stat().st_size:,} B\n")
sys.stdout.write(f"Initial download: {total:,} B / {LIMIT_BYTES:,} B\n")
if total > LIMIT_BYTES:
    raise DownloadTooLargeError
