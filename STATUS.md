# Aquila v0.2 Status

## Correctness-hardening release

| Area | Status |
|---|---|
| Bitboards | Implemented |
| Magic sliding attacks | Implemented + exhaustive startup self-test |
| Legal move generation | Implemented |
| Perft suite | Verified against reference counts |
| Make/unmake invariants | Deterministic randomized regression coverage |
| Zobrist consistency | Regression coverage against canonical recomputation |
| Threefold/fivefold repetition | Explicit claimable vs automatic semantics |
| 50-move / 75-move rules | Explicit claimable vs automatic semantics |
| Dead-position material cases | Explicit detector + regression coverage |
| TT mate-score normalization | Implemented + cross-ply regression test |
| UCI mate reporting | Emits `score mate N` for mate scores |
| Search termination | UCI stop regression coverage |
| UCI core | Implemented |
| Asynchronous stop | Implemented |
| Zobrist | Implemented |
| TT | Implemented |
| Iterative deepening | Implemented |
| PVS-style search | Implemented |
| LMR | Implemented |
| Null move | Implemented |
| Futility pruning | Implemented |
| Quiescence | Implemented |
| HCE | Baseline only |
| NNUE | Not yet implemented |
| SMP | Not yet implemented |
| PolyGlot book | Not yet implemented |
| Syzygy | Not yet implemented |
| SPRT harness | Not yet implemented |

## Strength claim

No Elo or "Stockfish-level" claim is made. This release establishes a correctness-tested baseline before further search/evaluation optimization.
