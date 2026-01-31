#include "engine_search.h"
#include "evaluation.h"
#include "game.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace Search {

bool is_mate(Score score) { return score > mate_threshold || score < -mate_threshold; }

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

void TranspositionTable::clear() { memset(&table[0], 0, sizeof(TableEntry) * table.size()); }

void SearchContext::reset() {
    thinkingTime = 0;
    table = nullptr;
    resetSearch();
}

void SearchContext::resetSearch() {
    stop = false;
    nodes = 0;
    ply = 0;
    moves.clear();
    memset(&history[0], 0, sizeof(history));
    // stack.clear();
}

void SearchContext::history_decay() {
    for (uint16_t i = 0; i < 64; i++) {
        for (uint16_t j = 0; j < 64; j++) {
            history[0][i][j] /= 2;
            history[1][i][j] /= 2;
        }
    }
}

void SearchContext::startTimer() { timeStart = std::chrono::steady_clock::now(); }

bool SearchContext::timeUp() const {
    if (thinkingTime <= 0) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timeStart);
    return elapsed.count() > thinkingTime;
}

Score score_move(SearchContext &ctx, ChessGame::Game &game, ChessGame::Move move) {
    if (move.promote != ChessGame::Piece::NONE) {
        return ChessGame::Evaluation::pieceValues[(uint8_t)move.promote] * 10;
    }
    uint8_t own = (uint8_t)ChessGame::piece_from_piece(game.board[move.from]);
    if (move.flags == ChessGame::MoveType::MOVE_CAPTURE) {
        uint8_t enemy = (uint8_t)ChessGame::piece_from_piece(game.board[move.to]);
        return ChessGame::Evaluation::pieceValues[enemy] * 10 -
               ChessGame::Evaluation::pieceValues[own];
    }

    return ctx.history[game.color][move.from][move.to];
}

