#include "engine_search.h"
#include "evaluation.h"
#include "game.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Search {

void TranspositionTable::setsize(uint32_t mb) {
    size_t entries = (1014 * 1024 * mb) / sizeof(TableEntry);
    size_t pow2 = 1;
    while (pow2 * 2 <= entries) {
        pow2 *= 2;
    }
    table.resize(pow2);
    clear();
}

uint32_t TranspositionTable::hashFull() const {
    uint64_t count = 0;
    for (auto entry : table) {
        if (entry.type != NodeType::NONE) {
            count++;
        }
    }
    return count * 1000 / table.size();
}

void TranspositionTable::clear() {
    //std::fill(table.begin(), table.end(), TableEntry{});
    memset(&table[0], 0, sizeof(TableEntry) * table.size());
}

void SearchContext::reset() {
    thinkingTime = 0;
    table = nullptr;
    resetSearch();
}

void SearchContext::resetSearch() {
    stop = false;
    nodes = 0;
    if (table != nullptr) {
        table->clear();
    }
}

void SearchContext::startTimer() { timeStart = std::chrono::steady_clock::now(); }

bool SearchContext::timeUp() const {
    if (thinkingTime == 0) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeStart);
    return elapsed.count() > thinkingTime;
}

Score simpleMinimax(ChessGame::Game &game, int32_t depth) {
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

Score simpleAlphaBeta(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth) {
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

Score score_move(ChessGame::Game &game, ChessGame::Move move) {
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

Score alphaBetaMO(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth) {
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
            return lossValue - depth;
        } else {
            return 0;
        }
    }
    return bestValue;
}

inline void swap(ChessGame::MoveList &moves, uint8_t a, uint8_t b) {
    ChessGame::Move tmp = moves[a];
    moves[a] = moves[b];
    moves[b] = tmp;
}

inline ChessGame::Move &find_next(ChessGame::Game &game, ChessGame::MoveList &moves, uint8_t idx) {
    int32_t best = 0;
    Score bestScore = -mate;
    for (uint32_t i = idx; i < moves.size(); i++) {
        Score score = score_move(game, moves[i]);
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    swap(moves, best, idx);
    return moves[idx];
}

Score alphaBetaMOTT(SearchContext &ctx, ChessGame::Game &game, int32_t alpha, int32_t beta,
                    int32_t depth) {
    ctx.nodes++;
    if (ctx.stop) {
        return 0;
    }

    if ((ctx.nodes & 2047) == 0 && ctx.timeUp()) {
        ctx.stop = true;
    }

    if (depth == 0) {
        return ChessGame::signedColor[game.color] * ChessGame::Evaluation::evaluate(game);
    }

    uint64_t entryIdx = game.hash & (ctx.table->size() - 1);
    ChessGame::Move bestMove;
    ChessGame::MoveList moves;
    uint8_t moveIdx = 0;
    uint8_t offset = 0;

    game.pseudoLegalMoves(moves);

    TableEntry &entry = ctx.table->table[entryIdx];
    if (entry.type != NodeType::NONE && entry.hash == game.hash) {
        if (entry.depth >= depth) {
            if (entry.type == NodeType::EXACT) {
                return entry.score;
            } else if (entry.type == NodeType::LOWER_BOUND && entry.score >= beta) {
                return beta;
            } else if (entry.type == NodeType::UPPER_BOUND && entry.score <= alpha) {
                return alpha;
            }
        }
        for (uint8_t i = 0; i < moves.size(); i++) {
            if (moves[i] == entry.best) {
                swap(moves, 0, i);
                offset++;
                break;
            }
        }
    }

    int32_t bestScore = -maxValue;
    int32_t origAlpha = alpha;

    bool legalMove = false;

    for (moveIdx = 0; moveIdx < moves.size(); moveIdx++) {
        ChessGame::Move move;
        if (offset > 0) {
            offset--;
            move = moves[moveIdx];
        } else {
            move = find_next(game, moves, moveIdx);
        }

        game.playMove(move);
        if (game.isCheck(game.color)) {
            game.undoMove(move);
            continue;
        }
        legalMove = true;
        Score score = -alphaBetaMOTT(ctx, game, -beta, -alpha, depth - 1);
        game.undoMove(move);
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
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
            bestScore = -mate - depth;
            return bestScore;
        } else {
            bestScore = 0;
            return bestScore;
        }
    }

    if (depth >= entry.depth && !ctx.stop) {
        entry.score = bestScore;
        entry.best = bestMove;
        entry.depth = depth;
        entry.hash = game.hash;
        if (bestScore <= origAlpha) {
            entry.type = NodeType::UPPER_BOUND;
        } else if (entry.score >= beta) {
            entry.type = NodeType::LOWER_BOUND;
        } else {
            entry.type = NodeType::EXACT;
        }
    }

    return bestScore;
}

} // namespace Search
