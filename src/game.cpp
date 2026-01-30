#include "game.h"
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iterator>
#include <print>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace ChessGame {

uint64_t splitmix64(uint64_t &state) {
    uint64_t z = (state += 0x9E3779B97f4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint64_t reverse_bits(uint64_t x) {
    x = ((x & 0x5555555555555555ull) << 1) | ((x >> 1) & 0x5555555555555555ull);
    x = ((x & 0x3333333333333333ull) << 2) | ((x >> 2) & 0x3333333333333333ull);
    x = ((x & 0x0F0F0F0F0F0F0F0Full) << 4) | ((x >> 4) & 0x0F0F0F0F0F0F0F0Full);
    x = ((x & 0x00FF00FF00FF00FFull) << 8) | ((x >> 8) & 0x00FF00FF00FF00FFull);
    x = ((x & 0x0000FFFF0000FFFFull) << 16) | ((x >> 16) & 0x0000FFFF0000FFFFull);
    x = (x << 32) | (x >> 32);
    return x;
}

Position str2pos(const std::string str) {
    uint8_t x = str.at(0) - 'a';
    uint8_t y = str.at(1) - '1';
    return coords_to_pos(x, y);
}

std::string pos2str(Position pos) {
    uint8_t r = rank_from_pos(pos);
    uint8_t f = file_from_pos(pos);
    return std::format("{}{}", files[f], r + 1);
}

std::string getPieceSymbol(uint8_t piece) {
    Piece p = piece_from_piece(piece);
    uint8_t color = color_from_piece(piece);
    return pieceSymbols[color][(uint8_t)p];
}

BitIterator &BitIterator::operator++() {
    bits &= bits - 1;
    return *this;
}

BitIterator BitRange::begin() const { return {bits}; }

std::default_sentinel_t BitRange::end() const { return {}; }

bool BitIterator::operator!=(std::default_sentinel_t) const { return bits != 0; }

void showBitBoard(BitBoard board) {
    char bits[] = {' ', 'x'};
    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            uint8_t bit = (board >> coords_to_pos(x, 7 - y)) & 1;
            std::print("|{}", bits[bit]);
        }
        std::print("| {}\n", 8 - y);
    }
    for (auto c : files) {
        std::print(" {}", c);
    }
    std::print("\n");
}

uint8_t char2Piece(char c) {
    uint8_t player = 0;
    if (std::islower(c)) {
        player = 1;
    }
    char low = std::tolower(c);
    for (uint8_t i = 0; i < pieceChars.size(); i++) {
        if (pieceChars[i] == low) {
            return (player * PIECE_COLOR_MASK) | i;
        }
    }
    return (uint8_t)Piece::NONE;
}
std::string Move::toSimpleNotation() const {
    std::string res = std::format("{}{}", pos2str(from), pos2str(to));
    if (promote != Piece::NONE) {
        res.push_back(pieceChars[(uint8_t)promote]);
    }
    return res;
}

std::string Move::toAlgebraicNotation(uint8_t coloredPiece) const {
    Piece piece = piece_from_piece(coloredPiece);
    uint8_t color = color_from_piece(coloredPiece);
    bool capture = flags == MoveType::MOVE_CAPTURE;
    bool ep = flags == MoveType::MOVE_EP;

    if (flags == MoveType::MOVE_CASTLE) {
        uint8_t side = to > 4;
        if (side == CASTLING_QUEEN) {
            return "0-0-0";
        } else {
            return "0-0";
        }
    }

    std::string res = "";
    if (piece != Piece::PAWN) {
        res.append(getPieceSymbol(coloredPiece));
    }
    switch (piece) {
    case Piece::PAWN:
        if (capture) {
            res.push_back(files[file_from_pos(from)]);
        }
        break;
    case Piece::KING:
        break;
    default:
        res.append(pos2str(from));
        break;
    }
    if (capture) {
        res.push_back('x');
    }

    res.append(pos2str(to));

    if (capture && ep) {
        res.append(" e.p.");
    }

    if (promote != Piece::NONE) {
        res.push_back('=');
        res.append(getPieceSymbol(to_piece(promote, color)));
    }

    return res;
}

std::string Move::toString() const {
    return std::format("from: {} to: {} flags: {} promote: {}", from, to, (uint8_t)flags,
                       (uint8_t)promote);
}

