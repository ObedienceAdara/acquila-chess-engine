#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make -j >/dev/null
INPUT=$(cat <<'EOF'
position startpos
perft 4
position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
perft 4
position fen 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1
perft 4
position fen r1bq2r1/1pppkppp/1b3n2/pP1PP3/2n5/2P5/P3QPPP/RNB1K2R w KQ a6 0 12
perft 4
position fen 2b1b3/1r1P4/3K3p/1p6/2p5/6k1/1P3p2/4B3 w - - 0 42
perft 5
quit
EOF
)
out=$(printf '%s\n' "$INPUT" | ./aquila)
printf '%s\n' "$out"
grep -q 'perft 4 nodes 197281' <<<"$out"
grep -q 'perft 4 nodes 4085603' <<<"$out"
grep -q 'perft 4 nodes 43238' <<<"$out"
grep -q 'perft 4 nodes 1280017' <<<"$out"
grep -q 'perft 5 nodes 5617302' <<<"$out"
