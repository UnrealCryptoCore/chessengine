#include "engine_search.h"
#include "evaluation.h"
#include "game.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <print>

namespace Search {

bool is_mate(Score score) { return score > mate || score < -mate; }

Score see(ChessGame::Game &game, ChessGame::Position pos, uint8_t color) {
    Score gain[64];
    int32_t value = 0;
    ChessGame::BitBoard attackers[2];
    attackers[WHITE] = game.squareAttackers(pos, BLACK);
    attackers[BLACK] = game.squareAttackers(pos, WHITE);
    uint8_t side = color;
    while (1) {
        ChessGame::BitBoard attack = attackers[color];
        ChessGame::Position from = game.get_lva(attackers[side], side);
    }

    return value;
}

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
    // std::fill(table.begin(), table.end(), TableEntry{});
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
        //table->clear();
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
    game.pseudo_legal_moves(moves);
    bool legalMove = false;
    int32_t value = lossValue;
    for (auto move : moves) {
        game.playMove(move.move);
        if (game.is_check(game.color)) {
            game.undoMove(move.move);
            continue;
        }
        legalMove = true;
        int32_t tmpVal = -simpleMinimax(game, depth - 1);
        if (tmpVal > value) {
            value = tmpVal;
        }
        game.undoMove(move.move);
    }