void Game::reset() {
    color = WHITE;
    occupancyBoth = 0;
    occupancy[WHITE] = 0;
    occupancy[BLACK] = 0;
    ep = NO_EP;
    castling = 0;
    halfmove = 0;
    fullmoves = 1;
    board.fill((uint8_t)Piece::NONE);
    for (uint8_t piece = 0; piece < numberChessPieces; piece++) {
        bitboard[WHITE][piece] = 0ULL;
        bitboard[BLACK][piece] = 0ULL;
    }
    undoStack.clear();
    hash = calculateHash();
}

void Game::calculateOccupancy() {
    occupancy[WHITE] = 0;
    occupancy[BLACK] = 0;
    for (uint8_t p = 0; p < numberChessPieces; p++) {
        occupancy[WHITE] |= bitboard[WHITE][p];
        occupancy[BLACK] |= bitboard[BLACK][p];
    }
    occupancyBoth = occupancy[WHITE] | occupancy[BLACK];
}

void checkOccupancy(Game &game) {
    BitBoard occupancy[2];
    occupancy[WHITE] = 0;
    occupancy[BLACK] = 0;
    for (uint8_t p = 0; p < numberChessPieces; p++) {
        occupancy[WHITE] |= game.bitboard[WHITE][p];
        occupancy[BLACK] |= game.bitboard[BLACK][p];
    }
    assert(game.occupancy[0] == occupancy[0]);
    assert(game.occupancy[1] == occupancy[1]);
}

bool Game::is_valid_move(Move move) {
    bool res = true;
    make_move(move);
    if (is_check(!color)) {
        res = false;
    }
    undo_move(move);
    return res;
}

void Game::legal_moves(MoveList &moves) {
    pseudo_legal_moves(moves);
    for (int32_t i = 0; i < (int32_t)moves.size(); i++) {
        if (is_valid_move(moves[i].move)) {
            continue;
        }
        moves.remove_unordered(i);
        i--;
    }
}

BitBoard Game::attackBoard(BitBoard p, BitBoard mask) {
    BitBoard occRank = mask & occupancyBoth;
    BitBoard left = occRank - (p << 1);
    BitBoard right = reverse_bits(reverse_bits(occRank) - (reverse_bits(p) << 1));
    BitBoard attacks = (left ^ right) & mask;
    return attacks;
}

BitBoard Game::rookAttacks(Position pos) {
    uint8_t rank = rank_from_pos(pos);
    uint8_t file = file_from_pos(pos);
    BitBoard p = position_to_bitboard(pos);

    BitBoard attacks = attackBoard(p, fileMasks[file]);
    attacks |= attackBoard(p, rankMasks[rank]);
    return attacks;
}

BitBoard Game::bishopAttacks(Position pos) {
    BitBoard p = position_to_bitboard(pos);

    BitBoard attacks = attackBoard(p, diag1Masks[pos]);
    attacks |= attackBoard(p, diag2Masks[pos]);
    return attacks;
}

void Game::validRookMoves(Position pos, MoveList &moves) {
    BitBoard attacks = rookAttacks(pos);
    attacks &= ~occupancy[color];
    for (Position to : BitRange{attacks}) {
        MoveType flags =
            (MoveType)((uint8_t)(MoveType::MOVE_CAPTURE) * (board[to] != (uint8_t)Piece::NONE));
        moves.push_back(ScoreMove{{pos, to, flags}});
    }
}

void Game::validBishopMoves(Position pos, MoveList &moves) {
    BitBoard attacks = bishopAttacks(pos);
    attacks &= ~occupancy[color];
    for (Position to : BitRange{attacks}) {
        MoveType flags =
            (MoveType)((uint8_t)(MoveType::MOVE_CAPTURE) * (board[to] != (uint8_t)Piece::NONE));
        moves.push_back(ScoreMove{{pos, to, flags}});
    }
}

void addPawnMoves(MoveList &moves, Move move, bool promote) {
    if (promote) {
        for (uint8_t piece = (uint8_t)Piece::QUEEN; piece < (uint8_t)Piece::PAWN; piece++) {
            move.promote = (Piece)piece;
            moves.push_back(ScoreMove{move});
        }
    } else {
        moves.push_back(ScoreMove{move});
    }
}

