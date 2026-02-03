#pragma once

#include "game.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace Mondfisch::Search {

using Score = int16_t;

constexpr uint8_t max_depth = 64;
constexpr Score mate = 30000;
constexpr Score mate_threshold = 29000;
constexpr Score max_value = 32000;
constexpr Score loss_value = -mate;

constexpr int32_t max_history = 10000;

constexpr uint8_t node_shift = 6;
constexpr uint8_t gen_mask = 0b00111111;
constexpr uint8_t node_mask = 0b11000000;

enum class NodeType : uint8_t {
    EXACT = 1 << node_shift,
    UPPER_BOUND = 2 << node_shift,
    LOWER_BOUND = 3 << node_shift,
};

struct TableEntry {
    uint64_t hash;
    Move best;
    Score score;
    uint8_t depth;
    uint8_t gen;

    inline uint8_t age() const { return gen & gen_mask; }

    inline NodeType type() const { return NodeType(gen & node_mask); }
};

struct SearchResult {
    Score score;
    Move bestMove;
    uint64_t nodes;
    std::vector<Move> pv;
    uint32_t depth;
    int64_t elapsed;
};

struct TranspositionTable {
    std::vector<TableEntry> table;

    void setsize(uint32_t mb);
    size_t size() const { return table.size(); }
    void clear();

    inline void update(uint64_t hash, uint8_t gen, uint32_t depth, Move bestMove, Score bestScore,
                       NodeType flag, uint8_t ply);
    inline bool probe(uint64_t hash, TableEntry &entry, uint8_t ply) const;

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
    uint8_t gen = 0;
    TranspositionTable *table = nullptr;
    std::array<std::array<Move, 2>, max_depth> killers{};
    std::array<std::array<std::array<int32_t, 64>, 64>, 2> history{};
    MoveList moves;
    // StackList<StackElement, max_depth> stack{};

    void reset();
    void resetSearch();
    void history_decay();
    void startTimer();
    bool timeUp() const;
};

bool is_mate(Score score);

void score_moves(SearchContext &ctx, Game &game, MoveList &moves);

void sort_moves(MoveList &moves);

Score search(SearchContext &ctx, Game &game, int32_t alpha, int32_t beta, int32_t depth,
             int32_t ply, bool allowNullMove);

Score quiescence(SearchContext &ctx, Game &game, Score alpha, Score beta);

Score test_search_root(SearchContext &ctx, Game &game, int32_t alpha, int32_t beta, int32_t depth);

Score search_root(SearchContext &ctx, Game &game, Score alpha, Score beta, int32_t depth);

void calculate_pv_moves(SearchContext &ctx, Game &game, std::vector<Move> &moves, int8_t depth);

SearchResult iterative_deepening(SearchContext &ctx, Game &game, uint32_t depth);
} // namespace Mondfisch::Search
