#include "game.h"
#include <cstdint>
#include <iostream>
#include <print>
#include <ranges>
#include <string>
#include <vector>

void play(ChessGame::Game &game) {
    std::vector<ChessGame::Move> moveHistory;
    std::string inp;
    while (1) {
        game.showBoard();
        ChessGame::MoveList moves;
        game.validMoves(moves);
        for (auto [idx, move] : std::views::enumerate(moves)) {
            std::print("{}: {}, ", idx, move.toAlgebraicNotation(game.board[move.from]));
        }
        while (1) {
            std::print("\nEnter move number: ");
            std::getline(std::cin, inp);
            if (inp.starts_with("undo")) {
                game.undoMove(moveHistory.back());
                moveHistory.pop_back();
                break;
            } else {
                uint8_t move = std::stoi(inp);
                game.playMove(moves[move]);
                moveHistory.push_back(moves[move]);
                break;
            }
        }
    }
}


void perft(ChessGame::Game &game) {
    for (uint32_t i = 1; i < 6; i++) {
        perftInfo(game, i);
    }
}

int main() {
    ChessGame::initConstants();
    auto game = ChessGame::Game{};
    game.loadStartingPos();
    game.loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    //std::print("{}\n", game.perft(1));
    //perftInfo(game, 5);
    play(game);
}