void Game::validPawnMoves(Position pos, MoveList &moves) {
    uint8_t forward = signedColor[color];
    uint8_t flags = 0;

    Position move = FORWARD(pos, forward);
    bool promote = is_on_rank(move, promotionRank[color]);
    if (!is_set(occupancyBoth, move)) {
        addPawnMoves(moves, Move{pos, move}, promote);
    }

    BitBoard bitmoves;
    BitBoard epMask = epMasks[!color][ep];

    bitmoves = pawnAttacks[color][pos] & occupancy[!color];
    for (Position to : BitRange{bitmoves}) {
        MoveType flags = MoveType::MOVE_CAPTURE;
        addPawnMoves(moves, Move{pos, to, flags}, promote);
    }

    bitmoves = pawnAttacks[color][pos] & epMask;
    for (Position to : BitRange{bitmoves}) {
        MoveType flags = MoveType::MOVE_EP;
        addPawnMoves(moves, Move{pos, to, flags}, promote);
    }

    Position move2 = FORWARD(move, forward);
    if (is_on_rank(pos, sndHomeRank[color]) && !is_set(occupancyBoth, move) &&
        !is_set(occupancyBoth, move2)) {
        MoveType flags = MoveType::MOVE_DOUBLE_PAWN;
        moves.push_back(ScoreMove{{pos, move2, flags}});
    }
}

void Game::validBitMaskMoves(Position pos, MoveList &moves, std::array<BitBoard, 64> boards) {
    BitBoard bitmoves = boards[pos] & ~occupancy[color];
    for (Position to : BitRange{bitmoves}) {
        MoveType flags =
            (MoveType)((uint8_t)MoveType::MOVE_CAPTURE * is_set(occupancy[!color], to));
        moves.push_back(ScoreMove{{pos, to, flags}});
    }
}

bool Game::is_pseudo_legal(Move move) {
    uint8_t p = board[move.from];
    if (color_from_piece(p) != color) {
        return false;
    }
    return true;
}

void Game::pseudo_legal_moves(MoveList &moves) {
    BitBoard pawns = bitboard[color][(uint8_t)Piece::PAWN];
    for (Position pos : BitRange{pawns}) {
        validPawnMoves(pos, moves);
    }

    BitBoard knights = bitboard[color][(uint8_t)Piece::KNIGHT];
    for (Position pos : BitRange{knights}) {
        validBitMaskMoves(pos, moves, knightMoves);
    }

    BitBoard kings = bitboard[color][(uint8_t)Piece::KING];
    for (Position pos : BitRange{kings}) {
        validBitMaskMoves(pos, moves, kingMoves);
        MoveType flags = MoveType::MOVE_CASTLE;
        for (uint8_t side = 0; side < 2; side++) {
            if (castling & castlingMask[color][side] &&
                (castlingPathMasks[color][side] & occupancyBoth) == 0) {
                bool pathAttack = false;
                for (Position pos : BitRange{castlingCheckMasks[color][side]}) {
                    if (isSqaureAttacked(pos, !color)) {
                        pathAttack = true;
                        break;
                    }
                }
                if (!pathAttack) {
                    moves.push_back(ScoreMove{{pos, castlingKingMoves[color][side], flags}});
                }
            }
        }
    }

    BitBoard rooks = bitboard[color][(uint8_t)Piece::ROOK];
    for (Position pos : BitRange{rooks}) {
        validRookMoves(pos, moves);
    }

    BitBoard bishops = bitboard[color][(uint8_t)Piece::BISHOP];
    for (Position pos : BitRange{bishops}) {
        validBishopMoves(pos, moves);
    }

    BitBoard queens = bitboard[color][(uint8_t)Piece::QUEEN];
    for (Position pos : BitRange{queens}) {
        validRookMoves(pos, moves);
        validBishopMoves(pos, moves);
    }
}

Position Game::get_lva(BitBoard attackers, uint8_t color) {
    BitBoard bb;
    for (int8_t piece = (int8_t)Piece::PAWN; piece >= (int8_t)Piece::KING; piece--) {
        bb = attackers & bitboard[color][piece];
        if (bb) {
            return bitboard_to_position(bb);
        }
    }
    return 0;
}

BitBoard Game::squareAttackers(Position pos, uint8_t color) {
    BitBoard bAttacks = bishopAttacks(pos);
    BitBoard rAttacks = rookAttacks(pos);
    BitBoard enemyPawns = bitboard[color][(uint8_t)Piece::PAWN];
    BitBoard enemyQueens = bitboard[color][(uint8_t)Piece::QUEEN];
    BitBoard attacks = pawnAttacks[!color][pos];
    BitBoard attackers = enemyPawns & attacks;

    attackers |= knightMoves[pos] & bitboard[color][(uint8_t)Piece::KNIGHT];
    attackers |= bAttacks & bitboard[color][(uint8_t)Piece::BISHOP];
    attackers |= rAttacks & bitboard[color][(uint8_t)Piece::BISHOP];
    attackers |= (rAttacks | bAttacks) & enemyQueens;
    attackers |= (bitboard[color][(uint8_t)Piece::KING] & kingMoves[pos]);
    return attackers;
}

