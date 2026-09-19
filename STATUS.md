# Aquila v0.4 Status

## Quiescence + tapered HCE release

| Area | Status |
|---|---|
| Bitboards | Implemented |
| Magic sliding attacks | Implemented + exhaustive startup self-test |
| Legal move generation | Implemented |
| Perft suite | Verified against reference counts |
| Make/unmake invariants | Deterministic randomized regression coverage |
| Incremental Zobrist hashing | O(1) make updates with canonical recomputation regression checks |
| Threefold/fivefold repetition | Explicit claimable vs automatic semantics |
| 50-move / 75-move rules | Explicit claimable vs automatic semantics |
| Dead-position material cases | Explicit detector + regression coverage |
| TT mate-score normalization | Implemented + cross-ply regression test |
| Clustered TT | Four-entry buckets with depth/age-aware replacement |
| SEE | Static exchange evaluation for tactical capture ordering |
| Counter-moves | Previous-move indexed quiet-move ordering heuristic |
| Aspiration windows | Iterative-deepening root windows with widening on fail low/high |
| LMR | Depth/move-number/history/PV/check-aware reductions |
| Null move | Low-material guards plus deeper verification search |
| Futility pruning | Selective shallow quiet-move pruning |
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
| Quiescence | SEE-filtered captures + promotions + controlled checks + delta pruning + stand-pat + bounded evasions |
| Quiescence regressions | Multi-capture horizon + quiet-check coverage |
| HCE | Layered tapered evaluator |
| HCE material | Tuned middlegame/endgame piece values |
| HCE pawn structure | Passed, protected passed, isolated, doubled, backward, connected, chains, islands |
| HCE piece activity | Mobility, outposts, bishop pair, open/semi-open files, 7th rank, trapped pieces, coordination |
| HCE king safety | Shelter, pawn shield, king-ring attacks, open files, safe squares |
| HCE space/development | Center control, space, minor-piece development |
| HCE endgame | King activity, passer races, rook-behind-passer activity |
| NNUE | Not yet implemented |
| SMP | Not yet implemented |
| PolyGlot book | Not yet implemented |
| Syzygy | Not yet implemented |
| SPRT harness | Not yet implemented |

## Strength claim

No Elo or "Stockfish-level" claim is made. This release establishes a correctness-tested tactical-horizon and classical-evaluation baseline before quantitative strength testing. The new heuristics are engineering changes, not an Elo claim.
