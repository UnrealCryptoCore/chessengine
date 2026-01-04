#include "game.h"
#include "minimax.h"
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <ostream>
#include <ranges>
#include <sstream>
#include <string>

std::string name = "idk";
std::string author = "cryptocore";

struct Logger {
    std::string filename;

    void log(const std::string &s) {
        std::cerr << s << std::endl;
        if (!filename.empty()) {
            std::ofstream file(filename);
            file << s << std::endl;
            file.flush();
            file.close();
        }
    }
};

struct IO {
    static void send(const std::string &s) {
        std::cout << s << '\n';
        std::cout.flush();
    }

    static void sendId() {
        send(std::format("id name {}", name));
        send(std::format("id author {}", author));
    }

    static void sendUciOk() { send("uciok"); }

    //static void sendOption(std::string &option) {}

    static void sendOptions() {}

    static void sendReadyOk() { send("readyok"); }

    static void sendBestMove(ChessGame::Move move) {
        send(std::format("bestmove {}", move.toSimpleNotation()));
    }

    static bool recv(std::string &s) { return static_cast<bool>(std::getline(std::cin, s)); }
};

ChessGame::Move minimax(ChessGame::Game &game, uint32_t depth) {
    int32_t value = Minimax::lossValue;
    uint32_t best = 0;
    ChessGame::MoveList moves;
    game.validMoves(moves);
    int32_t alpha = Minimax::lossValue;
    int32_t beta = Minimax::winValue;
    for (auto [idx, move] : std::views::enumerate(moves)) {
        game.playMove(move);
        int32_t tmp = -Minimax::simpleAlphaBeta(game, -beta, -alpha, depth - 1);
        if (tmp > value) {
            value = tmp;
            best = idx;
        }
        game.undoMove(move);
    }
    return moves[best];
}

int main() {
    Logger logger{};
    std::string inp;
    ChessGame::initConstants();
    ChessGame::Game game{};
    while (1) {
        if (!IO::recv(inp)) {
            logger.log(std::format("Command not recognized: {}", inp));
            continue;
        }
        std::stringstream ss(inp);
        std::string cmd;
        std::string arg;
        std::getline(ss, cmd, ' ');
        if (cmd == "uci") {
            IO::sendId();
            IO::sendUciOk();
        } else if (cmd == "isready") {
            IO::sendReadyOk();
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
            std::getline(ss, cmd, ' ');
            if (cmd == "perft") {
                std::getline(ss, arg, ' ');
                uint32_t n = std::stoi(arg);
                ChessGame::perftInfo(game, n);
            } else if (cmd == "depth") {
                std::getline(ss, arg, ' ');
                uint32_t n = std::stoi(arg);
                ChessGame::Move move = minimax(game, n);
                IO::sendBestMove(move);
            } else {
                logger.log(std::format("What is {}?", cmd));
            }

        } else if (cmd == "stop") {
            logger.log(std::format("Cant stop!"));
        } else if (cmd == "quit") {
            break;
        } else if (cmd == "show") {
            game.showBoard();
        } else {
            logger.log(std::format("What is {}?", cmd));
        }
    }
}
