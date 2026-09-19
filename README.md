# Aquila Chess Engine

Aquila is a standalone C++17 chess engine built from scratch as an engineering foundation for progressively stronger search, evaluation, and performance work. It speaks the Universal Chess Interface (UCI), includes a verified legal move generator, and is structured for continuous benchmarking and improvement.

> **Status:** v0.4 quiescence + tapered HCE foundation. Aquila is not currently claimed to be Stockfish-strength.

## Highlights

- 64-bit bitboards with mailbox state for convenient board access
- Fixed-shift magic bitboards for rook and bishop attacks
- Exhaustive startup validation of the magic attack tables
- Legal move generation with castling, en passant, promotion, and underpromotion
- Incremental Zobrist hashing and repetition tracking with invariant checks
- Separate claimable/automatic draw handling and common dead-position detection
- Negamax alpha-beta search with iterative deepening
- Four-entry clustered transposition table with generation-aware replacement and principal-variation search
- Selective quiescence: SEE-filtered captures, promotions, controlled checking moves, delta pruning, stand-pat handling, and bounded check evasions
- Tapered HCE: tuned material, pawn structure, piece activity, king safety, space/development, rook-file activity, and endgame-specific terms
- Aspiration windows, history-aware late-move reduction, guarded null-move pruning, and selective futility pruning
- Systematic move ordering: TT move, SEE-ranked captures, promotions, killers, counter-moves, and history-ranked quiet moves
- UCI time management and asynchronous `stop` support
- Built-in `perft`, FEN, move-list, and debug diagnostics

## Repository layout

```text
.
├── src/                 # Engine implementation
├── tests/               # Perft and smoke tests
├── tools/               # Self-play / dataset tooling
├── docs/                # Architecture, rules, and roadmap
├── CMakeLists.txt       # CMake build
├── Makefile             # Make-based build and tests
├── TRAINING.md          # NNUE/data-generation design
├── TESTING.md           # Validation and benchmarking methodology
└── STATUS.md            # Current implementation status
```

## Build

### Linux / macOS

```bash
make -j
```

For a CMake build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The Make build produces `aquila` and `aquila-portable`. The portable binary avoids the optional native tuning used by the optimized build.

## Run as a UCI engine

Start the executable and send standard UCI commands:

```text
uci
isready
position startpos moves e2e4 e7e5 g1f3
go depth 12
```

Aquila is designed to work with UCI-compatible chess software such as Cute Chess and Arena.

Useful diagnostics include:

```text
perft 4
fen
moves
d
```

## Verification

The current move generator matches the included reference counts for the tested positions:

| Position | Depth | Nodes |
|---|---:|---:|
| Start position | 4 | 197,281 |
| Kiwipete | 4 | 4,085,603 |
| Endgame test | 4 | 43,238 |
| En-passant / castling | 4 | 1,280,017 |
| Promotion-heavy | 5 | 5,617,302 |

Run the full correctness suite with:

```bash
make test
```

The regression suite covers perft, deterministic make/unmake state restoration, incremental Zobrist-key consistency, repetition and move-count draw semantics, TT cluster/replacement behavior, TT bound/depth semantics, mate-distance normalization, quiescence tactical-horizon behavior, tapered-HCE directional behavior, UCI mate-score formatting, and asynchronous search termination.

## Development direction

The project is intentionally staged. The current baseline now has stronger tactical horizon handling and a layered tapered HCE. Next phases are controlled measurement/SPRT testing, further search improvements, SMP, production NNUE inference and training, PolyGlot opening-book support, and Syzygy tablebases. See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## NNUE / training

[`TRAINING.md`](TRAINING.md) documents the intended dataset and NNUE training pipeline. [`tools/selfplay.py`](tools/selfplay.py) provides a reproducible UCI self-play data-generation starting point.

## Engineering notes

Aquila uses an independently written implementation rather than copying Stockfish internals. Stockfish is used as a reference point for engine architecture, benchmarking practices, and the eventual NNUE/search direction.

The current version deliberately makes no Elo or competitive-strength claim.

## License

MIT. See [`LICENSE`](LICENSE).
