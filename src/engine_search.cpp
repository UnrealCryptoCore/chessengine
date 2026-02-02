#include "engine_search.h"
#include "evaluation.h"
#include "game.h"
#include "uci.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace Mondfisch::Search {

bool is_mate(Score score) { return std::abs(score) > mate_threshold; }

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
        if (bool(entry.depth)) {
            count++;
        }
    }
    return count * 1000 / table.size();
}

void TranspositionTable::clear() { memset(&table[0], 0, sizeof(TableEntry) * table.size()); }

int16_t score_to_tt(int16_t score, int ply) {
    if (score > mate_threshold) {
        return score + ply;
    }
    if (score < -mate_threshold) {
        return score - ply;
    }
    return score;
}

int16_t score_from_tt(int16_t score, int ply) {
    if (score > mate_threshold) {
        return score - ply;
    }
    if (score < -mate_threshold) {
        return score + ply;
    }
    return score;
}

inline bool TranspositionTable::probe(uint64_t hash, TableEntry &entry, uint8_t ply) const {
    entry = get(hash);
    if (!bool(entry.depth) || entry.hash != hash) {
        return false;
    }
    entry.score = score_from_tt(entry.score, ply);
    return true;
}

inline void TranspositionTable::update(uint64_t hash, uint8_t gen, uint32_t depth, Move bestMove,
                                       Score bestScore, NodeType flag, uint8_t ply) {
    TableEntry &entry = get(hash);
    bestScore = score_to_tt(bestScore, ply);
    if (depth >= entry.depth || entry.age() != gen) {
        entry.score = bestScore;
        entry.best = bestMove;
        entry.depth = depth;
        entry.hash = hash;
        entry.gen = uint8_t(flag) | gen;
    }
}

void SearchContext::reset() {
    thinkingTime = 0;
    table = nullptr;
    resetSearch();
}

