#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <print>
#include <sstream>
#include <string>

#define RANK_MASK(rank) (0xffULL << ((rank) * 8))
#define FORWARD(pos, amount) ((pos) + 8 * (amount))
#define BACKWARD(pos, amount) ((pos) - 8 * (amount))
#define LEFT(pos, amount) ((pos) - (amount))
#define RIGHT(pos, amount) ((pos) + (amount))

#define WHITE 0
#define BLACK 1
#define NO_EP 8

#define CASTLING_QUEEN 0
#define CASTLING_KING 1
#define CASTLING_QUEEN_MASK_WHITE (1 << CASTLING_QUEEN)
#define CASTLING_KING_MASK_WHITE (1 << CASTLING_KING)
#define CASTLING_QUEEN_MASK_BLACK (2 << CASTLING_QUEEN_MASK_WHITE)
#define CASTLING_KING_MASK_BLACK (2 << CASTLING_KING_MASK_WHITE)

#define PIECE_COLOR_MASK 0b1000

/*#define MOVE_CAPTURE 1
#define MOVE_EP 2
#define MOVE_CASTLE 4
#define MOVE_DOUBLE_PAWN 8*/

namespace ChessGame {

using Position = uint8_t;
using BitBoard = uint64_t;

constexpr int numberChessPieces = 6;

const std::array<char, numberChessPieces + 1> pieceChars{'k', 'q', 'r', 'b', 'n', 'p', ' '};
const std::array<std::array<std::string, numberChessPieces + 1>, 2> pieceSymbols{{
    {"♚", "♛", "♜", "♝", "♞", "♟", " "},
    {"♔", "♕", "♖", "♗", "♘", "♙", " "},
}};
const std::array<char, 8> files{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
const std::array<int8_t, 2> signedColor{1, -1};

inline std::array<std::array<BitBoard, 9>, 2> epMasks;
inline std::array<BitBoard, 8> rankMasks;
inline std::array<BitBoard, 8> fileMasks;
inline std::array<BitBoard, 64> diag1Masks;
inline std::array<BitBoard, 64> diag2Masks;
inline std::array<std::array<BitBoard, 2>, 2> castlingPathMasks;
inline std::array<std::array<BitBoard, 2>, 2> castlingCheckMasks;
inline std::array<uint8_t, 2> firstHomeRank{0, 7};
inline std::array<uint8_t, 2> sndHomeRank{1, 6};
inline std::array<uint8_t, 2> promotionRank{7, 0};

inline std::array<std::array<uint64_t, 64>, 2> pawnAttacks;
inline std::array<BitBoard, 64> knightMoves;
inline std::array<BitBoard, 64> kingMoves;
inline std::array<BitBoard, 64> rookMoves;
inline std::array<BitBoard, 64> bishopMoves;
inline std::array<uint8_t, 64> castlingBoardMask;

inline uint64_t zobristPieces[2][numberChessPieces + 1][64];
inline uint64_t zobristSide;
inline std::array<uint64_t, 16> zobristCastle{};
inline std::array<uint64_t, 9> zobristEP{};

enum class MoveType : uint8_t {
    NONE = 0,
    MOVE_CAPTURE = 1,
    MOVE_EP = 2,
    MOVE_CASTLE = 4,
    MOVE_DOUBLE_PAWN = 8,
};

enum class Piece : uint8_t {
    KING = 0,
    QUEEN,
    ROOK,
    BISHOP,
    KNIGHT,
    PAWN,
    NONE,
};

inline void unset_bit(BitBoard &bb, Position pos) { bb &= ~(1ULL << pos); }

inline void set_bit(BitBoard &bb, Position pos) { bb |= 1ULL << pos; }

inline bool is_set(BitBoard bb, Position pos) { return (bb & (1ULL << pos)) != 0; }

inline BitBoard position_to_bitboard(Position pos) { return 1ULL << pos; }
inline Position bitboard_to_position(BitBoard bb) { return std::countr_zero(bb); }

inline constexpr Position coords_to_pos(Position x, Position y) { return x + y * 8; }

inline bool is_on_rank(Position pos, uint8_t rank) { return (1ULL << pos) & RANK_MASK(rank); }

inline constexpr uint8_t file_from_pos(Position pos) { return pos % 8; }

inline constexpr uint8_t rank_from_pos(Position pos) { return pos >> 3; }

inline uint8_t file_from_char(char c) { return c - 'a'; };

inline bool is_valid_pos(Position pos) { return pos < 64; }

inline uint8_t color_from_piece(uint8_t piece) { return (piece & PIECE_COLOR_MASK) != 0; }

inline Piece piece_from_piece(uint8_t piece) { return (Piece)(piece & (~PIECE_COLOR_MASK)); }

inline uint8_t to_piece(Piece piece, uint8_t color) { return (color * PIECE_COLOR_MASK) | (uint8_t)piece; }

inline uint64_t safe_shift(uint64_t x, uint8_t shift) {
    constexpr unsigned W = 64;

    uint64_t mask = (uint64_t)(shift >= W) - 1;
    return (x << (shift & (W - 1))) & mask;
}

const std::array<std::array<Position, 2>, 2> castlingKingMoves{
    {{coords_to_pos(2, 0), coords_to_pos(6, 0)}, {coords_to_pos(2, 7), coords_to_pos(6, 7)}}};

const std::array<std::array<Position, 2>, 2> castlingRookMovesTo{
    {{coords_to_pos(3, 0), coords_to_pos(5, 0)}, {coords_to_pos(3, 7), coords_to_pos(5, 7)}}};

const std::array<std::array<Position, 2>, 2> castlingRookMovesFrom{
    {{coords_to_pos(0, 0), coords_to_pos(7, 0)}, {coords_to_pos(0, 7), coords_to_pos(7, 7)}}};

const std::array<std::array<uint8_t, 2>, 2> castlingMask{
    {{CASTLING_QUEEN_MASK_WHITE, CASTLING_KING_MASK_WHITE}, {CASTLING_QUEEN_MASK_BLACK, CASTLING_KING_MASK_BLACK}}};

uint64_t reverse_bits(uint64_t x);
Position str2pos(std::string str);
std::string pos2str(Position pos);
std::string getPieceSymbol(uint8_t piece);
void showBitBoard(BitBoard board);
uint8_t char2Piece(char c);
void initConstants();

struct BitIterator {
    uint64_t bits;
    using value_type = unsigned;
    unsigned operator*() const { return std::countr_zero(bits); }

    BitIterator &operator++();
    bool operator!=(std::default_sentinel_t) const;
};

struct BitRange {
    uint64_t bits;

    BitIterator begin() const;
    std::default_sentinel_t end() const;
};

struct UndoMove {
    BitBoard occupancy[3];
    uint64_t hash;
    uint8_t capture;
    uint8_t castling;
    uint8_t ep;
};

struct Move {
    Position from = 0;
    Position to = 0;
    MoveType flags = MoveType::NONE;
    Piece promote = Piece::NONE;

    std::string toAlgebraicNotation(uint8_t coloredPiece) const;
    std::string toSimpleNotation() const;
    std::string toString() const;

    inline bool is_capture() {
        return uint8_t(flags) & (uint8_t(ChessGame::MoveType::MOVE_CAPTURE) | uint8_t(ChessGame::MoveType::MOVE_EP));
    }
    inline bool operator==(const Move &other) const {
        return *reinterpret_cast<const uint32_t *>(this) == *reinterpret_cast<const uint32_t *>(&other);
    }
};

struct ScoreMove {
    Move move;
    int16_t score;
    bool exact;
};

template <typename T, std::size_t N> struct StackList {
    uint16_t count = 0;
    std::array<T, N> stack;

    void push_back(T m) { stack[count++] = m; }
    T &push_back_empty() { return stack[count++]; }
    void pop_back() { count--; }

    T &back() { return stack[count - 1]; }

    void remove_unordered(size_t n) {
        count--;
        if (n == count) {
            return;
        }
        stack[n] = stack[count];
    }

    inline void swap(uint16_t a, uint16_t b) {
        T tmp = stack[a];
        stack[a] = stack[b];
        stack[b] = tmp;
    }

    size_t size() const { return count; }
    void resize(size_t size) { count = size; }

    void clear() { count = 0; }
    bool empty() { return count == 0; }

    T *begin() { return &stack[0]; }
    T *end() { return &stack[count]; }

    const T *begin() const { return &stack[0]; }
    const T *end() const { return &stack[count]; }

    T &operator[](size_t i) { return stack[i]; }
    const T &operator[](size_t i) const { return stack[i]; }
};

using MoveList = StackList<ScoreMove, 256>;

struct Game {
    std::array<std::array<BitBoard, numberChessPieces>, 2> bitboard;
    std::array<uint8_t, 64> board;
    uint8_t color;
    uint8_t ep = NO_EP;
    uint8_t castling = 0;
    uint8_t halfmove = 0;
    uint8_t fullmoves = 0;
    std::array<BitBoard, 2> occupancy{0, 0};
    BitBoard occupancyBoth;
    uint64_t hash = 0;
    StackList<UndoMove, 1024> undoStack;

    void reset();
    void calculateOccupancy();
    BitBoard attackBoard(BitBoard p, BitBoard mask);
    BitBoard rookAttacks(Position pos);
    BitBoard bishopAttacks(Position pos);
    void validRookMoves(Position pos, MoveList &moves);
    void validBishopMoves(Position pos, MoveList &moves);
    void validPawnMoves(Position pos, MoveList &moves);
    void validBitMaskMoves(Position pos, MoveList &moves, std::array<BitBoard, 64> boards);
    bool isSqaureAttacked(Position pos, uint8_t color);
    Position get_lva(BitBoard attackers, uint8_t color);
    BitBoard squareAttackers(Position pos, uint8_t color);
    bool is_valid_move(Move move);
    bool is_check(uint8_t color);
    void legal_moves(MoveList &moves);
    void pseudo_legal_moves(MoveList &moves);
    bool is_pseudo_legal(Move move);
    void movePiece(Position from, Position to, Piece pieceFrom, uint8_t pieceTo);
    void movePiece(Position from, Position to);
    void playMove(std::string &move);
    void playMove(Move move);
    void undoMove(Move move);
    uint32_t perft(uint32_t n);
    uint64_t calculateHash();
    void fromSimpleBoard();
    bool isConsistent();
    void loadFen(const std::string &fen);
    void loadFen(std::stringstream &ss);
    void loadStartingPos();
    std::string dumpFen();
    void showBoard();
    void showAll();
};

void perftInfo(Game &game, uint32_t n);
} // namespace ChessGame
