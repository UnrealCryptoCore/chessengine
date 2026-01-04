#include "evaluation.h"
#include "game.h"
#include <cstdint>

namespace ChessGame {
namespace Evaluation {

int32_t evaluate(Game &game) {
    int32_t value = 0;
    for (uint8_t piece = (uint8_t)Piece::KING; piece < (uint8_t)Piece::NONE; piece++) {
        for (Position pos : BitRange{game.bitboard[0][piece]}) {
            value += pieceTable[piece][pos];
            value += pieceValues[piece];
        }
        for (Position pos : BitRange{game.bitboard[1][piece]}) {
            value -= pieceTable[piece][pos];
            value -= pieceValues[piece];
        }
 
    }
    return value;
}

} // namespace Evaluation
} // namespace ChessGame
