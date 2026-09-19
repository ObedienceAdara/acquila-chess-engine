# Testing & Benchmarking

## 1. Perft correctness

Run known perft suites at increasing depths. A move generator should not be tuned for speed until node counts match reference counts.

Example:

`position startpos`
`perft 4`

Expected start-position depth-4 node count: **197281**.

Useful additional test FENs should cover castling, en passant, promotions, pinned pieces and discovered checks.

## 2. Core correctness regression

Run `make test` before every strength change. The deterministic core harness covers:

- make/unmake exact state restoration
- bitboard/mailbox/occupancy invariants
- Zobrist key consistency
- threefold/fivefold repetition
- 50-move claim and 75-move automatic draw semantics
- dead-position material cases
- TT cluster collision and replacement behavior
- generation/age replacement behavior
- EXACT/LOWER/UPPER bound storage and cutoffs
- TT depth-eligibility and repeated-root behavior
- TT mate-distance normalization across different plies
- checkmate precedence over the 75-move threshold

## 3. UCI regression

The UCI harness verifies:

- `uci` / `uciok`
- `isready` / `readyok`
- forced-mate reporting as `score mate N`
- absence of mate sentinels masquerading as centipawns
- responsive asynchronous `stop` handling

## 5. Strength/search regression

Keep a fixed tactical/strategic position suite. Track:

- best move
- score
- depth
- nodes
- NPS
- principal variation
- aspiration re-search frequency
- null-move cutoffs and verification failures
- futility-pruned moves
- reduced versus full-depth re-searches

Strength changes should be introduced one mechanism at a time and compared by fixed self-play/tournament runs.

## 7. SPRT

Use CuteChess-cli or OpenBench for engine-vs-engine testing. Every search/evaluation change should have a reproducible binary, configuration, time control and game count.

## 8. External rating

Only claim an Elo/rating after a published match protocol or independent rating list result. No internal node-count benchmark should be presented as an Elo estimate.
