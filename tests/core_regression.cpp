#define main aquila_embedded_main
#include "../src/main.cpp"
#undef main

#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace aquila;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool same_state(const Board& a, const Board& b) {
    if (std::memcmp(a.b, b.b, sizeof(a.b)) != 0) return false;
    if (std::memcmp(a.bb, b.bb, sizeof(a.bb)) != 0) return false;
    if (std::memcmp(a.occ, b.occ, sizeof(a.occ)) != 0) return false;
    return a.all == b.all &&
           a.side == b.side &&
           a.castle == b.castle &&
           a.ep == b.ep &&
           a.halfmove == b.halfmove &&
           a.fullmove == b.fullmove &&
           a.key == b.key &&
           a.history == b.history;
}

void apply_uci(Board& board, const std::string& uci) {
    Move move = find_uci(board, uci);
    require(move.data != 0, "could not parse legal move: " + uci);
    Undo undo;
    require(board.make(move, undo), "make failed for move: " + uci);
}

std::uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;
    std::uint64_t nodes = 0;
    for (const Move& move : board.legal()) {
        Undo undo;
        require(board.make(move, undo), "perft make failed");
        nodes += perft(board, depth - 1);
        board.undo(undo);
    }
    return nodes;
}

void test_perft() {
    Board board;
    require(perft(board, 4) == 197281, "start-position perft mismatch");
}

void test_make_unmake_and_zobrist() {
    Board board;
    require(board.is_consistent(), "initial board invariant failed");

    Board after = board;
    apply_uci(after, "e2e4");
    require(after.is_consistent(), "post-move invariant failed");
    require(after.key == after.compute_key(), "Zobrist recomputation mismatch");
    require(after.key != board.key, "Zobrist key did not change");

    std::mt19937 rng(0xA51C0DEu);

    for (int game = 0; game < 64; ++game) {
        Board path;
        Board initial = path;
        std::vector<Undo> stack;

        for (int ply = 0; ply < 100; ++ply) {
            require(path.is_consistent(), "path invariant failed before move");
            const auto moves = path.legal();
            if (moves.empty()) break;

            const Move selected = moves[std::uniform_int_distribution<std::size_t>(0, moves.size() - 1)(rng)];

            Board before = path;
            Undo probe;
            require(path.make(selected, probe), "random probe make failed");
            require(path.is_consistent(), "random post-make invariant failed");
            require(path.key == path.compute_key(), "random Zobrist mismatch");
            path.undo(probe);
            require(path.is_consistent(), "random post-undo invariant failed");
            require(same_state(path, before), "make/unmake failed exact round trip");

            Undo played;
            require(path.make(selected, played), "random path make failed");
            require(path.is_consistent(), "random path invariant failed");
            stack.push_back(played);
        }

        while (!stack.empty()) {
            path.undo(stack.back());
            stack.pop_back();
            require(path.is_consistent(), "reverse-unmake invariant failed");
        }
        require(same_state(path, initial), "full random make/unmake restoration failed");
    }
}

void test_draw_rules() {
    Board repetition;
    const std::vector<std::string> cycle{"g1f3", "g8f6", "f3g1", "f6g8"};
    for (int round = 0; round < 2; ++round)
        for (const auto& move : cycle)
            apply_uci(repetition, move);

    require(repetition.is_threefold_repetition(), "threefold repetition not detected");
    require(!repetition.is_fivefold_repetition(), "fivefold repetition detected too early");
    require(repetition.is_claimable_draw(), "threefold claim not exposed");
    require(!repetition.is_automatic_draw(), "threefold incorrectly classified as automatic");

    for (int round = 2; round < 4; ++round)
        for (const auto& move : cycle)
            apply_uci(repetition, move);

    require(repetition.is_fivefold_repetition(), "fivefold repetition not detected");
    require(repetition.is_automatic_draw(), "fivefold not classified as automatic");

    Board fifty;
    fifty.set_fen("7k/8/8/8/8/8/8/K5R1 w - - 100 1");
    require(fifty.is_fifty_move_claimable(), "50-move claim not detected");
    require(fifty.is_claimable_draw(), "50-move claim not exposed");
    require(!fifty.is_automatic_draw(), "50-move claim incorrectly classified as automatic");

    Board seventyfive;
    seventyfive.set_fen("7k/8/8/8/8/8/8/K5R1 w - - 150 1");
    require(seventyfive.is_seventyfive_move_draw(), "75-move automatic draw not detected");
    require(seventyfive.is_automatic_draw(), "75-move draw not classified as automatic");

    Board dead;
    dead.set_fen("7k/8/8/8/8/8/8/K7 w - - 0 1");
    require(dead.is_dead_position(), "K vs K dead position not detected");

    Board knightless;
    knightless.set_fen("7k/8/8/8/8/8/1N6/K7 w - - 0 1");
    require(knightless.is_dead_position(), "K+N vs K dead position not detected");

    Board checkmate;
    checkmate.set_fen("7k/8/8/8/8/8/6q1/6K1 w - - 150 1");
    Searcher mate_search(checkmate);
    require(mate_search.debug_search(2, -INF, INF, 0) == -MATE,
            "75-move threshold incorrectly masked checkmate");
}

void test_mate_tt_normalization() {
    Board mate;
    mate.set_fen("7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
    Searcher searcher(mate);

    const Score first = searcher.debug_search(1, -INF, INF, 1);
    const Score cached_at_later_ply = searcher.debug_search(1, -INF, INF, 5);

    require(first == MATE - 2, "unexpected mate score in TT seed search");
    require(cached_at_later_ply == MATE - 6, "TT mate score was not normalized by ply");
    require(first - cached_at_later_ply == 4, "mate distance changed incorrectly across TT lookup");
}

void test_uci_score_formatting() {
    require(uci_score(MATE - 1) == "score mate 1", "positive mate UCI formatting failed");
    require(uci_score(-MATE + 2) == "score mate -1", "negative mate UCI formatting failed");
    require(uci_score(123) == "score cp 123", "centipawn UCI formatting failed");
}

} // namespace

int main() {
    try {
        init_attacks();
        test_perft();
        test_make_unmake_and_zobrist();
        test_draw_rules();
        test_mate_tt_normalization();
        test_uci_score_formatting();
        std::cout << "core regression tests: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "core regression tests: FAIL: " << e.what() << "\n";
        return 1;
    }
}
