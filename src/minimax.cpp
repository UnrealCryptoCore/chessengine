#include "minimax.h"
#include "evaluation.h"
#include "game.h"
#include <cstdint>

namespace Minimax {

int32_t simpleMinimax(ChessGame::Game &game, int32_t depth) {
    if (depth == 0) {
        return ChessGame::signedColor[game.color] * ChessGame::Evaluation::evaluate(game);
    }

    ChessGame::MoveList moves;
    game.pseudoLegalMoves(moves);
    bool legalMove = false;
    int32_t value = lossValue;
    for (auto move : moves) {
        game.playMove(move);
        if (game.isCheck(game.color)) {
            game.undoMove(move);
            continue;
        }
        legalMove = true;
        int32_t tmpVal = -simpleMinimax(game, depth - 1);
        if (tmpVal > value) {
            value = tmpVal;
        }
        game.undoMove(move);
    }

    if (!legalMove) {
        if (game.isCheck(!game.color)) {
            return lossValue;
        } else {
            return 0;
        }
    }
    return value;
}

int32_t simpleAlphaBeta(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth) {
    if (depth == 0) {
        return ChessGame::signedColor[game.color] * ChessGame::Evaluation::evaluate(game);
    }
    int32_t bestValue = lossValue;

    ChessGame::MoveList moves;
    game.pseudoLegalMoves(moves);
    bool legalMove = false;

    for (auto move : moves) {
        game.playMove(move);
        if (game.isCheck(game.color)) {
            game.undoMove(move);
            continue;
        }
        legalMove = true;
        int32_t score = -simpleAlphaBeta(game, -beta, -alpha, depth - 1);
        game.undoMove(move);
        if (score > bestValue) {
            bestValue = score;
            if (alpha > beta) {
                alpha = score;
            }
        }
        if (score >= beta) {
            return bestValue;
        }
    }

    if (!legalMove) {
        if (game.isCheck(!game.color)) {
            return lossValue;
        } else {
            return 0;
        }
    }
    return bestValue;
}

} // namespace Minimax
