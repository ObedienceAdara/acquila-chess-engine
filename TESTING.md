# Testing & Benchmarking

## 1. Perft correctness

Run known perft suites at increasing depths. A move generator should not be tuned for speed until node counts match reference counts.

Example:

```text
position startpos
perft 4
```

Expected start-position depth-4 node count: **197281**.

Useful additional test FENs should cover castling, en passant, promotions, pinned pieces and discovered checks.

## 2. Search regression

Keep a fixed tactical/strategic position suite. Track:

- best move
- score
- depth
- nodes
- NPS
- principal variation

## 3. SPRT

Use CuteChess-cli or OpenBench for engine-vs-engine testing. Every search/evaluation change should have a reproducible binary, configuration, time control and game count.

## 4. External rating

Only claim an Elo/rating after a published match protocol or independent rating list result. No internal node-count benchmark should be presented as an Elo estimate.
