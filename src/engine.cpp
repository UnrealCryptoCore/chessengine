#include "engine_search.h"
#include "game.h"
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <ostream>
#include <print>
#include <ranges>
#include <sstream>
#include <string>

std::string name = "idk";
std::string author = "cryptocore";

bool debug = false;

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

constexpr const char *toString(OptionType type) {
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
    }
}

struct UciGame {
    ChessGame::Game game{};
    Search::SearchContext ctx{};
    uint64_t wtime = 0;
    uint64_t btime = 0;
    uint8_t kBest = 1;

    void new_uci_game(Search::TranspositionTable *table) {
        game.reset();
        game.loadStartingPos();
        ctx.reset();
        ctx.table = table;
        ctx.table->clear();
    }
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

Logger logger{"", std::ofstream{}};

struct IO {
    static void send(const std::string &s) {
        std::cout << s << std::endl;
        std::cout.flush();
        logger.log(std::format("{}\n", s));
    }

    static void sendId() {
        send(std::format("id name {}", name));
        send(std::format("id author {}", author));
    }

    static void sendUciOk() { send("uciok"); }

    static void sendOption(const Option &option) {
        std::string res = "option name ";
        res += option.name;
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
            .max = "",
            .defaultStr = "1"
        });
    }

    static void sendReadyOk() { send("readyok"); }

    static void sendBestMove(ChessGame::Move move) { send(std::format("bestmove {}", move.toSimpleNotation())); }

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
        if (result.score > Search::mate) {
            score = std::format("mate {}", result.score - Search::mate);
        } else {
            score = std::format("cp {}", result.score);
        }
        send(std::format("info depth {} score {} time {} nodes {} nps {} pv {} hashfull {}", result.depth, score,
                         result.elapsed, result.nodes, nps, pvs, hashfull));
    }

    static bool recv(std::string &s) { return static_cast<bool>(std::getline(std::cin, s)); }
};

Search::SearchResult iterative_deepening(UciGame &game, uint32_t depth) {
    Search::SearchResult lastResult;
    auto startDepth = game.ctx.timeStart;

    game.game.legal_moves(game.ctx.moves);

    for (uint32_t i = 1; i <= depth; i++) {
        Search::search_root(game.ctx, game.game, i, game.kBest);
        Search::sort_moves(game.ctx.moves);

        if (game.ctx.stop) {
            break;
        }

        ChessGame::ScoreMove bestMove = game.ctx.moves[0];

        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - startDepth).count();
        Search::SearchResult result{
            .score = bestMove.score,
            .bestMove = bestMove.move,
            .nodes = game.ctx.nodes,
            .pv = {bestMove.move},
            .depth = i,
            .elapsed = elapsed,
        };
        IO::sendSearchInfo(result, game.ctx.table->hashFull());

        startDepth = end;
        lastResult = result;

        if (Search::is_mate(result.score)) {
            break;
        }
    }
    return lastResult;
}

ChessGame::Move simple_choose_move(ChessGame::MoveList &moves) { return moves[std::rand() % moves.size()].move; }

ChessGame::Move choose_move(Search::SearchContext &ctx) {
    constexpr Search::Score window = 20;
    Search::Score treshold = ctx.moves[0].score;
    if (Search::is_mate(treshold)) {
        return ctx.moves[0].move;
    }
    treshold -= window;

    float max = 0;
    float counter = 0;
    for (uint16_t i = 0; i < ctx.moves.size(); i++) {
        int32_t score = ctx.moves[i].score;
        if (score <= treshold) {
            break;
        }
        max += std::exp(score * 0.001f);
    }

    float r = static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / max));

    for (int32_t i = 0; i < (int32_t)ctx.moves.size(); i++) {
        int32_t score = ctx.moves[i].score;
        counter += std::exp(score * 0.001f);
        if (counter >= r) {
            return ctx.moves[i].move;
        }
    }
    std::print("something went wrong!");
    return ctx.moves[0].move;
}