bool Game::isSqaureAttacked(Position pos, uint8_t enemy) {
    BitBoard enemyPawns = bitboard[enemy][(uint8_t)Piece::PAWN];
    BitBoard attacks;

    attacks = pawnAttacks[!enemy][pos];
    if (enemyPawns & attacks) {
        return true;
    }

    if ((knightMoves[pos] & bitboard[enemy][(uint8_t)Piece::KNIGHT]) != 0) {
        return true;
    }

    BitBoard enemyQueens = bitboard[enemy][(uint8_t)Piece::QUEEN];
    attacks = bishopAttacks(pos);
    if ((attacks & (enemyQueens | bitboard[enemy][(uint8_t)Piece::BISHOP])) != 0) {
        return true;
    }
    attacks = rookAttacks(pos);
    if ((attacks & (enemyQueens | bitboard[enemy][(uint8_t)Piece::ROOK])) != 0) {
        return true;
    }

    if (bitboard[enemy][(uint8_t)Piece::KING] & kingMoves[pos]) {
        return true;
    }
    return false;
}

bool Game::is_check(uint8_t color) {
    BitBoard board = bitboard[color][uint8_t(Piece::KING)];
    Position pos = bitboard_to_position(board);
    if (isSqaureAttacked(pos, !color)) {
        return true;
    }
    return false;
}

bool Game::has_non_pawn_material(uint8_t color) {
    return occupancy[color] ^ bitboard[color][uint8_t(Piece::KING)] ^
           bitboard[color][uint8_t(Piece::PAWN)];
}

inline void Game::move_piece(Position from, Position to, Piece pieceFrom, uint8_t pTo) {
    Piece pieceTo = piece_from_piece(pTo);
    BitBoard &bbFrom = bitboard[color][(uint8_t)pieceFrom];
    BitBoard &bbTo = bitboard[color][(uint8_t)pieceTo];
    unset_bit(bbFrom, from);
    set_bit(bbTo, to);
    board[to] = pTo;
    board[from] = (uint8_t)Piece::NONE;

    hash ^= zobristPieces[color][(uint8_t)pieceFrom][from];
    hash ^= zobristPieces[color][(uint8_t)pieceTo][to];
}

inline void Game::move_piece(Position from, Position to) {
    move_piece(from, to, piece_from_piece(board[from]), board[from]);
}

void Game::make_move(Move move) {
    UndoMove &undo = undoStack.push_back_empty();
    undo = {
        .occupancy = {occupancy[WHITE], occupancy[BLACK], occupancyBoth},
        .hash = hash,
        .capture = (uint8_t)Piece::NONE,
        .castling = castling,
        .ep = ep,
        .halfmove = halfmove,
    };

    hash ^= zobristEP[ep];
    ep = NO_EP;
    hash ^= zobristEP[ep];

    Position to = move.to;
    Piece pieceTo;
    uint8_t side;
    Position rFrom;
    Position rTo;

    switch (move.flags) {
    case MoveType::MOVE_EP:
        to = BACKWARD(move.to, signedColor[color]);
        /* falltrough */
    case MoveType::MOVE_CAPTURE:
        undo.capture = board[to];
        pieceTo = piece_from_piece(undo.capture);
        unset_bit(occupancy[!color], to);
        unset_bit(bitboard[!color][(uint8_t)pieceTo], to);
        board[to] = (uint8_t)Piece::NONE;
        hash ^= zobristPieces[!color][(uint8_t)pieceTo][to];
        break;
    case MoveType::MOVE_CASTLE:
        side = move.to > move.from;
        rFrom = castlingRookMovesFrom[color][side];
        rTo = castlingRookMovesTo[color][side];

        move_piece(rFrom, rTo);
        unset_bit(occupancy[color], rFrom);
        set_bit(occupancy[color], rTo);

        break;
    case MoveType::MOVE_DOUBLE_PAWN:
        hash ^= zobristEP[ep];
        ep = file_from_pos(move.from);
        hash ^= zobristEP[ep];
        break;
    case MoveType::NONE:
        break;
    }

    uint8_t cpieceTo = board[move.from];
    Piece pieceFrom = piece_from_piece(cpieceTo);
    if (move.promote != Piece::NONE) {
        cpieceTo = to_piece(move.promote, color);
    }

    hash ^= zobristCastle[castling];
    castling &= castlingBoardMask[move.from];
    castling &= castlingBoardMask[move.to];
    hash ^= zobristCastle[castling];

    unset_bit(occupancy[color], move.from);
    set_bit(occupancy[color], move.to);
    move_piece(move.from, move.to, pieceFrom, cpieceTo);
    occupancyBoth = occupancy[WHITE] | occupancy[BLACK];

    if (pieceFrom == Piece::PAWN || move.flags == MoveType::MOVE_CAPTURE) {
        halfmove = 0;
    } else {
        halfmove++;
    }

    color ^= 1;
    hash ^= zobristSide;
}

