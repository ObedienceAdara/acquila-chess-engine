# Aquila Architecture

```mermaid
flowchart TD
    UCI[UCI loop] --> POS[Position / Board]
    UCI --> SEARCH[Searcher]
    POS --> GEN[Legal Move Generator]
    GEN --> SEARCH
    SEARCH --> TT[Transposition Table]
    SEARCH --> Q[Quiescence]
    SEARCH --> HEUR[Move Ordering]
    SEARCH --> EVAL[HCE Evaluation]
    SEARCH --> TIME[Time Manager]
    EVAL --> PST[Material + Piece-Square Tables]
    EVAL --> MOB[Mobility / King pressure]
```

## Position state

The board keeps both a mailbox array and color/piece bitboards. This deliberately trades a little update work for cheap piece lookup while leaving the core representation bitboard-native.

Zobrist hashing incorporates piece-square occupancy, side to move, castling rights and en-passant square. The repetition stack stores keys for the played line.

## Search

The search is a negamax alpha-beta tree with iterative deepening. Non-first moves use a PVS-style null window. The baseline also has TT cutoffs, quiescence, null-move pruning, futility pruning, late move reduction, killer moves and history heuristic.

The current implementation is intentionally single-threaded. SMP is a separate engineering phase because synchronization, split scheduling and shared-history quality can easily make a nominally parallel search slower.

## Evaluation

The current evaluator is an HCE baseline. NNUE is not stubbed as if it were trained. The intended production path is a separate feature/accumulator layer so that network representation and inference can evolve without rewriting move generation or search.