void filter_move_canditates(ChessGame::MoveList &moves, Search::Score window) {
    for (uint16_t i = 1; i < moves.size(); i++) {
        if (!moves[i].exact) {
            moves.remove_unordered(i);
            i--;
            continue;
        }
        if (moves[i].score < moves[0].score - window) {
            moves.remove_unordered(i);
            i--;
            continue;
        }
    }
}

void think(UciGame &game, uint32_t depth) {
    game.ctx.startTimer();
    game.ctx.resetSearch();
    game.ctx.moves.clear();

    iterative_deepening(game, depth);
    filter_move_canditates(game.ctx.moves, 20);
    ChessGame::Move best = simple_choose_move(game.ctx.moves);
    IO::sendBestMove(best);
}

uint64_t calculateSafetyMargin(uint64_t time) {
    if (time <= 50) {
        return 7;
    } else if (time <= 100) {
        return 10;
    } else if (time <= 1000) {
        return 15;
    } else {
        return 20;
    }
}

int main() {
    srand(time(NULL));
    std::string inp;
    ChessGame::initConstants();

    Search::TranspositionTable table;
    table.setsize(16);

    UciGame game{};
    game.game.loadStartingPos();
    game.ctx.reset();
    game.ctx.table = &table;

    while (1) {
        if (!IO::recv(inp)) {
            if (debug) {
                logger.log(std::format("Command not recognized: {}", inp));
            }
            continue;
        }
        logger.log(inp);
        std::stringstream ss(inp);
        std::string cmd;
        std::string arg;
        std::getline(ss, cmd, ' ');
        if (cmd == "uci") {
            IO::sendId();
            IO::sendOptions();
            IO::sendUciOk();
        } else if (cmd == "isready") {
            IO::sendReadyOk();
        } else if (cmd == "ucinewgame") {
            game.new_uci_game(&table);
        } else if (cmd == "position") {
            std::getline(ss, arg, ' ');
            if (arg == "fen") {
                game.game.loadFen(ss);
            } else if (arg == "startpos") {
                game.game.loadStartingPos();
            }
            std::getline(ss, arg, ' ');
            if (arg == "moves") {
                while (std::getline(ss, arg, ' ')) {
                    game.game.playMove(arg);
                }
            }

        } else if (cmd == "go") {
            std::getline(ss, cmd, ' ');
            if (cmd == "perft") {
                std::getline(ss, arg, ' ');
                uint32_t n = std::stoi(arg);
                ChessGame::perftInfo(game.game, n);
            } else if (cmd == "depth") {
                game.ctx.thinkingTime = 0;
                std::getline(ss, arg, ' ');
                uint32_t n = std::stoi(arg);
                think(game, n);
            } else if (cmd == "movetime") {
                std::getline(ss, arg, ' ');
                uint32_t n = std::stoi(arg);
                game.ctx.thinkingTime = n - calculateSafetyMargin(n);
                think(game, 1024);
            } else {
                logger.log(std::format("What is {}?", cmd));
            }

        } else if (cmd == "setoption") {
            std::getline(ss, cmd, ' ');
            if (cmd == "Hash") {
                std::getline(ss, arg, ' ');
                uint32_t n = std::stoi(arg);
                table.setsize(n);
            } else if (cmd == "MultiPV") {
                std::getline(ss, arg, ' ');
                uint32_t n = std::stoi(arg);
                game.kBest = n;
            }
        } else if (cmd == "stop") {
            game.ctx.stop = true;
        } else if (cmd == "quit") {
            break;
        } else if (cmd == "debug") {
            logger.init();
            debug = true;
            logger.filename = "debug.log";
        } else if (cmd == "show") {
            std::getline(ss, arg, ' ');
            if (arg == "all") {
                game.game.showAll();
            } else {
                game.game.showBoard();
            }
        } else {
            logger.log(std::format("What is {}?", cmd));
        }
    }
}