    if (!legalMove) {
        if (game.is_check(!game.color)) {
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
    game.pseudo_legal_moves(moves);
    bool legalMove = false;

    for (auto move : moves) {
        game.playMove(move.move);
        if (game.is_check(game.color)) {
            game.undoMove(move.move);
            continue;
        }
        legalMove = true;
        int32_t score = -simpleAlphaBeta(game, -beta, -alpha, depth - 1);
        game.undoMove(move.move);
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
        if (game.is_check(!game.color)) {
            return lossValue;
        } else {
            return 0;
        }
    }
    return bestValue;
}

Score score_move(ChessGame::Game &game, ChessGame::Move move) {
    if (move.promote != ChessGame::Piece::NONE) {
        return ChessGame::Evaluation::pieceValues[(uint8_t)move.promote] * 10;
    }
    uint8_t own = (uint8_t)ChessGame::piece_from_piece(game.board[move.from]);
    if (move.flags == ChessGame::MoveType::MOVE_CAPTURE) {
        uint8_t enemy = (uint8_t)ChessGame::piece_from_piece(game.board[move.to]);
        return ChessGame::Evaluation::pieceValues[enemy] * 10 - ChessGame::Evaluation::pieceValues[own];
    }

    return ChessGame::Evaluation::pieceValues[own] / 10;
}

void score_moves(ChessGame::Game &game, ChessGame::MoveList &moves) {
    for (auto &move : moves) {
        move.score = score_move(game, move.move);
    }
}

void sort_moves(ChessGame::MoveList &moves) {
    for (uint16_t i = 0; i < moves.size(); i++) {
        uint16_t best = i;
        for (uint16_t j = i + 1; j < moves.size(); j++) {
            if (moves[j].score > moves[best].score) {
                best = j;
            }
        }
        moves.swap(i, best);
    }
}

uint32_t find_best(ChessGame::Game &game, ChessGame::MoveList &moves) {
    uint32_t best = 0;
    int32_t bestScore = lossValue;
    for (uint32_t i = 0; i < moves.size(); i++) {
        int32_t score = score_move(game, moves[i].move);
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
    game.pseudo_legal_moves(moves);
    bool legalMove = false;

    while (!moves.empty()) {
        uint32_t bestMove = find_best(game, moves);
        ChessGame::ScoreMove move = moves[bestMove];
        moves.remove_unordered(bestMove);

        game.playMove(move.move);
        if (game.is_check(game.color)) {
            game.undoMove(move.move);
            continue;
        }
        legalMove = true;
        int32_t score = -alphaBetaMO(game, -beta, -alpha, depth - 1);
        game.undoMove(move.move);
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
        if (game.is_check(!game.color)) {
            return lossValue - depth;
        } else {
            return 0;
        }
    }
    return bestValue;
}

inline ChessGame::Move find_next(ChessGame::Game &game, ChessGame::MoveList &moves, uint8_t idx) {
    int32_t best = idx;
    Score bestScore = -mate;
    for (uint32_t i = idx; i < moves.size(); i++) {
        if (moves[i].score > bestScore) {
            bestScore = moves[i].score;
            best = i;
        }
    }
    moves.swap(best, idx);
    return moves[idx].move;
}

inline ChessGame::ScoreMove find_next_rm(ChessGame::Game &game, ChessGame::MoveList &moves) {
    int32_t best = 0;
    Score bestScore = -mate;
    for (uint32_t i = 0; i < moves.size(); i++) {
        if (moves[i].score > bestScore) {
            bestScore = moves[i].score;
            best = i;
        }
    }
    ChessGame::ScoreMove move = moves[best];
    moves.remove_unordered(best);
    return move;
}

inline void push_move_to_front(ChessGame::MoveList &moves, ChessGame::Move move) {
    for (uint8_t i = 0; i < moves.size(); i++) {
        if (moves[i].move != move) {
            continue;
        }
        ChessGame::ScoreMove tmp = moves[i];
        for (uint8_t j = 1; j < i; j++) {
            moves[i - j + 1] = moves[i - j];
        }
        moves[0] = tmp;
        return;
    }
}

inline void set_move_score(ChessGame::MoveList &moves, ChessGame::Move move, Score value) {
    for (uint8_t i = 0; i < moves.size(); i++) {
        if (moves[i].move == move) {
            moves[i].score = value;
            break;
        }
    }
}

Score alphaBetaMOTT(SearchContext &ctx, ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth) {
    ctx.nodes++;

    if (ctx.stop) {
        return 0;
    }

    if ((ctx.nodes & 2047) == 0 && ctx.timeUp()) {
        ctx.stop = true;
    }

    if (depth == 0) {
        return quiescence(ctx, game, alpha, beta);
    }

    uint64_t entryIdx = game.hash & (ctx.table->size() - 1);
    ChessGame::Move bestMove;
    ChessGame::MoveList moves;

    game.pseudo_legal_moves(moves);
    score_moves(game, moves);

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
        set_move_score(moves, entry.best, mate);
    }

    int32_t bestScore = -maxValue;
    int32_t origAlpha = alpha;

    bool legalMove = false;

    while (moves.size() > 0) {
        ChessGame::Move move = find_next_rm(game, moves).move;

        game.playMove(move);
        if (game.is_check(game.color)) {
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
        if (game.is_check(!game.color)) {
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

Score search_root(Search::SearchContext &ctx, ChessGame::Game &game, uint32_t depth) {
    ctx.nodes++;
    Score alpha = -mate;
    uint64_t entryIdx = game.hash & (ctx.table->size() - 1);
    ChessGame::Move bestMove;

    TableEntry &entry = ctx.table->table[entryIdx];
    if (entry.type != NodeType::NONE && entry.hash == game.hash) {
        /*if (entry.depth >= depth) {
            if (entry.type == NodeType::EXACT) {
                return entry.score;
            } else if (entry.type == NodeType::LOWER_BOUND && entry.score >= beta) {
                return beta;
            } else if (entry.type == NodeType::UPPER_BOUND && entry.score <= alpha) {
                return alpha;
            }
        }*/
        push_move_to_front(ctx.moves, entry.best);
    }

    int32_t bestScore = -maxValue;
    int32_t origAlpha = alpha;

    bool first = true;
    for (auto &move : ctx.moves) {
        game.playMove(move.move);

        Score score = -alphaBetaMOTT(ctx, game, -mate, mate, depth - 1);
        game.undoMove(move.move);

        if (ctx.stop) {
            break;
        }

        move.score = score;
        if (score > bestScore) {
            bestScore = score;
            bestMove = move.move;
            if (score > alpha) {
                alpha = score;
            }
        }
    }

    if (!ctx.stop) {
        entry.score = bestScore;
        entry.best = bestMove;
        entry.depth = depth;
        entry.hash = game.hash;
        entry.type = NodeType::EXACT;
    }
    return bestScore;
}

Score quiescence(SearchContext &ctx, ChessGame::Game &game, Score alpha, Score beta) {
    ctx.nodes++;

    if (ctx.stop) {
        return 0;
    }

    if ((ctx.nodes & 2047) == 0 && ctx.timeUp()) {
        ctx.stop = true;
    }

    Score static_eval = ChessGame::signedColor[game.color] * ChessGame::Evaluation::evaluate(game);
    Score best_value = static_eval;
    if (best_value > beta) {
        return best_value;
    }
    if (best_value > alpha) {
        alpha = best_value;
    }

    ChessGame::MoveList moves;
    game.pseudo_legal_moves(moves);
    score_moves(game, moves);

    while (moves.size() > 0) {
        ChessGame::ScoreMove move = find_next_rm(game, moves);
        if (move.move.flags != ChessGame::MoveType::MOVE_CAPTURE) {
            continue;
        }
        game.playMove(move.move);
        if (game.is_check(game.color)) {
            game.undoMove(move.move);
            continue;
        }
        Score score = -quiescence(ctx, game, -beta, -alpha);
        game.undoMove(move.move);
        if (score >= beta) {
            return score;
        }
        if (score > best_value) {
            best_value = score;
        }
        if (score > alpha) {
            alpha = score;
        }
    }
    return best_value;
}
} // namespace Search
