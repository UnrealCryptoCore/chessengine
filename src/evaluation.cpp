#include "evaluation.h"
#include "game.h"
#include <cstdint>
#include <print>

namespace ChessGame {
namespace Evaluation {

void show_piece_square_table(const std::array<int16_t, 64> &squares) {
    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            std::print("|{:>3d}", squares[coords_to_pos(x, 7-y)]);
        }
        std::print("| {}\n", 8 - y);
    }
    for (auto c : files) {
        std::print("  {} ", c);
    }
    std::print("\n");
}

int32_t evaluate(Game &game) {
    int32_t value = 0;
    for (uint8_t piece = (uint8_t)Piece::KING; piece < (uint8_t)Piece::NONE; piece++) {
        for (Position pos : BitRange{game.bitboard[0][piece]}) {
            value += piece_table[0][piece][pos];
            value += pieceValues[piece];
        }
        for (Position pos : BitRange{game.bitboard[1][piece]}) {
            value -= piece_table[1][piece][pos];
            value -= pieceValues[piece];
        }
 
    }
    return value;
}

} // namespace Evaluation
} // namespace ChessGame