void SearchContext::resetSearch() {
    stop = false;
    nodes = 0;
    gen = (gen + 1) & gen_mask;
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

Score score_move(SearchContext &ctx, Game &game, Move move) {
    Score score = 0;
    if (move.promote == Piece::QUEEN) {
        score = 20000;
    } else if (move.promote != Piece::NONE) {
        score = 13000;
    }

    if (move.flags == MoveType::MOVE_CAPTURE) {
        Score see = game.see(move.from, move.to, game.color);
        if (see >= 0) {
            return 16000 + see + score;
        } else {
            return 16000 - see + score;
        }
    }
    if (score > 0) {
        return score;
    }

    return ctx.history[game.color][move.from][move.to];
}

void score_moves(SearchContext &ctx, Game &game, MoveList &moves) {
    for (auto &move : moves) {
        move.score = score_move(ctx, game, move.move);
    }
}

void sort_moves(MoveList &moves) {
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

uint32_t find_best(SearchContext &ctx, Game &game, MoveList &moves) {
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

inline Move find_next(Game &game, MoveList &moves, uint8_t idx) {
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

inline ScoreMove find_next_rm(Game &game, MoveList &moves) {
    int32_t best = 0;
    Score bestScore = -mate;
    for (uint32_t i = 0; i < moves.size(); i++) {
        if (moves[i].score > bestScore) {
            bestScore = moves[i].score;
            best = i;
        }
    }
    ScoreMove move = moves[best];
    moves.remove_unordered(best);
    return move;
}

inline void push_move_to_front(MoveList &moves, Move move) {
    for (uint8_t i = 0; i < moves.size(); i++) {
        if (moves[i].move != move) {
            continue;
        }
        ScoreMove tmp = moves[i];
        for (uint8_t j = 1; j < i; j++) {
            moves[i - j + 1] = moves[i - j];
        }
        moves[0] = tmp;
        return;
    }
}

inline void set_move_score(MoveList &moves, Move move, Score value) {
    for (uint8_t i = 0; i < moves.size(); i++) {
        if (moves[i].move == move) {
            moves[i].score = value;
            break;
        }
    }
}

void update_history(SearchContext &ctx, uint8_t color, Position from, Position to, int32_t bonus) {
    int32_t clampedBonus = std::clamp(bonus, -max_history, max_history);
    ctx.history[color][from][to] +=
        clampedBonus - ctx.history[color][from][to] * std::abs(clampedBonus) / max_history;
}

bool inline is_killer(SearchContext &ctx, uint8_t ply, Move move) {
    return move == ctx.killers[ply][0] || move == ctx.killers[ply][1];
}

Score search(SearchContext &ctx, Game &game, int32_t alpha, int32_t beta, int32_t depth,
             int32_t ply, bool allowNullMove) {
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

    bool check = game.is_check(game.color);

    // null move
    if (allowNullMove && depth >= 3 && !check && game.has_non_pawn_material(game.color)) {
        constexpr int R = 2;

        game.make_null_move();
        Score score = -search(ctx, game, -beta, -beta + 1, depth - 1 - R, ply + 1, false);
        game.undo_null_move();

        if (score >= beta) {
            return score;
        }
    }

    int32_t origAlpha = alpha;
    NodeType flag = NodeType::UPPER_BOUND;
    int32_t bestScore = -max_value;
    uint8_t legalMoves = 0;
    Move bestMove{};

    TableEntry entry;
    bool validTE = ctx.table->probe(game.hash, entry, ply);
    if (validTE) {
        if (entry.depth >= depth && !(is_mate(entry.score) && (entry.age() != ctx.gen))) {
            NodeType type = entry.type();
            if (type == NodeType::EXACT) {
                return entry.score;
            } else if (type == NodeType::LOWER_BOUND && entry.score >= beta) {
                return entry.score;
            } else if (type == NodeType::UPPER_BOUND && entry.score <= alpha) {
                return entry.score;
            }
        }

        if (game.is_pseudo_legal(entry.best)) {
            game.make_move(entry.best);
            if (!game.is_check(!game.color)) {
                bestScore = -search(ctx, game, -beta, -alpha, depth - 1, ply + 1, true);
                if (bestScore >= beta) {
                    game.undo_move(entry.best);
                    ctx.table->update(game.hash, ctx.gen, depth, entry.best, bestScore,
                                      NodeType::LOWER_BOUND, ply);
                    return bestScore;
                }
                if (bestScore > alpha) {
                    alpha = bestScore;
                    flag = NodeType::EXACT;
                }
                bestMove = entry.best;
                legalMoves++;
            }
            game.undo_move(bestMove);
        }
    }

    MoveList moves;

    game.pseudo_legal_moves(moves);
    score_moves(ctx, game, moves);

    // killer moves
    for (uint8_t i = 0; i < 2; i++) {
        set_move_score(moves, ctx.killers[ply][i], mate / 2 - i);
    }

    sort_moves(moves);
    for (uint8_t i = 0; i < moves.size(); i++) {
        Move move = moves[i].move;
        if (move == entry.best) {
            continue;
        }

        game.make_move(move);
        if (game.is_check(!game.color)) {
            game.undo_move(move);
            continue;
        }

        int8_t reduction = 0;
        Score score;
        if (legalMoves == 0) {
            score = -search(ctx, game, -beta, -alpha, depth - 1, ply + 1, true);
        } else {
            bool canReduce = depth >= 3 && legalMoves >= 4 && !check;
            if (move.is_tactical() || is_killer(ctx, ply, move)) {
                canReduce = false;
            }
            if (canReduce) {
                reduction = 1.0 + std::log(depth) * std::log(legalMoves) / 3;
                reduction += bool(ctx.history[!game.color][move.from][move.to] < 0);
            }
            score = -search(ctx, game, -alpha - 1, -alpha, depth - 1 - reduction, ply + 1, true);
            if (score > alpha && reduction > 0) {
                score = -search(ctx, game, -alpha - 1, -alpha, depth - 1, ply + 1, true);
            }
            if (score > alpha && score < beta) {
                score = -search(ctx, game, -beta, -alpha, depth - 1, ply + 1, true);
            }
        }

        if (score > alpha) {
            alpha = score;
            flag = NodeType::EXACT;
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
                    ctx.killers[ply][1] = ctx.killers[ply][0];
                    ctx.killers[ply][0] = move;
                }

                // update history heuristic
                update_history(ctx, game.color, move.from, move.to, depth * depth);
                // penalize other quiet moves
                for (uint8_t j = 0; j < i; j++) {
                    Move quietMove = moves[j].move;
                    if (quietMove.is_capture()) {
                        continue;
                    }
                    if (quietMove == entry.best) {
                        continue;
                    }
                    if (is_killer(ctx, ply, quietMove)) {
                        continue;
                    }
                    update_history(ctx, game.color, quietMove.from, quietMove.to, -depth * depth);
                }
            }
            flag = NodeType::LOWER_BOUND;
            break;
        }
    }

    if (legalMoves == 0) {
        if (check) {
            bestScore = -mate + ply;
        } else {
            bestScore = 0;
        }
        return bestScore;
    }

    if (ctx.stop) {
        return 0;
    }

    ctx.table->update(game.hash, ctx.gen, depth, bestMove, bestScore, flag, ply);
    return bestScore;
}

Score search_root(Search::SearchContext &ctx, Game &game, uint32_t depth) {
    ctx.nodes++;
    Score alpha = -mate;
    Score beta = mate;
    uint64_t entryIdx = game.hash & (ctx.table->size() - 1);
    Move bestMove;

    TableEntry &entry = ctx.table->table[entryIdx];
    if (bool(entry.depth) && entry.hash == game.hash) {
        push_move_to_front(ctx.moves, entry.best);
    }

    Score bestScore = -max_value;
    Score origAlpha = alpha;

    for (uint8_t i = 0; i < ctx.moves.size(); i++) {
        ScoreMove &move = ctx.moves[i];
        game.make_move(move.move);

        Score score;
        if (i == 0) {
            score = -search(ctx, game, -mate, mate, depth - 1, 1, true);
        } else {
            score = -search(ctx, game, -alpha - 1, -alpha, depth - 1, 1, true);
            if (score > alpha && score < beta) {
                score = -search(ctx, game, -mate, mate, depth - 1, 1, true);
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
        entry.gen = (uint8_t(NodeType::EXACT) << node_shift) | ctx.gen;
    }
    return bestScore;
}

Score quiescence(SearchContext &ctx, Game &game, Score alpha, Score beta) {
    ctx.nodes++;

    if (ctx.stop) {
        return 0;
    }

    if ((ctx.nodes & 2047) == 0 && ctx.timeUp()) {
        ctx.stop = true;
    }

    Score static_eval = signedColor[game.color] * Evaluation::tapered_eval(game);
    Score best_value = static_eval;
    if (best_value > beta) {
        return best_value;
    }
    if (best_value > alpha) {
        alpha = best_value;
    }

    MoveList moves;
    game.pseudo_legal_captures(moves);
    score_moves(ctx, game, moves);

    while (moves.size() > 0) {
        Move move = find_next_rm(game, moves).move;
        if (game.see(move.from, move.to, game.color) < 0) {
            continue;
        }
        game.make_move(move);
        if (game.is_check(!game.color)) {
            game.undo_move(move);
            continue;
        }
        Score score = -quiescence(ctx, game, -beta, -alpha);
        game.undo_move(move);
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

void calculate_pv_moves(SearchContext &ctx, Game &game, std::vector<Move> &moves, int8_t depth) {
    auto move = moves.back();
    game.make_move(move);

    if (game.is_draw()) {
        game.undo_move(move);
        return;
    }

    Search::TableEntry &entry = ctx.table->get(game.hash);
    if (entry.hash != game.hash) {
        game.undo_move(move);
        return;
    }

    if (!game.is_pseudo_legal(entry.best)) {
        return;
    }

    moves.push_back(entry.best);
    if (depth > 0) {
        calculate_pv_moves(ctx, game, moves, depth - 1);
    }

    game.undo_move(move);
}

Search::SearchResult iterative_deepening(SearchContext &ctx, Game &game, uint32_t depth) {
    ctx.resetSearch();
    Search::SearchResult lastResult;
    auto start = ctx.timeStart;

    game.legal_moves(ctx.moves);

    for (uint32_t i = 1; i <= depth; i++) {
        Search::search_root(ctx, game, i);

        if (ctx.stop) {
            break;
        }

        Search::sort_moves(ctx.moves);
        ScoreMove bestMove = ctx.moves[0];

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::vector<Move> pvs{ctx.moves[0].move};
        calculate_pv_moves(ctx, game, pvs, i);
        Search::SearchResult result{
            .score = bestMove.score,
            .bestMove = bestMove.move,
            .nodes = ctx.nodes,
            .pv = pvs,
            .depth = i,
            .elapsed = elapsed,
        };
        IO::sendSearchInfo(result, ctx.table->hashFull());

        start = end;
        lastResult = result;

        if (Search::is_mate(result.score)) {
            break;
        }

        ctx.history_decay();
    }
    return lastResult;
}
} // namespace Mondfisch::Search