void score_moves(SearchContext &ctx, ChessGame::Game &game, ChessGame::MoveList &moves) {
    for (auto &move : moves) {
        move.score = score_move(ctx, game, move.move);
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

uint32_t find_best(SearchContext &ctx, ChessGame::Game &game, ChessGame::MoveList &moves) {
    uint32_t best = 0;
    int32_t bestScore = -max_value;
    for (uint32_t i = 0; i < moves.size(); i++) {
        int32_t score = score_move(ctx, game, moves[i].move);
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
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

void update_history(SearchContext &ctx, uint8_t color, ChessGame::Position from,
                    ChessGame::Position to, int32_t bonus) {
    int32_t clampedBonus = std::clamp(bonus, -max_history, max_history);
    ctx.history[color][from][to] +=
        clampedBonus - ctx.history[color][from][to] * std::abs(clampedBonus) / max_history;
}

void inline update_TT(SearchContext &ctx, ChessGame::Game &game, uint32_t entryIdx, uint32_t depth,
                      ChessGame::Move bestMove, Score bestScore, Score alpha, Score beta) {
    TableEntry &entry = ctx.table->table[entryIdx];
    if (depth >= entry.depth && !ctx.stop) {
        entry.score = bestScore;
        entry.best = bestMove;
        entry.depth = depth;
        entry.hash = game.hash;
        if (bestScore <= alpha) {
            entry.type = NodeType::UPPER_BOUND;
        } else if (bestScore >= beta) {
            entry.type = NodeType::LOWER_BOUND;
        } else {
            entry.type = NodeType::EXACT;
        }
    }
}

Score search(SearchContext &ctx, ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth,
             bool allowNullMove) {
    ctx.nodes++;

    if (ctx.stop) {
        return 0;
    }

    if ((ctx.nodes & 2047) == 0 && ctx.timeUp()) {
        ctx.stop = true;
    }

    // check for draw
    if (game.is_draw()) {
        return 0;
    }

    if (depth <= 0) {
        return quiescence(ctx, game, alpha, beta);
    }

    // null move
    if (allowNullMove && depth >= 3 && !game.is_check(game.color) &&
        game.has_non_pawn_material(game.color)) {
        constexpr int R = 2;

        game.make_null_move();
        Score score = -search(ctx, game, -beta, -beta + 1, depth - 1 - R, false);
        game.undo_null_move();

        if (score >= beta) {
            return score;
        }
    }

    uint64_t entryIdx = game.hash & (ctx.table->size() - 1);
    ChessGame::Move bestMove{};
    ChessGame::MoveList moves;

    game.pseudo_legal_moves(moves);
    score_moves(ctx, game, moves);

    // Transposition Table move first
    TableEntry &entry = ctx.table->table[entryIdx];
    uint16_t entryDepth = entry.depth;
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
        set_move_score(moves, entry.best, mate);
    }

    ctx.ply++;

    // killer moves
    for (uint8_t i = 0; i < 2; i++) {
        set_move_score(moves, ctx.killers[ctx.ply][i], mate / 2 - i);
    }

    int32_t bestScore = -max_value;
    int32_t origAlpha = alpha;

    uint8_t legalMoves = 0;

    sort_moves(moves);
    for (uint8_t i = 0; i < moves.size(); i++) {
        ChessGame::Move move = moves[i].move;

        game.make_move(move);
        if (game.is_check(!game.color)) {
            game.undo_move(move);
            continue;
        }

        int8_t reduction = 0;
        Score score;
        if (legalMoves == 0) {
            score = -search(ctx, game, -beta, -alpha, depth - 1, true);
        } else {
            if (depth >= 3 && legalMoves >= 3 && !move.is_capture() && !game.is_check(game.color) &&
                ctx.history[!game.color][move.from][move.to] < 0) {
                reduction = 1;
            }
            score = -search(ctx, game, -alpha - 1, -alpha, depth - 1 - reduction, true);
            if (score > alpha && reduction > 0) {
                score = -search(ctx, game, -alpha - 1, -alpha, depth - 1, true);
            }
            if (score > alpha && score < beta) {
                score = -search(ctx, game, -beta, -alpha, depth - 1, true);
            }
        }

        if (score > alpha) {
            alpha = score;
        }

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }

        game.undo_move(move);
        legalMoves++;

        if (score >= beta) {
            if (!move.is_capture()) {
                // update killer moves
                if (legalMoves > 1) {
                    ctx.killers[ctx.ply][1] = ctx.killers[ctx.ply][0];
                    ctx.killers[ctx.ply][0] = move;
                }

                // update history heuristic
                update_history(ctx, game.color, move.from, move.to, depth * depth);
                for (uint8_t j = 0; j < i; j++) {
                    ChessGame::Move quietMove = moves[j].move;
                    if (quietMove.is_capture()) {
                        continue;
                    }
                    update_history(ctx, game.color, quietMove.from, quietMove.to, -depth * depth);
                }
            }
            break;
        }
    }

    ctx.ply--;

    if (legalMoves == 0) {
        if (game.is_check(game.color)) {
            bestScore = -mate + ctx.ply;
        } else {
            bestScore = 0;
        }
        return bestScore;
    }

    update_TT(ctx, game, entryIdx, depth, bestMove, bestScore, origAlpha, beta);
    return bestScore;
}

Score search_root(Search::SearchContext &ctx, ChessGame::Game &game, uint32_t depth) {
    ctx.ply = 1;
    ctx.nodes++;
    Score alpha = -mate;
    Score beta = mate;
    uint64_t entryIdx = game.hash & (ctx.table->size() - 1);
    ChessGame::Move bestMove;

    TableEntry &entry = ctx.table->table[entryIdx];
    if (entry.type != NodeType::NONE && entry.hash == game.hash) {
        push_move_to_front(ctx.moves, entry.best);
    }

    Score bestScore = -max_value;
    Score origAlpha = alpha;

    for (uint8_t i = 0; i < ctx.moves.size(); i++) {
        ChessGame::ScoreMove &move = ctx.moves[i];
        game.make_move(move.move);

        Score score;
        if (i == 0) {
            score = -search(ctx, game, -mate, mate, depth - 1, true);
        } else {
            score = -search(ctx, game, -alpha - 1, -alpha, depth - 1, true);
            if (score > alpha && score < beta) {
                score = -search(ctx, game, -mate, mate, depth - 1, true);
            }
        }

        game.undo_move(move.move);

        if (ctx.stop) {
            return 0;
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

    update_history(ctx, !game.color, bestMove.from, bestMove.to, depth * depth);

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
    score_moves(ctx, game, moves);

    while (moves.size() > 0) {
        ChessGame::ScoreMove move = find_next_rm(game, moves);
        if (!move.move.is_capture() && move.move.promote == ChessGame::Piece::NONE) {
            continue;
        }
        game.make_move(move.move);
        if (game.is_check(!game.color)) {
            game.undo_move(move.move);
            continue;
        }
        Score score = -quiescence(ctx, game, -beta, -alpha);
        game.undo_move(move.move);
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
