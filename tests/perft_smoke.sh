#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make -j >/dev/null
out=$(printf 'position startpos\nperft 1\nperft 2\nperft 3\nperft 4\nquit\n' | ./aquila)
printf '%s\n' "$out"
grep -q 'perft 1 nodes 20' <<<"$out"
grep -q 'perft 2 nodes 400' <<<"$out"
grep -q 'perft 3 nodes 8902' <<<"$out"
grep -q 'perft 4 nodes 197281' <<<"$out"
