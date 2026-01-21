#pragma once

#include "game.h"
#include <cstdint>
#include <vector>
#include <chrono>

namespace Search {

typedef int16_t Score;

inline uint32_t nodeCount = 0;
inline uint32_t TTHits = 0;
inline uint32_t usableTThits = 0;

constexpr Score mate = 30000;
constexpr Score maxValue = 32000;
constexpr Score lossValue = -mate;

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

struct SearchContext {
    bool stop = false;
    uint64_t thinkingTime = 0;
    uint64_t nodes = 0;
    std::chrono::steady_clock::time_point timeStart;

    TranspositionTable *table = nullptr;

    void reset();
    void resetSearch();
    void startTimer();
    bool timeUp() const;
};

Score simpleMinimax(ChessGame::Game &game, int32_t depth);

Score simpleAlphaBeta(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

Score alphaBetaMO(ChessGame::Game &game, int32_t alpha, int32_t beta, int32_t depth);

Score alphaBetaMOTT(SearchContext &ctx, ChessGame::Game &game, int32_t alpha, int32_t beta,
                    int32_t depth);

} // namespace Search
