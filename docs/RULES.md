# Rule Handling

Aquila tracks the FEN state needed for legal play: side to move, castling rights, en-passant target, halfmove clock and fullmove number.

Implemented:

- castling legality through empty-square and attacked-square checks
- en-passant captures
- promotion to knight, bishop, rook and queen
- check, checkmate and stalemate
- threefold repetition detection
- fivefold repetition detection
- fifty-move claimable state detection
- seventy-five-move automatic draw threshold
- common dead-position/insufficient-material cases

The dead-position rule in FIDE is broader than a small "insufficient material" lookup. The current implementation deliberately covers common mechanically-detectable cases and does not claim to solve every possible dead-position proof.
