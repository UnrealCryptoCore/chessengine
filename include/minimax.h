#pragma once

#include "game.h"
#include <array>
#include <cstdint>

namespace Minimax {

inline uint32_t nodeCount = 0;

const int32_t winValue = 1 << 30;
const int32_t lossValue = -winValue;

enum class NodeType : uint8_t {
    NONE = 0,
    EXACT,
    UPPER_BOUND,
    LOWER_BOUND,
};

struct TableEntry {
    uint64_t hash;
    int32_t score;
    uint8_t best;
    uint8_t depth;
    NodeType type;
};

// struct TranspositionTable {};
typedef std::array<TableEntry, 1024 * 1024> TranspositionTable;

int32_t simpleMinimax(ChessGame::Game &game, int32_t depth);

int32_t simpleAlphaBeta(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

int32_t alphaBetaMO(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

int32_t alphaBetaMOTT(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

} // namespace Minimax
