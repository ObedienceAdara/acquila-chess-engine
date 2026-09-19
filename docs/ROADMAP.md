# Competitive Roadmap

## Phase 0 — correctness

- Fixed-shift magic sliding attacks are implemented and exhaustively self-tested at startup.
- Add a broad published perft corpus.
- Add property tests for make/unmake and Zobrist stability.

## Phase 1 — strong classical search

- Multi-entry clustered TT.
- Better move picker stages.
- Null-move verification and zugzwang guards.
- More precise LMR tables.
- SEE, check extensions, singular extensions and razoring where justified by testing.
- Shared history tables and continuation history.
- SMP workers with Lazy SMP or an equivalent reproducible approach.

## Phase 2 — evaluation

- Robust tapered HCE with pawn structure, king safety, mobility, threats and passed-pawn terms.
- Automated parameter tuning.
- Regression suite.

## Phase 3 — NNUE

- Feature transform + incremental accumulators.
- Quantized SIMD inference.
- PyTorch training/export pipeline.
- Network versioning and checksum validation.
- SPTR/STS-style validation before adoption.

## Phase 4 — ecosystem

- PolyGlot book reader.
- Syzygy WDL/DTZ probing.
- Better UCI options and diagnostics.
- CuteChess tournament harness.

## Phase 5 — serious strength work

Strength comes from repeated measurement, not from adding a long list of search heuristics. Every change should have a baseline, reproducible test conditions and statistically meaningful results.