void Game::undo_move(Move move) {
    UndoMove undo = undoStack.back();
    color ^= 1;

    uint8_t pieceTo = board[move.to];
    Piece pieceFrom = piece_from_piece(pieceTo);
    if (move.promote != Piece::NONE) {
        pieceTo = to_piece(Piece::PAWN, color);
    }

    move_piece(move.to, move.from, pieceFrom, pieceTo);

    castling = undo.castling;
    ep = undo.ep;

    if (move.flags == MoveType::MOVE_CASTLE) {
        uint8_t side = move.to > move.from;
        Position rFrom = castlingRookMovesFrom[color][side];
        Position rTo = castlingRookMovesTo[color][side];
        move_piece(rTo, rFrom);
    }

    if ((uint8_t)move.flags & ((uint8_t)MoveType::MOVE_CAPTURE | (uint8_t)MoveType::MOVE_EP)) {
        Piece capture = piece_from_piece(undo.capture);
        Position to = move.to;
        if (move.flags == MoveType::MOVE_EP) {
            to = BACKWARD(move.to, signedColor[color]);
        }

        board[to] = undo.capture;
        set_bit(bitboard[!color][(uint8_t)capture], to);
    }

    occupancy[WHITE] = undo.occupancy[WHITE];
    occupancy[BLACK] = undo.occupancy[BLACK];
    occupancyBoth = undo.occupancy[2];
    hash = undo.hash;
    halfmove = undo.halfmove;

    undoStack.pop_back();
}

void Game::make_null_move() {
    UndoMove &undo = undoStack.push_back_empty();
    undo = {
        .hash = hash,
        .ep = ep,
    };
    hash ^= zobristEP[ep];
    ep = NO_EP;
    hash ^= zobristEP[ep];

    hash ^= zobristSide;
    color ^= 1;
}

void Game::undo_null_move() {
    UndoMove undo = undoStack.back();
    ep = undo.ep;
    hash = undo.hash;
    color ^= 1;
    undoStack.pop_back();
}

uint32_t Game::perft(uint32_t n) {
    if (n == 0) {
        return 1;
    }

    uint32_t counter = 0;
    MoveList moves;
    pseudo_legal_moves(moves);
    for (auto move : moves) {
        make_move(move.move);
        if (is_check(!color)) {
            undo_move(move.move);
            continue;
        }
        counter += perft(n - 1);
        undo_move(move.move);
    }
    return counter;
}

void Game::playMove(std::string &move) {
    Position from = str2pos(move.substr(0, 2));
    Position to = str2pos(move.substr(2, 4));
    Piece promote = Piece::NONE;
    if (move.size() >= 5) {
        promote = piece_from_piece(char2Piece(move.at(4)));
    }
    MoveType flags = MoveType::NONE;
    if (board[to] != (uint8_t)Piece::NONE) {
        flags = MoveType::MOVE_CAPTURE;
    }
    Piece pieceFrom = piece_from_piece(board[from]);
    if (pieceFrom == Piece::KING) {
        if (std::abs((int8_t)file_from_pos(from) - file_from_pos(to)) > 1) {
            flags = MoveType::MOVE_CASTLE;
        }
    } else if (pieceFrom == Piece::PAWN) {
        int8_t rnkFrom = rank_from_pos(from);
        int8_t rnkTo = rank_from_pos(to);
        int8_t fileFrom = file_from_pos(from);
        int8_t fileTo = file_from_pos(to);
        if (std::abs(rnkFrom - rnkTo) > 1) {
            flags = MoveType::MOVE_DOUBLE_PAWN;
        }
        if (fileFrom != fileTo && board[to] == (uint8_t)Piece::NONE) {
            flags = MoveType::MOVE_EP;
        }
    }
    Move m{
        .from = from,
        .to = to,
        .flags = flags,
        .promote = promote,
    };
    make_move(m);
}

