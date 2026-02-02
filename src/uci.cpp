#include "uci.h"
#include <print>

namespace Mondfisch {

void UciEngine::new_uci_game() {
    game.reset();
    game.loadStartingPos();
    ctx.reset();
    ctx.table = &table;
    ctx.table->clear();
}

Move choose_top_k(MoveList &moves, uint8_t k) {
    if (k <= 1) {
        return moves[0].move;
    }
    return moves[std::rand() % k].move;
}

Move simple_choose_move(MoveList &moves) { return moves[std::rand() % moves.size()].move; }

Move choose_move(Search::SearchContext &ctx) {
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
    return ctx.moves[0].move;
}

void filter_move_canditates(MoveList &moves, Search::Score window, uint8_t k) {
    moves.resize(k);
    for (uint16_t i = 1; i < moves.size(); i++) {
        if (moves[i].score < moves[0].score - window) {
            moves.remove_unordered(i);
            i--;
            continue;
        }
    }
}

void UciEngine::think() {
    ctx.startTimer();

    Search::iterative_deepening(ctx, game, depth);
    filter_move_canditates(ctx.moves, 20, kBest);
    Move best = choose_top_k(ctx.moves, kBest);
    IO::sendBestMove(best);
}

uint64_t calc_safe_move_time(uint64_t time) {
    if (time <= 50) {
        return time - 7;
    } else if (time <= 100) {
        return time - 10;
    } else if (time <= 1000) {
        return time - 15;
    } else {
        return time - 20;
    }
}

uint64_t UciEngine::calc_time() {
    constexpr int64_t min = 10;
    int64_t timeLeft = game.color == WHITE ? timeValues.wtime : timeValues.btime;
    int64_t timeInc = game.color == WHITE ? timeValues.winc : timeValues.binc;

    int64_t target = timeLeft / 40 + timeInc;

    if (target > 0.8 * timeLeft) {
        target = 0.8 * timeLeft;
    }

    target -= 20;
    if (target <= min) {
        target = min;
    }
    return target;
}

void UciEngine::loop() {
    std::string inp;
    while (1) {
        if (!IO::recv(inp)) {
            continue;
        }
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
            new_uci_game();
        } else if (cmd == "position") {
            std::getline(ss, arg, ' ');
            if (arg == "fen") {
                game.loadFen(ss);
            } else if (arg == "startpos") {
                game.loadStartingPos();
            }
            std::getline(ss, arg, ' ');
            if (arg == "moves") {
                while (std::getline(ss, arg, ' ')) {
                    game.playMove(arg);
                }
            }

        } else if (cmd == "go") {
            ss >> cmd;
            if (cmd == "perft") {
                uint32_t n;
                ss >> n;
                perftInfo(game, n);
            } else {
                depth = -1;
                timeValues = TimeManagement{};
                do {
                    if (cmd == "depth") {
                        ss >> depth;
                    } else if (cmd == "movetime") {
                        ss >> timeValues.movetime;
                    } else if (cmd == "wtime") {
                        ss >> timeValues.wtime;
                    } else if (cmd == "btime") {
                        ss >> timeValues.btime;
                    } else if (cmd == "winc") {
                        ss >> timeValues.winc;
                    } else if (cmd == "binc") {
                        ss >> timeValues.binc;
                    }
                } while (ss >> cmd);
                if (timeValues.movetime != -1) {
                    ctx.thinkingTime = timeValues.movetime =
                        calc_safe_move_time(timeValues.movetime);
                } else if (depth == -1) {
                    ctx.thinkingTime = calc_time();
                }
                if (depth == -1) {
                    depth = Search::max_depth;
                }
                think();
            }
        } else if (cmd == "setoption") {
            ss >> cmd;
            if (cmd == "Hash") {
                uint32_t n;
                ss >> n;
                table.setsize(n);
            } else if (cmd == "MultiPV") {
                uint32_t n;
                ss >> n;
                kBest = n;
            }
        } else if (cmd == "stop") {
            ctx.stop = true;
        } else if (cmd == "quit") {
            break;
        } else if (cmd == "debug") {
        } else if (cmd == "show") {
            std::getline(ss, arg, ' ');
            if (arg == "all") {
                game.showAll();
            } else {
                game.showBoard();
            }
        }
    }
}
} // namespace Mondfisch
