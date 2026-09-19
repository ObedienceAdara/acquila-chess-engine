# NNUE Training Plan

The repository currently ships an HCE evaluator. NNUE is intentionally not faked or hidden behind a dummy class.

## Target pipeline

1. Generate positions with fixed Stockfish versions and record FEN + score + best move.
2. Store compact training shards rather than millions of individual text files.
3. Train in PyTorch using quantization-aware constraints compatible with integer inference.
4. Export weights to a stable binary format.
5. Implement a C++ inference path with an efficiently updated accumulator.
6. Validate offline error metrics and then validate playing strength with SPRT.

## Recommended evolution

- First network: small HalfKP-compatible proof of concept.
- Second network: larger feature transformer and incremental accumulator.
- Later networks: follow current Stockfish architecture research rather than freezing the project around an obsolete architecture.

## Data discipline

Do not mix evaluation targets from incompatible engine versions without recording the source engine, version/build hash, search limit and MultiPV settings.
