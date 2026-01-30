#pragma once

#include "game.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace Search {

using Score = int16_t;

constexpr uint8_t max_depth = 64;
constexpr Score mate = 30000;
constexpr Score mate_threshold = 29000;
constexpr Score max_value = 32000;
constexpr Score loss_value = -mate;

constexpr int32_t max_history = 10000;

enum class NodeType : uint8_t {
    NONE = 0,
    EXACT,
    UPPER_BOUND,
    LOWER_BOUND,
};

struct TableEntry {
    uint64_t hash;
    ChessGame::Move best;
    Score score;
    uint8_t depth;
    NodeType type;
};

struct SearchResult {
    Score score;
    ChessGame::Move bestMove;
    uint64_t nodes;
    std::vector<ChessGame::Move> pv;
    uint32_t depth;
    int64_t elapsed;
};

struct TranspositionTable {
    std::vector<TableEntry> table;

    void setsize(uint32_t mb);
    size_t size() const { return table.size(); }
    void clear();

    TableEntry &get(uint64_t hash) { return table[hash & (table.size() - 1)]; }
    const TableEntry &get(uint64_t hash) const { return table[hash & (table.size() - 1)]; }

    TableEntry &operator[](size_t i) { return table[i]; }
    const TableEntry &operator[](size_t i) const { return table[i]; }

    uint32_t hashFull() const;
};

struct StackElement {
    uint16_t ply = 0;
    bool allowNullMove = 0;
};

struct SearchContext {
    bool stop = false;
    uint64_t thinkingTime = 0;
    uint64_t nodes = 0;
    std::chrono::steady_clock::time_point timeStart;
    uint32_t ply = 0;
    TranspositionTable *table = nullptr;
    std::array<std::array<ChessGame::Move, 2>, max_depth> killers{};
    std::array<std::array<std::array<int32_t, 64>, 64>, 2> history{};
    ChessGame::MoveList moves;
    //ChessGame::StackList<StackElement, max_depth> stack{};

    void reset();
    void resetSearch();
    void history_decay();
    void startTimer();
    bool timeUp() const;
};

bool is_mate(Score score);

void score_moves(SearchContext &ctx, ChessGame::Game &game, ChessGame::MoveList &moves);

void sort_moves(ChessGame::MoveList &moves);

Score simpleMinimax(ChessGame::Game &game, int32_t depth);

Score simpleAlphaBeta(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

Score search(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

Score search(SearchContext &ctx, ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth, bool allowNullMove);

Score quiescence(SearchContext &ctx, ChessGame::Game &game, Score alpha, Score beta);

Score test_search_root(SearchContext &ctx, ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

Score search_root(Search::SearchContext &ctx, ChessGame::Game &game, uint32_t depth);

} // namespace Search
