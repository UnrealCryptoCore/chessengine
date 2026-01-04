#include "game.h"
#include <catch2/catch_test_macros.hpp>

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

    SECTION("Position 2: r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ") {
        game.loadFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
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

    SECTION("Position 5: rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8") {
        game.loadFen("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
        REQUIRE(game.perft(1) == 44);
        REQUIRE(game.perft(2) == 1486);
        REQUIRE(game.perft(3) == 62379);
    }

    SECTION(
        "Position 6: r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10") {
        game.loadFen("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
        REQUIRE(game.perft(1) == 46);
        REQUIRE(game.perft(2) == 2079);
        REQUIRE(game.perft(3) == 89890);
    }

    SECTION("Position 6: 8/Q1p5/8/6P1/Pk2B3/7P/KP1P3P/R1B5 w - - 1 49") {
        game.loadFen("8/Q1p5/8/6P1/Pk2B3/7P/KP1P3P/R1B5 w - - 1 49");
        REQUIRE(game.perft(1) == 33);
        REQUIRE(game.perft(2) == 99);
        REQUIRE(game.perft(3) == 3285);
        REQUIRE(game.perft(4) == 10085);
        REQUIRE(game.perft(5) == 334392);
    }
}
