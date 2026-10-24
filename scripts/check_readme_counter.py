#!/usr/bin/env python3
"""Keep the README's first JTML example identical to the quickstart source."""
from pathlib import Path
import re
import sys


def main(root: Path) -> int:
    readme = (root / "README.md").read_text(encoding="utf-8")
    source = (root / "examples/friendly_counter.jtml").read_text(encoding="utf-8")
    snippet = re.search(r"^```jtml\n(.*?)^```$", readme, re.MULTILINE | re.DOTALL)
    if snippet is None or snippet.group(1) != source:
        print("README counter differs from examples/friendly_counter.jtml; copy the complete source into the first JTML block.", file=sys.stderr)
        return 1
    print("README counter matches examples/friendly_counter.jtml.")
    return 0


if __name__ == "__main__":
    root = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parents[1]
    raise SystemExit(main(root))
