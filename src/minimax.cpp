#include "minimax.h"
#include "evaluation.h"
#include "game.h"
#include <cstdint>

namespace Minimax {

int32_t simpleMinimax(ChessGame::Game &game, int32_t depth) {
    nodeCount++;
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
    nodeCount++;
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
            if (score > alpha) {
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

int32_t score_move(ChessGame::Game &game, ChessGame::Move move) {
    if (move.promote != ChessGame::Piece::NONE) {
        return 10000;
    }
    if (move.flags | MOVE_CAPTURE) {
        uint8_t own = (uint8_t)ChessGame::piece_from_piece(game.board[move.from]);
        uint8_t enemy = (uint8_t)ChessGame::piece_from_piece(game.board[move.to]);
        return ChessGame::Evaluation::pieceValues[own] + ChessGame::Evaluation::pieceValues[enemy];
    }

    return 0;
}

uint32_t find_best(ChessGame::Game &game, ChessGame::MoveList &moves) {
    int32_t best = 0;
    int32_t bestScore = lossValue;
    for (uint32_t i = 0; i < moves.size(); i++) {
        int32_t score = score_move(game, moves[i]);
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

int32_t alphaBetaMO(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth) {
    nodeCount++;
    if (depth == 0) {
        return ChessGame::signedColor[game.color] * ChessGame::Evaluation::evaluate(game);
    }
    int32_t bestValue = lossValue;

    ChessGame::MoveList moves;
    game.pseudoLegalMoves(moves);
    bool legalMove = false;

    while (!moves.empty()) {
        uint32_t bestMove = find_best(game, moves);
        ChessGame::Move move = moves[bestMove];
        moves.remove_unordered(bestMove);

        game.playMove(move);
        if (game.isCheck(game.color)) {
            game.undoMove(move);
            continue;
        }
        legalMove = true;
        int32_t score = -alphaBetaMO(game, -beta, -alpha, depth - 1);
        game.undoMove(move);
        if (score > bestValue) {
            bestValue = score;
            if (score > alpha) {
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

TranspositionTable table = {0};

int32_t alphaBetaMOTT(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth) {
    nodeCount++;

    uint64_t entryIdx = game.hash % table.size();
    uint8_t bestMove = -1;

    struct TableEntry &entry = table[entryIdx];

    if (entry.type != NodeType::NONE && entry.hash == game.hash) {
        if (entry.depth >= depth) {
            if (entry.type == NodeType::EXACT) {
                return entry.score;
            } else if (entry.type == NodeType::LOWER_BOUND && entry.score >= beta) {
                return entry.score;
            } else if (entry.type == NodeType::UPPER_BOUND && entry.score <= alpha) {
                return entry.score;
            }
        }
        bestMove = entry.best;
    }

    if (depth == 0) {
        return ChessGame::signedColor[game.color] * ChessGame::Evaluation::evaluate(game);
    }

    int32_t bestScore = lossValue;
    int32_t origAlpha = alpha;

    ChessGame::MoveList moves;
    game.pseudoLegalMoves(moves);
    bool legalMove = false;

    while (!moves.empty()) {
        uint8_t moveIdx = find_best(game, moves);
        ChessGame::Move move = moves[moveIdx];
        moves.remove_unordered(moveIdx);

        game.playMove(move);
        if (game.isCheck(game.color)) {
            game.undoMove(move);
            continue;
        }
        legalMove = true;
        int32_t score = -alphaBetaMO(game, -beta, -alpha, depth - 1);
        game.undoMove(move);
        if (score > bestScore) {
            bestScore = score;
            bestMove = moveIdx;
            if (score > alpha) {
                alpha = score;
            }
        }
        if (score >= beta) {
            break;
        }
    }

    if (!legalMove) {
        if (game.isCheck(!game.color)) {
            bestScore = lossValue;
        } else {
            bestScore = 0;
        }
    }

    entry.score = 0;
    if (bestScore <= origAlpha) {
        entry.type = NodeType::UPPER_BOUND;
    } else if (entry.score >= beta) {
        entry.type = NodeType::LOWER_BOUND;
    } else {
        entry.type = NodeType::EXACT;
    }

    entry.depth = depth;
    entry.best = bestMove;
    entry.hash = game.hash;

    return bestScore;
}

} // namespace Minimax
