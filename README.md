# Aquila Chess Engine

Aquila is a standalone C++17 chess engine built from scratch as an engineering foundation for progressively stronger search, evaluation, and performance work. It speaks the Universal Chess Interface (UCI), includes a verified legal move generator, and is structured for continuous benchmarking and improvement.

> **Status:** v0.1 foundation. Aquila is not currently claimed to be Stockfish-strength.

## Highlights

- 64-bit bitboards with mailbox state for convenient board access
- Fixed-shift magic bitboards for rook and bishop attacks
- Exhaustive startup validation of the magic attack tables
- Legal move generation with castling, en passant, promotion, and underpromotion
- Zobrist hashing and repetition tracking
- Fifty-move and 75-move draw handling plus common dead-position detection
- Negamax alpha-beta search with iterative deepening
- Transposition table and principal-variation search
- Quiescence search, late-move reduction, null-move pruning, and futility pruning
- TT move, MVV-LVA, killer, and history move ordering
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

Run the suite with:

```bash
make test
```

## Development direction

The project is intentionally staged. The next strength-oriented phases are: stronger search heuristics and SEE, a tuned tapered evaluator, SMP, production NNUE inference and training, PolyGlot opening-book support, Syzygy tablebases, and automated SPRT/tournament testing. See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## NNUE / training

[`TRAINING.md`](TRAINING.md) documents the intended dataset and NNUE training pipeline. [`tools/selfplay.py`](tools/selfplay.py) provides a reproducible UCI self-play data-generation starting point.

## Engineering notes

Aquila uses an independently written implementation rather than copying Stockfish internals. Stockfish is used as a reference point for engine architecture, benchmarking practices, and the eventual NNUE/search direction.

The current version deliberately makes no Elo or competitive-strength claim.

## License

MIT. See [`LICENSE`](LICENSE).
