#include "game.h"
#include <algorithm>
#include <bit>
#include <cassert>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstdlib>
#include <print>
#include <unordered_map>

TEST_CASE("Computing valid positions", "[perft]") {
    ChessGame::initConstants();
    ChessGame::Game game{};
    SECTION("Position 1: Startpos") {
        game.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        REQUIRE(game.perft(1) == 20);
        REQUIRE(game.perft(2) == 400);
        REQUIRE(game.perft(3) == 8902);
        REQUIRE(game.perft(4) == 197281);
        REQUIRE(game.perft(5) == 4865609);
    }

    SECTION("Position 2: r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1") {
        game.loadFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        REQUIRE(game.perft(1) == 48);
        REQUIRE(game.perft(2) == 2039);
        REQUIRE(game.perft(3) == 97862);
        REQUIRE(game.perft(4) == 4085603);
    }

    SECTION("Position 3: 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1") {
        game.loadFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
        REQUIRE(game.perft(1) == 14);
        REQUIRE(game.perft(2) == 191);
        REQUIRE(game.perft(3) == 2812);
        REQUIRE(game.perft(4) == 43238);
        REQUIRE(game.perft(5) == 674624);
    }

    SECTION("Position 4: r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1") {
        game.loadFen("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
        REQUIRE(game.perft(1) == 6);
        REQUIRE(game.perft(2) == 264);
        REQUIRE(game.perft(3) == 9467);
        REQUIRE(game.perft(4) == 422333);
    }

    const std::string p5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
    SECTION("Position 5: " + p5) {
        game.loadFen(p5);
        REQUIRE(game.perft(1) == 44);
        REQUIRE(game.perft(2) == 1486);
        REQUIRE(game.perft(3) == 62379);
    }

    const std::string p6 =
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
    SECTION("Position 6: " + p6) {
        game.loadFen(p6);
        REQUIRE(game.perft(1) == 46);
        REQUIRE(game.perft(2) == 2079);
        REQUIRE(game.perft(3) == 89890);
    }

    const std::string p7 = "8/Q1p5/8/6P1/Pk2B3/7P/KP1P3P/R1B5 w - - 1 49";
    SECTION("Position 7: " + p7) {
        game.loadFen(p7);
        REQUIRE(game.perft(1) == 33);
        REQUIRE(game.perft(2) == 99);
        REQUIRE(game.perft(3) == 3285);
        REQUIRE(game.perft(4) == 10085);
        REQUIRE(game.perft(5) == 334392);
    }

    const std::string p8 = "2r3k1/1q1nbppp/r3p3/3pP3/pPpP4/P1Q2N2/2RN1PPP/2R4K b - b3 0 23";
    SECTION("Position 8: " + p8) {
        game.loadFen(p8);
        REQUIRE(game.perft(1) == 46);
        REQUIRE(game.perft(2) == 1356);
        REQUIRE(game.perft(3) == 56661);
        REQUIRE(game.perft(4) == 1803336);
    }
}

TEST_CASE("SEE Tests", "[see]") {
    ChessGame::initConstants();
    ChessGame::Game game{};

    const std::string p1 = "1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - 0 1; Rxe5?";
    SECTION("Position 1:" + p1) {
        game.loadFen(p1);
        int32_t val = game.see(ChessGame::str2pos("e1"), ChessGame::str2pos("e5"), game.color);
        REQUIRE(val == 100);
    }

    const std::string p2 = "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1; Nxe5?";
    SECTION("Position 2:" + p2) {
        game.loadFen(p2);
        int32_t val = game.see(ChessGame::str2pos("d3"), ChessGame::str2pos("e5"), game.color);
        REQUIRE(val == -220);
    }

    const std::string p3 = "1k1r3q/1ppn3p/p4b2/4p3/5P2/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1; Nxe5?";
    SECTION("Position 3:" + p3) {
        game.loadFen(p3);
        int32_t val = game.see(ChessGame::str2pos("d3"), ChessGame::str2pos("e5"), game.color);
        REQUIRE(val == 100);
    }
}

TEST_CASE("Zobrist Hashing Quality Tests", "[hashing]") {
    ChessGame::initConstants();
    ChessGame::Game game{};
    game.loadStartingPos();

    SECTION("Avalanche Effect: Single move should flip ~32 bits") {
        const int iterations = 10000;
        double total_bits_flipped = 0;

        for (int i = 0; i < iterations; ++i) {
            uint64_t hash_before = game.get_hash();

            ChessGame::MoveList moves;
            game.legal_moves(moves);
            if (moves.empty()) {
                game.loadStartingPos();
                continue;
            }

            ChessGame::ScoreMove m = moves[i % moves.size()];
            game.make_move(m.move);

            uint64_t hash_after = game.get_hash();
            total_bits_flipped += std::popcount(hash_before ^ hash_after);

            game.undo_move(m.move);
        }

        double average_flips = total_bits_flipped / iterations;

        // The expected value is 32. We allow a small margin for statistical noise.
        REQUIRE_THAT(average_flips, Catch::Matchers::WithinAbs(32.0, 1.0));
    }

    SECTION("Bit Bias: Every bit should be set ~50% of the time") {
        const int iterations = 20000;
        std::vector<int> bit_counts(64, 0);

        for (int i = 0; i < iterations; ++i) {

            // Use a random walk to get diverse positions
            ChessGame::MoveList moves;
            game.legal_moves(moves);
            if (moves.empty()) {
                game.loadStartingPos();
                game.legal_moves(moves);
            }
            auto move = moves[rand() % moves.size()].move;
            game.make_move(move);
            game.undoStack.clear();
            game.history.clear();

            uint64_t h = game.get_hash();
            for (int bit = 0; bit < 64; ++bit) {
                if ((h >> bit) & 1)
                    bit_counts[bit]++;
            }
        }

        for (int bit = 0; bit < 64; ++bit) {
            double probability = (double)bit_counts[bit] / iterations;
            // Every bit should be 1 roughly 50% of the time
            CHECK_THAT(probability, Catch::Matchers::WithinAbs(0.5, 0.05));
        }
    }

    SECTION("Collision Resistance: No collisions in random walk") {
        std::unordered_map<uint64_t, std::string> seen_hashes;
        const int positions_to_check = 100000;
        int collisions = 0;

        for (int i = 0; i < positions_to_check; ++i) {
            ChessGame::MoveList moves;
            game.legal_moves(moves);

            if (moves.empty() || i % 100 == 0) {
                moves.clear();
                game.loadStartingPos();
                game.legal_moves(moves);
            }

            game.make_move(moves[rand() % moves.size()].move);

            uint64_t h = game.get_hash();
            std::string fen = game.dumpFen();

            if (seen_hashes.contains(h)) {
                if (seen_hashes[h] != fen) {
                    collisions++;
                }
            } else {
                seen_hashes[h] = fen;
            }
        }

        // In a 64-bit space, finding 1 collision in 100k positions
        // is effectively impossible with a good hash.
        CHECK(collisions == 0);
    }
}
