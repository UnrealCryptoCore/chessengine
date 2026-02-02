#pragma once

#include "engine_search.h"
#include "game.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <string>

namespace Mondfisch {
constexpr std::string name = "Mondfisch";
constexpr std::string author = "cryptocore";

inline bool debug = false;

enum class OptionType {
    SPIN,
    CHECK,
    COMBO,
    BUTTON,
    STRING,
};

struct Option {
    std::string name;
    OptionType type;
    std::string min = "";
    std::string max = "";
    std::string var = "";
    std::string defaultStr = "";
};

constexpr std::string toString(OptionType type) {
    switch (type) {
    case OptionType::SPIN:
        return "spin";
    case OptionType::CHECK:
        return "check";
    case OptionType::COMBO:
        return "combo";
    case OptionType::BUTTON:
        return "button";
    case OptionType::STRING:
        return "string";
    default:
        return "";
    }
}

struct TimeManagement {
    int64_t wtime = -1;
    int64_t btime = -1;
    int64_t winc = -1;
    int64_t binc = -1;
    int64_t movetime = -1;
};

struct UciEngine {
    Game game{};
    Search::TranspositionTable table{};
    Search::SearchContext ctx{};
    TimeManagement timeValues{};
    int32_t depth = 0;
    uint8_t kBest = 1;

    UciEngine() {
        table.setsize(16);
        ctx.reset();
        new_uci_game();
    }

    void think();
    void new_uci_game();
    void loop();
    uint64_t calc_time();
};

struct Logger {
    std::string filename;
    std::ofstream file;

    void log(const std::string &s) {
        if (!debug) {
            return;
        }
        std::cerr << s << std::endl;
        if (!filename.empty()) {
            file << s << std::endl;
        }
    }

    void init() { file.open(filename, std::ios_base::app); }
    void close() {
        file.flush();
        file.close();
    }
};

struct IO {
    static void send(const std::string &s) {
        std::cout << s << std::endl;
        std::cout.flush();
    }

    static void sendId() {
        send(std::format("id name {}", name));
        send(std::format("id author {}", author));
    }

    static void sendUciOk() { send("uciok"); }

    static void sendOption(const Option &option) {
        std::string res = "option name ";
        res += option.name;
        res += " type " + toString(option.type);
        if (!option.defaultStr.empty()) {
            res += " default " + option.defaultStr;
        }
        if (!option.min.empty()) {
            res += " min " + option.min;
        }
        if (!option.max.empty()) {
            res += " max " + option.max;
        }

        if (!option.var.empty()) {
            res += " var " + option.var;
        }
        send(res);
    }

    static void sendOptions() {
        sendOption(Option{
            .name = "Hash",
            .type = OptionType::SPIN,
            .min = "1",
            .max = "128",
            .defaultStr = "16",
        });
        sendOption(Option{
            .name = "MultiPV",
            .type = OptionType::SPIN,
            .min = "1",
            .max = "256",
            .defaultStr = "1",
        });
    }

    static void sendReadyOk() { send("readyok"); }

    static void sendBestMove(Move move) {
        send(std::format("bestmove {}", move.toSimpleNotation()));
    }

    static void sendSearchInfo(Search::SearchResult &result, uint32_t hashfull) {
        uint32_t nps = result.nodes * 1000.0f / result.elapsed;
        std::string pvs = "";
        for (auto [i, move] : std::views::enumerate(result.pv)) {
            if (i > 0) {
                pvs += " ";
            }
            pvs += move.toSimpleNotation();
        }
        std::string score;
        if (Search::is_mate(result.score)) {
            score = std::format("mate {}", ((Search::mate - std::abs(result.score)) + 1) / 2);
        } else {
            score = std::format("cp {}", result.score);
        }
        send(std::format("info depth {} score {} time {} nodes {} nps {} pv {} hashfull {}",
                         result.depth, score, result.elapsed, result.nodes, nps, pvs, hashfull));
    }

    static bool recv(std::string &s) { return static_cast<bool>(std::getline(std::cin, s)); }
};

} // namespace Mondfisch