uint64_t Game::calculateHash() {
    uint64_t hash = 0;
    for (Position pos = 0; pos < 64; pos++) {
        uint8_t p = board[pos];
        uint8_t color = color_from_piece(p);
        Piece piece = piece_from_piece(p);
        hash ^= zobristPieces[color][(uint8_t)piece][pos];
    }
    hash ^= color * zobristSide;
    hash ^= zobristEP[ep];
    hash ^= zobristCastle[castling];
    return hash;
}

void Game::fromSimpleBoard() {
    for (uint8_t p = 0; p < 2; p++) {
        bitboard[p].fill(0);
    }
    for (auto [pos, pb] : std::views::enumerate(board)) {
        uint8_t color = color_from_piece(pb);
        Piece piece = piece_from_piece(pb);
        if (piece == Piece::NONE) {
            continue;
        }
        set_bit(bitboard[color][(uint8_t)piece], pos);
    }
    calculateOccupancy();
}

void Game::loadStartingPos() {
    loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Game::loadFen(const std::string &fen) {
    std::stringstream ss(fen);
    loadFen(ss);
}

void Game::loadFen(std::stringstream &ss) {
    reset();
    std::string item;
    std::string fenBoard;
    std::string fenPlayer;
    std::string fenCastling;
    std::string fenEnPassant;
    std::string fenHalfMoves;
    std::string fenFullMoves;
    std::getline(ss, fenBoard, ' ');
    std::getline(ss, fenPlayer, ' ');
    std::getline(ss, fenCastling, ' ');
    std::getline(ss, fenEnPassant, ' ');
    std::getline(ss, fenHalfMoves, ' ');
    std::getline(ss, fenFullMoves, ' ');

    if (fenPlayer.size() == 1) {
        color = fenPlayer.at(0) == 'w' ? 0 : 1;
    }

    std::stringstream sboard(fenBoard);
    std::vector<std::string> fenRanks;
    while (std::getline(sboard, item, '/')) {
        fenRanks.push_back(item);
    }
    assert(fenRanks.size() == 8);

    board.fill((uint8_t)Piece::NONE);
    Position pos = 0;
    for (auto &fenRank : std::views::reverse(fenRanks)) {
        for (auto b : fenRank) {
            if (std::isdigit(b)) {
                pos += b - '0';
            } else {
                uint8_t field = char2Piece(b);
                board[pos] = field;
                pos++;
            }
        }
        assert(pos % 8 == 0);
    }
    fromSimpleBoard();

    castling = 0;
    if (fenCastling.contains('K')) {
        castling |= CASTLING_KING_MASK_WHITE;
    }
    if (fenCastling.contains('Q')) {
        castling |= CASTLING_QUEEN_MASK_WHITE;
    }
    if (fenCastling.contains('k')) {
        castling |= CASTLING_KING_MASK_BLACK;
    }
    if (fenCastling.contains('q')) {
        castling |= CASTLING_QUEEN_MASK_BLACK;
    }

    if (fenEnPassant.contains('-')) {
        ep = NO_EP;
    } else {
        uint8_t file = file_from_char(fenEnPassant.at(0));
        uint8_t rank = fenEnPassant.at(1) - '1';
        ep = file;
    }

    halfmove = std::stoi(fenHalfMoves);
    fullmoves = std::stoi(fenFullMoves);

    hash = calculateHash();
}

std::string Game::dumpFen() {
    std::string res = "";
    uint8_t empty = 0;
    for (int8_t rank = 7; rank >= 0; rank--) {

        for (uint8_t file = 0; file < 8; file++) {
            uint8_t piece = board[coords_to_pos(file, rank)];
            Piece p = piece_from_piece(piece);
            uint8_t color = color_from_piece(piece);
            if (p == Piece::NONE) {
                empty++;
                continue;
            }
            if (empty > 0) {
                res.push_back('0' + empty);
                empty = 0;
            }
            char c = pieceChars[(uint8_t)p];
            if (color == WHITE) {
                c = std::toupper(c);
            }
            res.push_back(c);
        }
        if (empty > 0) {
            res.push_back('0' + empty);
            empty = 0;
        }
        if (rank > 0) {
            res.push_back('/');
        }
    }
    res.push_back(' ');
    res.push_back(color == WHITE ? 'w' : 'b');
    res.push_back(' ');
    if (castling & CASTLING_KING_MASK_WHITE) {
        res.push_back('K');
    }
    if (castling & CASTLING_QUEEN_MASK_WHITE) {
        res.push_back('Q');
    }
    if (castling & CASTLING_KING_MASK_BLACK) {
        res.push_back('k');
    }
    if (castling & CASTLING_QUEEN_MASK_BLACK) {
        res.push_back('q');
    }
    if (castling == 0) {
        res.push_back('-');
    }

    res.push_back(' ');

    if (ep == NO_EP) {
        res.push_back('-');
    } else {
        Position epPos = bitboard_to_position(epMasks[color][ep]);
        res.append(pos2str(epPos));
    }

    res.push_back(' ');
    res.push_back('0');
    res.push_back(' ');
    res.push_back('1');
    return res;
}

void Game::showBoard() {
    std::print("{}\n", dumpFen());
    for (int8_t y = 7; y >= 0; y--) {
        for (int8_t x = 0; x < 8; x++) {
            uint8_t coloredPiece = board[coords_to_pos(x, y)];
            std::print("|{}", getPieceSymbol(coloredPiece));
        }
        std::print("| {}\n", y + 1);
    }
    for (auto c : files) {
        std::print(" {}", c);
    }
    std::print("\n");
}

void Game::showAll() {
    showBoard();
    for (uint8_t color = 0; color < 2; color++) {
        for (uint8_t i = 0; i < (uint8_t)Piece::NONE; i++) {
            std::print("color {} piece: {}\n", color, pieceSymbols[color][i]);
            showBitBoard(bitboard[color][i]);
            std::print("\n");
        }
    }
}

bool Game::isConsistent() {
    for (auto [pos, pb] : std::views::enumerate(board)) {
        uint8_t color = color_from_piece(pb);
        Piece piece = piece_from_piece(pb);
        if (piece == Piece::NONE) {
            continue;
        }
        // set_bit(bitboard[color][(uint8_t)piece], pos);
        if (!is_set(bitboard[color][(uint8_t)piece], pos)) {

            std::print("inconsistency!!!");
            showBoard();
            showBitBoard(bitboard[color][(uint8_t)piece]);
            exit(1);
        }
    }
    return true;
}

template <std::size_t N>
void initMoves(std::array<uint64_t, 64> &bitMoves, std::array<std::array<int8_t, 2>, N> &moves) {
    for (Position pos = 0; pos < 64; pos++) {
        BitBoard mask = 0;
        int8_t rank = rank_from_pos(pos);
        int8_t file = file_from_pos(pos);
        for (auto move : moves) {
            if (rank + move[0] < 0 || rank + move[0] > 7) {
                continue;
            }
            if (file + move[1] < 0 || file + move[1] > 7) {
                continue;
            }
            Position nPos = FORWARD(RIGHT(pos, move[1]), move[0]);
            mask |= position_to_bitboard(nPos);
        }

        bitMoves[pos] = mask;
    }
}

void initDiagonals() {
    for (Position pos = 0; pos < 64; pos++) {
        BitBoard mask1 = 0;
        BitBoard mask2 = 0;
        uint8_t rank = rank_from_pos(pos);
        uint8_t file = file_from_pos(pos);

        for (Position pos2 = 0; pos2 < 64; pos2++) {
            uint8_t rank2 = rank_from_pos(pos2);
            uint8_t file2 = file_from_pos(pos2);
            if (rank - file == rank2 - file2) {
                set_bit(mask1, pos2);
            }
            if (rank + file == rank2 + file2) {
                set_bit(mask2, pos2);
            }
        }

        diag1Masks[pos] = mask1;
        diag2Masks[pos] = mask2;
    }
}

void initCastlingMasks() {
    BitBoard queenSide = fileMasks[2] | fileMasks[3];
    BitBoard kingSide = fileMasks[5] | fileMasks[6];

    castlingPathMasks[WHITE][CASTLING_KING] = kingSide & rankMasks[0];
    castlingPathMasks[WHITE][CASTLING_QUEEN] = (queenSide | fileMasks[1]) & rankMasks[0];
    castlingPathMasks[BLACK][CASTLING_KING] = kingSide & rankMasks[7];
    castlingPathMasks[BLACK][CASTLING_QUEEN] = (queenSide | fileMasks[1]) & rankMasks[7];

    kingSide |= fileMasks[4];
    queenSide |= fileMasks[4];
    castlingCheckMasks[WHITE][CASTLING_KING] = kingSide & rankMasks[0];
    castlingCheckMasks[WHITE][CASTLING_QUEEN] = queenSide & rankMasks[0];
    castlingCheckMasks[BLACK][CASTLING_KING] = kingSide & rankMasks[7];
    castlingCheckMasks[BLACK][CASTLING_QUEEN] = queenSide & rankMasks[7];

    for (uint8_t i = 0; i < 64; i++) {
        castlingBoardMask[i] = 0b1111;
    }
    castlingBoardMask[str2pos("e1")] &= ~(CASTLING_QUEEN_MASK_WHITE | CASTLING_KING_MASK_WHITE);
    castlingBoardMask[str2pos("a1")] &= ~CASTLING_QUEEN_MASK_WHITE;
    castlingBoardMask[str2pos("h1")] &= ~CASTLING_KING_MASK_WHITE;

    castlingBoardMask[str2pos("e8")] &= ~(CASTLING_QUEEN_MASK_BLACK | CASTLING_KING_MASK_BLACK);
    castlingBoardMask[str2pos("a8")] &= ~CASTLING_QUEEN_MASK_BLACK;
    castlingBoardMask[str2pos("h8")] &= ~CASTLING_KING_MASK_BLACK;
}

void initEPMasks() {
    for (uint8_t i = 0; i < 8; i++) {
        epMasks[WHITE][i] = rankMasks[2] & fileMasks[i];
        epMasks[BLACK][i] = rankMasks[5] & fileMasks[i];
    }
    epMasks[WHITE][8] = 0;
    epMasks[BLACK][8] = 0;
}

void initMasks() {
    for (uint8_t i = 0; i < 8; i++) {
        fileMasks[i] = 0x0101010101010101ULL << i;
        rankMasks[i] = 0xffULL << (8 * i);
    }
    initDiagonals();
    initCastlingMasks();
    initEPMasks();
}
void initKnightMoves() {
    std::array<std::array<int8_t, 2>, 8> moves{
        {{1, 2}, {1, -2}, {2, 1}, {2, -1}, {-1, -2}, {-1, 2}, {-2, -1}, {-2, 1}}};
    initMoves(knightMoves, moves);
}

void initPawnMoves() {
    std::array<std::array<std::array<int8_t, 2>, 2>, 2> moves{
        {{{{{1, 1}}, {{1, -1}}}}, {{{{-1, 1}}, {{-1, -1}}}}}};

    initMoves(pawnAttacks[0], moves[0]);
    initMoves(pawnAttacks[1], moves[1]);
}

void initKingMoves() {
    std::array<std::array<int8_t, 2>, 8> moves{
        {{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}}};
    initMoves(kingMoves, moves);
}

void initRookMoves() {}

void initBishopMoves() {}

void initZobrist() {
    uint64_t seed = 1337;
    seed = splitmix64(seed);

    for (Position pos = 0; pos < 64; pos++) {
        for (uint8_t piece = (uint8_t)Piece::KING; piece < (uint8_t)Piece::NONE; piece++) {
            zobristPieces[WHITE][piece][pos] = splitmix64(seed);
            zobristPieces[BLACK][piece][pos] = splitmix64(seed);
        }
        zobristPieces[WHITE][numberChessPieces][pos] = 0;
        zobristPieces[BLACK][numberChessPieces][pos] = 0;
    }

    zobristSide = splitmix64(seed);
    for (uint8_t i = 0; i < zobristCastle.size(); i++) {
        zobristCastle[i] = splitmix64(seed);
    }
    for (uint8_t i = 0; i < zobristEP.size() - 1; i++) {
        zobristEP[i] = splitmix64(seed);
    }
    zobristEP[zobristEP.size() - 1] = 0; // NO_EP
}

void initConstants() {
    initMasks();
    initKnightMoves();
    initKingMoves();
    initPawnMoves();
    initRookMoves();
    initBishopMoves();

    initZobrist();
}

void perftInfo(Game &game, uint32_t n) {
    MoveList moves;
    uint32_t count = 0;
    game.pseudo_legal_moves(moves);
    for (auto move : moves) {
        game.make_move(move.move);
        if (game.is_check(!game.color)) {
            game.undo_move(move.move);
            continue;
        }
        uint32_t tmp = game.perft(n - 1);
        count += tmp;
        std::print("{}: {}\n", move.move.toSimpleNotation(), tmp);
        game.undo_move(move.move);
    }

    std::print("\nnodes searched {}: \n", count);
}
} // namespace ChessGame
