#include "evaluation.h"
#include "game.h"
#include <array>
#include <bit>
#include <cstdint>
#include <print>

namespace Mondfisch::Evaluation {

void show_piece_square_table(const std::array<int16_t, 64> &squares) {
    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            std::print("|{:>3d}", squares[coords_to_pos(x, 7 - y)]);
        }
        std::print("| {}\n", 8 - y);
    }
    for (auto c : files) {
        std::print("  {} ", c);
    }
    std::print("\n");
}

int32_t eval_phase(Game &game) {
    int32_t phase = total_phase;

    for (uint8_t i = 1; i < numberChessPieces; i++) {
        phase -= std::popcount(game.bitboard[0][i]) * piece_phases[i - 1];
        phase -= std::popcount(game.bitboard[1][i]) * piece_phases[i - 1];
    }
    phase = (phase * 256 + (total_phase / 2)) / total_phase;
    return phase;
}

int32_t eval(int32_t opening, int32_t engame, int32_t phase) {
    return ((opening * (256 - phase)) + (engame * phase)) / 256;
}

int32_t tapered_eval(Game &game) {
    int32_t phase = eval_phase(game);
    int32_t opening = simple_evaluate<mg_piece_table, mg_value>(game);
    int32_t endgame = simple_evaluate<eg_piece_table, eg_value>(game);

    return eval(opening, endgame, phase);
}

template <std::array<std::array<std::array<int32_t, 64>, numberChessPieces>, 2> table,
          std::array<int, numberChessPieces + 1> pieceValue>
int32_t simple_evaluate(Game &game) {
    int32_t value = 0;
    for (int piece = int(Piece::KING); piece < int(Piece::NONE); piece++) {
        for (Position pos : BitRange{game.bitboard[0][piece]}) {
            value += table[0][piece][pos];
            value += pieceValue[piece];
        }
        for (Position pos : BitRange{game.bitboard[1][piece]}) {
            value -= table[1][piece][pos];
            value -= pieceValue[piece];
        }
    }
    return value;
}

} // namespace Mondfisch::Evaluation
