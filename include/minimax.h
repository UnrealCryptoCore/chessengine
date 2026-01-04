#pragma once

#include "game.h"
#include <cstdint>

namespace Minimax {

inline uint32_t nodeCount = 0;

const int32_t winValue = 1 << 30;
const int32_t lossValue = -winValue;

int32_t simpleMinimax(ChessGame::Game &game, int32_t depth);

int32_t simpleAlphaBeta(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

}
