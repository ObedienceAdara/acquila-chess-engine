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
    checkmate.set_fen("6rk/8/8/8/8/8/6q1/6K1 w - - 150 1");
    Searcher mate_search(checkmate);
    require(mate_search.debug_search(2, -INF, INF, 0) == -MATE,
            "75-move threshold incorrectly masked checkmate");
}

void test_incremental_hash_special_moves() {
    auto check = [](Board& board, const std::vector<std::string>& moves, const std::string& label) {
        require(board.is_consistent(), label + ": initial invariant failed");
        for (const auto& uci : moves) {
            apply_uci(board, uci);
            require(board.is_consistent(), label + ": incremental Zobrist mismatch after " + uci);
            require(board.key == board.compute_key(), label + ": canonical key mismatch after " + uci);
        }
    };

    Board normal;
    check(normal, {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5"}, "normal/castling preparation");

    Board castle;
    check(castle, {"e2e4", "e7e5", "g1f3", "b8c6", "f1e2", "g8f6", "e1g1"}, "castling");

    Board en_passant;
    en_passant.set_fen("7k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
    check(en_passant, {"e5d6"}, "en-passant");

    Board promotion;
    promotion.set_fen("7k/P7/8/8/8/8/8/K7 w - - 0 1");
    check(promotion, {"a7a8q"}, "promotion");

    Board capture;
    capture.set_fen("7k/8/8/8/8/3p4/2P5/K6R w - - 0 1");
    check(capture, {"c2d3"}, "capture");
}

void test_tt_cluster_and_replacement() {
    TT tt(1);
    require(TT::cluster_size() == 4, "TT cluster size is not four");
    const U64 stride = static_cast<U64>(tt.bucket_count());
    const Move dummy = Move::make(0, 1);

    const U64 k[5] = {
        stride * 1ULL, stride * 2ULL, stride * 3ULL, stride * 4ULL, stride * 5ULL
    };

    for (int i = 0; i < 4; ++i)
        tt.store(k[i], 4 + i * 4, 100 + i, EXACT, dummy, 50 + i);

    for (int i = 0; i < 4; ++i) {
        const TTEntry* e = tt.probe(k[i]);
        require(e != nullptr, "TT cluster lost an inserted entry");
        require(e->depth == 4 + i * 4, "TT stored depth mismatch");
        require(e->flag == EXACT, "TT stored bound mismatch");
        require(e->move == dummy.data, "TT stored move mismatch");
        require(e->eval == 50 + i, "TT stored static eval mismatch");
    }

    tt.store(k[4], 20, 999, LOWER, dummy, 77);
    require(tt.probe(k[0]) == nullptr, "deep replacement did not evict shallowest entry");
    require(tt.probe(k[4]) != nullptr, "replacement entry was not stored");

    TT aged(1);
    const U64 a[5] = {
        stride * 11ULL, stride * 12ULL, stride * 13ULL, stride * 14ULL, stride * 15ULL
    };
    for (int i = 0; i < 4; ++i) aged.store(a[i], 20, i, EXACT, dummy, 0);
    aged.new_search();
    aged.new_search();
    aged.new_search();
    aged.store(a[4], 1, 99, LOWER, dummy, 0);
    require(aged.probe(a[0]) == nullptr, "aged TT entry was not replaced");
    require(aged.probe(a[4]) != nullptr, "new aged replacement was not stored");

    const TTEntry* retained = aged.probe(a[4]);
    require(retained->generation == 3, "TT generation was not advanced for new search");
}

void test_tt_semantics() {
    Board board;
    Searcher searcher(board);
    constexpr int depth = 3;

    const Score exact = searcher.debug_search(depth, -INF, INF, 0);
    const TTEntry* exact_entry = searcher.debug_tt().probe(board.key);
    require(exact_entry != nullptr, "exact root search did not store TT entry");
    require(exact_entry->flag == EXACT, "exact search stored wrong TT bound");
    require(exact_entry->depth == depth, "exact search stored wrong TT depth");
    require(score_from_tt(exact_entry->score, 0) == exact, "exact TT score mismatch");

    searcher.debug_reset_nodes();
    const Score cached = searcher.debug_search(depth - 1, -INF, INF, 1);
    require(cached == exact, "depth-qualified TT exact lookup returned wrong score");
    require(searcher.node_count() == 1, "TT exact cutoff still searched the tree");

    searcher.clear();
    searcher.debug_search(depth - 1, -INF, INF, 0);
    searcher.debug_reset_nodes();
    searcher.debug_search(depth, -INF, INF, 1);
    require(searcher.node_count() > 1, "shallower TT entry incorrectly cut off deeper search");

    const Move bound_move = Move::make(12, 28);

    searcher.clear();
    constexpr Score lower_score = 500;
    searcher.debug_tt().store(board.key, depth, score_to_tt(lower_score, 1), LOWER, bound_move, 17);
    const TTEntry* lower = searcher.debug_tt().probe(board.key);
    require(lower != nullptr && lower->flag == LOWER, "lower-bound entry was not stored");
    require(lower->depth == depth, "lower-bound depth mismatch");
    require(lower->move == bound_move.data, "lower-bound move mismatch");
    searcher.debug_reset_nodes();
    const Score lower_cutoff = searcher.debug_search(depth, 400, 500, 1);
    require(lower_cutoff == lower_score, "LOWER TT cutoff returned wrong score");
    require(lower_cutoff >= 500, "LOWER TT cutoff violated its bound");
    require(searcher.node_count() == 1, "LOWER TT bound did not cut off search");

    searcher.clear();
    constexpr Score upper_score = -500;
    searcher.debug_tt().store(board.key, depth, score_to_tt(upper_score, 1), UPPER, bound_move, -17);
    const TTEntry* upper = searcher.debug_tt().probe(board.key);
    require(upper != nullptr && upper->flag == UPPER, "upper-bound entry was not stored");
    require(upper->depth == depth, "upper-bound depth mismatch");
    searcher.debug_reset_nodes();
    const Score upper_cutoff = searcher.debug_search(depth, -500, -400, 1);
    require(upper_cutoff == upper_score, "UPPER TT cutoff returned wrong score");
    require(upper_cutoff <= -500, "UPPER TT cutoff violated its bound");
    require(searcher.node_count() == 1, "UPPER TT bound did not cut off search");

    searcher.clear();
    searcher.debug_search(depth, -INF, INF, 0);
    searcher.debug_reset_nodes();
    const Score repeated_root = searcher.debug_search(depth, -INF, INF, 0);
    require(repeated_root == exact, "repeated root search changed its result");
    require(searcher.node_count() > 1, "root TT entry incorrectly caused a root cutoff");

    TT mate_tt(1);
    const U64 mate_key = 0x123456789ULL;
    const Move dummy = Move::make(0, 1);
    const Score mate_score = MATE - 6;
    mate_tt.store(mate_key, 8, score_to_tt(mate_score, 5), EXACT, dummy, 0);
    const TTEntry* me = mate_tt.probe(mate_key);
    require(me != nullptr, "mate TT entry missing");
    require(score_from_tt(me->score, 5) == mate_score, "mate score failed same-ply round trip");
    require(score_from_tt(me->score, 9) == mate_score - 4, "mate score failed cross-ply normalization");
}

void test_see_and_move_ordering() {
    Board winning;
    winning.set_fen("3r3k/8/8/8/8/8/8/3QK3 w - - 0 1");
    Searcher winning_search(winning);
    const Move winning_capture=find_uci(winning,"d1d8");
    require(winning_capture.data!=0, "winning SEE fixture move is illegal");
    require(winning_search.debug_see(winning_capture)>0, "SEE failed to recognize winning capture");

    Board losing;
    losing.set_fen("3r1q1k/8/8/8/8/8/8/3QK3 w - - 0 1");
    Searcher losing_search(losing);
    const Move losing_capture=find_uci(losing,"d1d8");
    require(losing_capture.data!=0, "losing SEE fixture move is illegal");
    require(losing_search.debug_see(losing_capture)<0, "SEE failed to recognize losing exchange");

    Board promotion;
    promotion.set_fen("7k/P7/8/8/8/8/8/K7 w - - 0 1");
    Searcher promotion_search(promotion);
    const Move promote=find_uci(promotion,"a7a8q");
    require(promote.data!=0, "promotion ordering fixture move is illegal");

    const int winning_score=winning_search.debug_move_score(winning_capture);
    const int losing_score=losing_search.debug_move_score(losing_capture);
    require(winning_score>losing_score, "winning SEE capture was not ordered above losing capture");

    const auto moves=promotion.legal();
    const int promotion_score=promotion_search.debug_move_score(promote);
    const Move killer=moves.front();
    const int killer_score=promotion_search.debug_move_score(killer,Move{},Move{},0);
    require(promotion_score>killer_score, "promotion was not ordered above quiet moves");

    const int tt_score=promotion_search.debug_move_score(killer,killer,Move{},0);
    require(tt_score>promotion_score, "TT move was not given highest priority");

    const int counter_score=promotion_search.debug_move_score(killer,Move{},killer,0);
    require(counter_score>killer_score, "counter-move priority was not applied");
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
        test_incremental_hash_special_moves();
        test_draw_rules();
        test_tt_cluster_and_replacement();
        test_tt_semantics();
        test_see_and_move_ordering();
        test_mate_tt_normalization();
        test_uci_score_formatting();
        std::cout << "core regression tests: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "core regression tests: FAIL: " << e.what() << "\n";
        return 1;
    }
}
