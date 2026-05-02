#pragma once

#include <array>
#include <bit>
#include <string>
#include <string_view>
#include <vector>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "cuckoo.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "zobrist.hpp"


namespace Syft {

class CastlingRights {
public:
    enum class Side : UInt8 {
        WHITE_KINGSIDE = 1 << 0,
        WHITE_QUEENSIDE = 1 << 1,
        BLACK_KINGSIDE = 1 << 2,
        BLACK_QUEENSIDE = 1 << 3
    };

    static constexpr Side WHITE_KINGSIDE = Side::WHITE_KINGSIDE;
    static constexpr Side WHITE_QUEENSIDE = Side::WHITE_QUEENSIDE;
    static constexpr Side BLACK_KINGSIDE = Side::BLACK_KINGSIDE;
    static constexpr Side BLACK_QUEENSIDE = Side::BLACK_QUEENSIDE;

    constexpr CastlingRights() noexcept : rights_(0) {}
    constexpr CastlingRights(Side side) noexcept : rights_(static_cast<UInt8>(side)) {}
    constexpr CastlingRights(UInt8 rights) noexcept : rights_(rights) { assert(rights < 16); }

    constexpr bool operator==(const CastlingRights &other) const noexcept { return rights_ == other.rights_; }
    constexpr operator UInt8() const noexcept { return rights_; }

    constexpr void set(Side side) noexcept { rights_ |= static_cast<UInt8>(side); }
    constexpr bool get(Side side) const noexcept { return (rights_ & static_cast<UInt8>(side)) != 0; }

    constexpr bool get(Color color) const noexcept {
        assert(color != Color::NONE);
        if (color == Color::WHITE) {
            return get(Side::WHITE_KINGSIDE) || get(Side::WHITE_QUEENSIDE);
        } else {
            return get(Side::BLACK_KINGSIDE) || get(Side::BLACK_QUEENSIDE);
        }
    }

    constexpr void clear() noexcept { rights_ = 0; }

    constexpr void clear(Side side) noexcept { rights_ &= ~static_cast<UInt8>(side); }

    constexpr void clear(Color color) noexcept {
        assert(color != Color::NONE);
        if (color == Color::WHITE) {
            clear(Side::WHITE_KINGSIDE);
            clear(Side::WHITE_QUEENSIDE);
        } else {
            clear(Side::BLACK_KINGSIDE);
            clear(Side::BLACK_QUEENSIDE);
        }
    }

    constexpr bool empty() const noexcept { return rights_ == 0; }

    constexpr UInt8 hash() const noexcept { return rights_; }
    static constexpr UInt8 hashIndex(Side side) noexcept { return static_cast<UInt8>(std::countr_zero(static_cast<UInt8>(side))); }

    static constexpr Color color(Side side) noexcept {
        if (side == Side::WHITE_KINGSIDE || side == Side::WHITE_QUEENSIDE) {
            return Color::WHITE;
        } else if (side == Side::BLACK_KINGSIDE || side == Side::BLACK_QUEENSIDE) {
            return Color::BLACK;
        } else {
            assert(false);
            return Color::NONE;
        }
    }

    static constexpr bool kingside(Side side) noexcept {
        return side == Side::WHITE_KINGSIDE || side == Side::BLACK_KINGSIDE;
    }

    static constexpr Side closestSide(Square square, Square kingSquare, Color color) noexcept {
        assert(square != Square::NONE && kingSquare != Square::NONE && color != Color::NONE);
        UInt8 shift = 0;
        if (color == Color::BLACK) {
            shift = 2;
        }
        if (square < kingSquare) {
            shift += 1;
        }
        return static_cast<Side>(1 << shift);
    }

    static constexpr Square rookFrom(Side side) noexcept {
        if (kingside(side)) {
            return Square(Square::SQUARE_H1, color(side));
        } else {
            return Square(Square::SQUARE_A1, color(side));
        }
    }

    static constexpr Square kingTo(Side side) noexcept {
        if (kingside(side)) {
            return Square(Square::SQUARE_G1, color(side));
        } else {
            return Square(Square::SQUARE_C1, color(side));
        }
    }

    static constexpr Square rookTo(Side side) noexcept {
        if (kingside(side)) {
            return Square(Square::SQUARE_F1, color(side));
        } else {
            return Square(Square::SQUARE_D1, color(side));
        }
    }

    constexpr UInt8 internal() const noexcept { return rights_; }

private:
    UInt8 rights_;
};

class Position {
public:
    static constexpr const std::string_view FEN_STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    explicit Position(std::string_view fen = FEN_STARTPOS) {
        states_.reserve(MAX_STATES);
        if (!set(fen)) {
            assert(false);
            reset();
        }
    }

    void reset() noexcept {
        states_.clear();
        states_.push_back(State());

        pieceBitboards_.fill(Bitboard());
        occupancyBitboards_.fill(Bitboard());
        board_.fill(Piece::NONE);

        sideToMove_ = Color::WHITE;
        ply_ = 0;
    }

    bool set(std::string_view fen) {
        reset();

        while (!fen.empty() && fen[0] == ' ') {
            fen.remove_prefix(1);
        }

        if (fen.empty()) {
            return false;
        }

        const std::vector<std::string_view> parts = Utils::splitStringView(fen, ' ');
        const std::string_view board = parts.size() > 0 ? parts[0] : "";
        const std::string_view side = parts.size() > 1 ? parts[1] : "w";
        const std::string_view castling = parts.size() > 2 ? parts[2] : "-";
        const std::string_view enPassant = parts.size() > 3 ? parts[3] : "-";
        const std::string_view halfmoves = parts.size() > 4 ? parts[4] : "0";
        const std::string_view fullmoves = parts.size() > 5 ? parts[5] : "1";

        if (board.empty()) {
            return false;
        }

        Int32 index = 56;
        for (char c : board) {
            if (c == '/') {
                index -= 16;
            } else if (c >= '0' && c <= '8') {
                index += c - '0';
            } else {
                if (index < 0 || index >= 64) {
                    return false;
                }

                const Piece piece = Piece(std::string_view(&c, 1));
                placePiece<true>(piece, Square(static_cast<UInt8>(index)));
                index++;
            }
        }

        if (pieces(PieceType::KING, Color::WHITE).empty() || pieces(PieceType::KING, Color::BLACK).empty()) {
            return false;
        }

        if (side != "w" && side != "b") {
            return false;
        }
        sideToMove_ = (side == "w") ? Color::WHITE : Color::BLACK;
        if (sideToMove_ == Color::WHITE) {
            state().hash ^= Zobrist::sideToMove();
        }

        if (castling != "-" && !castling.empty()) {
            for (char c : castling) {
                if (c == 'K') {
                    state().castlingRights.set(CastlingRights::WHITE_KINGSIDE);
                } else if (c == 'Q') {
                    state().castlingRights.set(CastlingRights::WHITE_QUEENSIDE);
                } else if (c == 'k') {
                    state().castlingRights.set(CastlingRights::BLACK_KINGSIDE);
                } else if (c == 'q') {
                    state().castlingRights.set(CastlingRights::BLACK_QUEENSIDE);
                } else {
                    return false;
                }
            }
            state().hash ^= Zobrist::castling(state().castlingRights.hash());
        } else if (castling != "-") {
            return false;
        }

        if (enPassant != "-") {
            if (enPassant.size() != 2) {
                return false;
            }
            const char fileChar = enPassant[0];
            const char rankChar = enPassant[1];
            if (fileChar < 'a' || fileChar > 'h' || rankChar < '1' || rankChar > '8') {
                return false;
            }

            state().enPassantSquare = Square(enPassant);
            state().hash ^= Zobrist::enPassant(state().enPassantSquare.file());
        }

        state().halfmoveClock = static_cast<UInt16>(std::stoi(std::string(halfmoves)));
        ply_ = static_cast<UInt16>((std::stoi(std::string(fullmoves)) - 1) * 2 + (sideToMove_ == Color::BLACK ? 1 : 0));

        assert(state().hash == zobrist());

        updateRepetitions();
        updateChecks();
        updatePins();
        updateBlocks();
        updateThreats();

        return true;
    }

    std::string fen() const {
        std::string str;
        str.reserve(100);

        for (Int32 rank = 7; rank >= 0; rank--) {
            UInt8 emptyCount = 0;
            for (UInt8 file = 0; file < 8; file++) {
                const Piece piece = pieceAt(Square(file, static_cast<UInt8>(rank)));
                if (piece == Piece::NONE) {
                    emptyCount++;
                } else {
                    if (emptyCount > 0) {
                        str += std::to_string(emptyCount);
                        emptyCount = 0;
                    }
                    str += static_cast<std::string>(piece);
                }
            }
            if (emptyCount > 0) {
                str += std::to_string(emptyCount);
            }
            if (rank > 0) {
                str += '/';
            }
        }

        str += ' ';
        str += (sideToMove_ == Color::WHITE) ? 'w' : 'b';

        str += ' ';
        if (state().castlingRights.empty()) {
            str += '-';
        } else {
            if (state().castlingRights.get(CastlingRights::WHITE_KINGSIDE)) {
                str += 'K';
            }
            if (state().castlingRights.get(CastlingRights::WHITE_QUEENSIDE)) {
                str += 'Q';
            }
            if (state().castlingRights.get(CastlingRights::BLACK_KINGSIDE)) {
                str += 'k';
            }
            if (state().castlingRights.get(CastlingRights::BLACK_QUEENSIDE)) {
                str += 'q';
            }
        }

        str += ' ';
        if (state().enPassantSquare == Square::NONE) {
            str += '-';
        } else {
            str += static_cast<std::string>(state().enPassantSquare);
        }

        str += ' ';
        str += std::to_string(state().halfmoveClock);
        str += ' ';
        str += std::to_string(fullMoveNumber());

        return str;
    }

    constexpr Color sideToMove() const noexcept { return sideToMove_; }
    constexpr USize ply() const noexcept { return ply_; }
    constexpr UInt32 fullMoveNumber() const noexcept { return ply_ / 2 + 1; }

    constexpr CastlingRights castlingRights() const noexcept { return state().castlingRights; }
    constexpr Square enPassantSquare() const noexcept { return state().enPassantSquare; }
    constexpr UInt16 halfmoveClock() const noexcept { return state().halfmoveClock; }
    constexpr UInt16 nullPly() const noexcept { return state().nullPly; }
    constexpr UInt16 repetitionPly() const noexcept { return state().repetitionPly; }
    constexpr UInt16 repetitions() const noexcept { return state().repetitions; }

    constexpr UInt64 hash() const noexcept { return state().hash; }
    constexpr UInt64 pawnHash() const noexcept { return state().pawnHash; }
    constexpr UInt64 nonPawnHash(Color color) const noexcept { return state().nonPawnHashes[static_cast<USize>(color)]; }
    constexpr UInt64 minorPieceHash() const noexcept { return state().minorPieceHash; }
    constexpr UInt64 majorPieceHash() const noexcept { return state().majorPieceHash; }

    constexpr UInt8 checks() const noexcept { return state().checks; }
    constexpr Bitboard checkMask() const noexcept { return state().checkMask; }
    constexpr Bitboard diagonalPinMask() const noexcept { return state().diagonalPinMask; }
    constexpr Bitboard orthogonalPinMask() const noexcept { return state().orthogonalPinMask; }
    constexpr Bitboard blockMask() const noexcept { return state().blockMask; }
    constexpr Bitboard threats() const noexcept { return state().threats; }
    constexpr Bitboard winningThreats() const noexcept { return state().winningThreats; }

    constexpr const std::array<Piece, 64> &board() const noexcept { return board_; }

    constexpr Bitboard castlingPath(CastlingRights::Side castlingSide) const noexcept {
        return CASTLING_PATH_BITBOARDS[static_cast<USize>(CastlingRights::hashIndex(castlingSide))];
    }

    void make(Move move) {
        assert(move != Move::NULL_MOVE);
        assert(pieceAt(move.from()).color() == sideToMove_);

        const bool captureMove = capture(move);
        const Piece capturedPiece = pieceAt(move.to());
        const PieceType pieceType = pieceAt(move.from()).type();

        states_.push_back(state());
        state().lastMove = move;
        state().capturedPiece = capturedPiece;
        state().halfmoveClock++;
        state().nullPly++;

        ply_++;

        if (state().enPassantSquare != Square::NONE) {
            state().hash ^= Zobrist::enPassant(state().enPassantSquare.file());
            state().enPassantSquare = Square::NONE;
        }

        if (captureMove) {
            state().halfmoveClock = 0;

            if (move.type() != MoveType::EN_PASSANT) {
                removePiece<true>(capturedPiece, move.to());

                if (capturedPiece.type() == PieceType::ROOK && move.to().rank().backRank(~sideToMove_)) {
                    const Square kingSq = kingSquare(~sideToMove_);
                    const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.to(), kingSq, ~sideToMove_);
                    if (state().castlingRights.get(castlingSide) && CastlingRights::rookFrom(castlingSide) == move.to()) {
                        state().castlingRights.clear(castlingSide);
                        state().hash ^= Zobrist::castlingIndex(CastlingRights::hashIndex(castlingSide));
                    }
                }
            }
        }

        if (pieceType == PieceType::KING && state().castlingRights.get(sideToMove_)) {
            state().hash ^= Zobrist::castling(state().castlingRights.hash());
            state().castlingRights.clear(sideToMove_);
            state().hash ^= Zobrist::castling(state().castlingRights.hash());
        } else if (pieceType == PieceType::ROOK && move.from().rank().backRank(sideToMove_)) {
            const Square kingSq = kingSquare(sideToMove_);
            const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.from(), kingSq, sideToMove_);
            if (state().castlingRights.get(castlingSide) && CastlingRights::rookFrom(castlingSide) == move.from()) {
                state().castlingRights.clear(castlingSide);
                state().hash ^= Zobrist::castlingIndex(CastlingRights::hashIndex(castlingSide));
            }
        } else if (pieceType == PieceType::PAWN) {
            state().halfmoveClock = 0;

            if (Square::indexDistance(move.from(), move.to()) == 16) {
                Bitboard enPassantMask = Attacks::pawn(move.to().enPassantSquare(), sideToMove_);
                if (enPassantMask & pieces(PieceType::PAWN, ~sideToMove_)) {
                    assert(pieceAt(move.to().enPassantSquare()) == Piece::NONE);
                    state().enPassantSquare = move.to().enPassantSquare();
                    state().hash ^= Zobrist::enPassant(state().enPassantSquare.file());
                }
            }
        }

        if (move.type() == MoveType::CASTLING) {
            assert(pieceAt(move.from()).type() == PieceType::KING);
            assert(pieceAt(move.to()).type() == PieceType::ROOK);

            const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
            const Square rookTo = CastlingRights::rookTo(castlingSide);
            const Square kingTo = CastlingRights::kingTo(castlingSide);
            const Piece king = pieceAt(move.from());
            const Piece rook = pieceAt(move.to());

            assert(king == Piece(PieceType::KING, sideToMove_));
            assert(rook == Piece(PieceType::ROOK, sideToMove_));

            removePiece<true>(king, move.from());
            removePiece<true>(rook, move.to());
            placePiece<true>(king, kingTo);
            placePiece<true>(rook, rookTo);
        } else if (move.type() == MoveType::PROMOTION) {
            const Piece pawn = Piece(PieceType::PAWN, sideToMove_);
            const Piece promotionPiece = Piece(move.promotion(), sideToMove_);
            assert(promotionPiece != Piece::NONE);

            removePiece<true>(pawn, move.from());
            placePiece<true>(promotionPiece, move.to());
        } else {
            assert(pieceAt(move.from()) != Piece::NONE);
            assert(pieceAt(move.to()) == Piece::NONE);
            const Piece movedPiece = pieceAt(move.from());

            removePiece<true>(movedPiece, move.from());
            placePiece<true>(movedPiece, move.to());
        }

        if (move.type() == MoveType::EN_PASSANT) {
            assert(pieceAt(move.to().enPassantSquare()) == PieceType::PAWN);
            Piece pawn = Piece(PieceType::PAWN, ~sideToMove_);
            removePiece<true>(pawn, move.to().enPassantSquare());
        }

        sideToMove_ = ~sideToMove_;
        state().hash ^= Zobrist::sideToMove();

        updateRepetitions();
        updateChecks();
        updatePins();
        updateBlocks();
        updateThreats();
    }

    void make() {
        states_.push_back(state());
        state().lastMove = Move::NULL_MOVE;
        state().capturedPiece = Piece::NONE;
        state().halfmoveClock++;
        state().nullPly = 0;
        state().repetitionPly = 0;
        state().repetitions = 0;

        ply_++;

        if (state().enPassantSquare != Square::NONE) {
            state().hash ^= Zobrist::enPassant(state().enPassantSquare.file());
        }
        state().enPassantSquare = Square::NONE;

        sideToMove_ = ~sideToMove_;
        state().hash ^= Zobrist::sideToMove();

        updateChecks();
        updatePins();
        updateBlocks();
        updateThreats();
    }

    void unmake() noexcept {
        const Move move = state().lastMove;
        const Piece capturedPiece = state().capturedPiece;

        states_.pop_back();
        ply_--;
        sideToMove_ = ~sideToMove_;

        if (move == Move::NULL_MOVE) {
            assert(capturedPiece == Piece::NONE);
            return;
        }

        if (move.type() == MoveType::CASTLING) {
            const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
            const Square rookTo = CastlingRights::rookTo(castlingSide);
            const Square kingTo = CastlingRights::kingTo(castlingSide);
            const Piece king = Piece(PieceType::KING, sideToMove_);
            const Piece rook = Piece(PieceType::ROOK, sideToMove_);

            assert(pieceAt(kingTo) == king);
            assert(pieceAt(rookTo) == rook);
            assert(king == Piece(PieceType::KING, sideToMove_));
            assert(rook == Piece(PieceType::ROOK, sideToMove_));

            removePiece<false>(king, kingTo);
            removePiece<false>(rook, rookTo);
            placePiece<false>(king, move.from());
            placePiece<false>(rook, move.to());
        } else if (move.type() == MoveType::PROMOTION) {
            const Piece pawn = Piece(PieceType::PAWN, sideToMove_);
            const Piece promotionPiece = pieceAt(move.to());

            assert(promotionPiece.type() == move.promotion());

            removePiece<false>(promotionPiece, move.to());
            placePiece<false>(pawn, move.from());

            if (capturedPiece != Piece::NONE) {
                assert(pieceAt(move.to()) == Piece::NONE);
                placePiece<false>(capturedPiece, move.to());
            }
        } else {
            assert(pieceAt(move.to()) != Piece::NONE);
            assert(pieceAt(move.from()) == Piece::NONE);

            const Piece movedPiece = pieceAt(move.to());

            removePiece<false>(movedPiece, move.to());
            placePiece<false>(movedPiece, move.from());

            if (move.type() == MoveType::EN_PASSANT) {
                Piece pawn = Piece(PieceType::PAWN, ~sideToMove_);
                Square pawnSquare = Square(move.to().file(), move.from().rank());

                assert(pieceAt(pawnSquare) == Piece::NONE);

                placePiece<false>(pawn, pawnSquare);
            } else if (capturedPiece != Piece::NONE) {
                assert(pieceAt(move.to()) == Piece::NONE);
                placePiece<false>(capturedPiece, move.to());
            }
        }
    }

    constexpr UInt64 zobrist() const noexcept {
        UInt64 key = 0ULL;

        Bitboard pieces = occupied();
        while (pieces) {
            const Square square = pieces.pop();
            key ^= Zobrist::piece(pieceAt(square), square);
        }

        if (state().enPassantSquare != Square::NONE) {
            key ^= Zobrist::enPassant(state().enPassantSquare.file());
        }

        key ^= Zobrist::castling(state().castlingRights.hash());

        if (sideToMove_ == Color::WHITE) {
            key ^= Zobrist::sideToMove();
        }

        return key;
    }

    UInt64 zobristAfter(const Move move) const noexcept {
        UInt64 key = state().hash;

        key ^= Zobrist::sideToMove();

        if (state().enPassantSquare != Square::NONE) {
            key ^= Zobrist::enPassant(state().enPassantSquare.file());
        }

        if (move == Move::NULL_MOVE) {
            return key;
        }

        const bool captureMove = (pieceAt(move.to()) != Piece::NONE) && (move.type() != MoveType::CASTLING);
        const Piece captured = pieceAt(move.to());
        const PieceType pieceType = pieceAt(move.from()).type();

        if (captureMove) {
            key ^= Zobrist::piece(captured, move.to());

            if (captured.type() == PieceType::ROOK && move.to().rank().backRank(~sideToMove_)) {
                const Square kingSq = kingSquare(~sideToMove_);
                const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.to(), kingSq, ~sideToMove_);
                if (state().castlingRights.get(castlingSide) && CastlingRights::rookFrom(castlingSide) == move.to()) {
                    key ^= Zobrist::castlingIndex(CastlingRights::hashIndex(castlingSide));
                }
            }
        }

        if (pieceType == PieceType::KING && state().castlingRights.get(sideToMove_)) {
            const CastlingRights oldRights = state().castlingRights;
            CastlingRights newRights = oldRights;
            newRights.clear(sideToMove_);
            key ^= Zobrist::castling(oldRights.hash());
            key ^= Zobrist::castling(newRights.hash());
        } else if (pieceType == PieceType::ROOK && move.from().rank().backRank(sideToMove_)) {
            const Square kingSq = kingSquare(sideToMove_);
            const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.from(), kingSq, sideToMove_);
            if (state().castlingRights.get(castlingSide) && CastlingRights::rookFrom(castlingSide) == move.from()) {
                key ^= Zobrist::castlingIndex(CastlingRights::hashIndex(castlingSide));
            }
        } else if (pieceType == PieceType::PAWN && Square::indexDistance(move.from(), move.to()) == 16) {
            Bitboard enPassantMask = Attacks::pawn(move.to().enPassantSquare(), sideToMove_);
            if (enPassantMask & pieces(PieceType::PAWN, ~sideToMove_)) {
                assert(pieceAt(move.to().enPassantSquare()) == Piece::NONE);
                key ^= Zobrist::enPassant(move.to().enPassantSquare().file());
            }
        }

        if (move.type() == MoveType::CASTLING) {
            assert(pieceAt(move.from()).type() == PieceType::KING);
            assert(pieceAt(move.to()).type() == PieceType::ROOK);

            const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
            const Square rookTo = CastlingRights::rookTo(castlingSide);
            const Square kingTo = CastlingRights::kingTo(castlingSide);
            const Piece king = pieceAt(move.from());
            const Piece rook = pieceAt(move.to());
            assert(king == Piece(PieceType::KING, sideToMove_) && rook == Piece(PieceType::ROOK, sideToMove_));

            key ^= Zobrist::piece(king, move.from());
            key ^= Zobrist::piece(king, kingTo);
            key ^= Zobrist::piece(rook, move.to());
            key ^= Zobrist::piece(rook, rookTo);
        } else if (move.type() == MoveType::PROMOTION) {
            const Piece pawn = Piece(PieceType::PAWN, sideToMove_);
            const Piece promotionPiece = Piece(move.promotion(), sideToMove_);
            assert(promotionPiece != Piece::NONE);
            key ^= Zobrist::piece(pawn, move.from());
            key ^= Zobrist::piece(promotionPiece, move.to());
        } else {
            assert(pieceAt(move.from()) != Piece::NONE);
            const Piece movedPiece = pieceAt(move.from());
            key ^= Zobrist::piece(movedPiece, move.from());
            key ^= Zobrist::piece(movedPiece, move.to());
        }

        if (move.type() == MoveType::EN_PASSANT) {
            assert(pieceAt(move.to().enPassantSquare()) == PieceType::PAWN);
            Piece pawn = Piece(PieceType::PAWN, ~sideToMove_);
            key ^= Zobrist::piece(pawn, move.to().enPassantSquare());
        }

        return key;
    }

    constexpr Bitboard friendly(Color color) const noexcept {
        assert(color != Color::NONE);
        return occupancyBitboards_[static_cast<USize>(color)];
    }

    constexpr Bitboard enemy(Color color) const noexcept {
        assert(color != Color::NONE);
        return friendly(~color);
    }

    constexpr Bitboard occupied() const noexcept { return occupancyBitboards_[0] | occupancyBitboards_[1]; }

    constexpr Piece pieceAt(Square square) const noexcept {
        assert(square != Square::NONE);
        return board_[static_cast<USize>(square.index())];
    }

    constexpr Bitboard pieces(PieceType pieceType) const noexcept {
        assert(pieceType != PieceType::NONE);
        return pieceBitboards_[static_cast<USize>(pieceType)];
    }

    constexpr Bitboard pieces(PieceType pieceType, Color color) const noexcept {
        assert(pieceType != PieceType::NONE && color != Color::NONE);
        return pieceBitboards_[static_cast<USize>(pieceType)] & occupancyBitboards_[static_cast<USize>(color)];
    }

    constexpr Square kingSquare(Color color) const noexcept {
        assert(color != Color::NONE);
        return pieces(PieceType::KING, color).lsb();
    }

    constexpr bool attacked(Square square, Color color) const noexcept {
        assert(square != Square::NONE && color != Color::NONE);
        if (Attacks::pawn(square, ~color) & pieces(PieceType::PAWN, color)) {
            return true;
        }
        if (Attacks::knight(square) & pieces(PieceType::KNIGHT, color)) {
            return true;
        }
        if (Attacks::king(square) & pieces(PieceType::KING, color)) {
            return true;
        }
        if (Attacks::bishop(square, occupied()) & ((pieces(PieceType::BISHOP, color) | pieces(PieceType::QUEEN, color)))) {
            return true;
        }
        if (Attacks::rook(square, occupied()) & ((pieces(PieceType::ROOK, color) | pieces(PieceType::QUEEN, color)))) {
            return true;
        }

        return false;
    }

    constexpr Bitboard attackers(Square square, Color color) const noexcept {
        assert(square != Square::NONE && color != Color::NONE);
        Bitboard attacks = Attacks::pawn(square, ~color) & pieces(PieceType::PAWN, color);
        attacks |= Attacks::knight(square) & pieces(PieceType::KNIGHT, color);
        attacks |= Attacks::bishop(square, occupied()) & (pieces(PieceType::BISHOP, color) | pieces(PieceType::QUEEN, color));
        attacks |= Attacks::rook(square, occupied()) & (pieces(PieceType::ROOK, color) | pieces(PieceType::QUEEN, color));
        attacks |= Attacks::king(square) & pieces(PieceType::KING, color);
        return attacks & occupied();
    }

    constexpr bool inCheck() const noexcept { return state().checks > 0; }

    constexpr bool capture(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return (pieceAt(move.to()) != Piece::NONE && move.type() != MoveType::CASTLING) || move.type() == MoveType::EN_PASSANT;
    }

    constexpr bool noisy(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return capture(move) || move.type() == MoveType::PROMOTION;
    }

    constexpr bool quiet(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return !noisy(move);
    }

    constexpr bool check(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        const auto findSniper = [this](Square kingSq, Bitboard occ) {
            const Bitboard bishops = Attacks::bishop(kingSq, occ) & (pieces(PieceType::BISHOP, sideToMove_) | pieces(PieceType::QUEEN, sideToMove_));
            const Bitboard rooks = Attacks::rook(kingSq, occ) & (pieces(PieceType::ROOK, sideToMove_) | pieces(PieceType::QUEEN, sideToMove_));
            return (bishops | rooks);
        };

        assert(pieceAt(move.from()).color() == sideToMove_);

        const Square from = move.from();
        const Square to = move.to();
        const Square kingSq = kingSquare(~sideToMove_);
        const Bitboard toBitboard = Bitboard(to);
        const PieceType pieceType = pieceAt(move.from()).type();

        Bitboard checks = 0ULL;
        if (pieceType == PieceType::PAWN) {
            checks = Attacks::pawn(kingSq, ~sideToMove_);
        } else if (pieceType == PieceType::KNIGHT) {
            checks = Attacks::knight(kingSq);
        } else if (pieceType == PieceType::BISHOP) {
            checks = Attacks::bishop(kingSq, occupied());
        } else if (pieceType == PieceType::ROOK) {
            checks = Attacks::rook(kingSq, occupied());
        } else if (pieceType == PieceType::QUEEN) {
            checks = Attacks::queen(kingSq, occupied());
        }

        if (checks & toBitboard) {
            return true;
        }

        const Bitboard fromBitboard = Bitboard(from);
        const Bitboard occ = occupied() ^ fromBitboard;

        Bitboard sniper = findSniper(kingSq, occ);
        if (sniper) {
            if (!(Attacks::between(kingSq, sniper.lsb()) & toBitboard) || move.type() == MoveType::CASTLING) {
                return true;
            } else {
                return false;
            }
        }

        if (move.type() == MoveType::NORMAL) {
            return false;
        } else if (move.type() == MoveType::PROMOTION) {
            Bitboard attacks = 0ULL;
            if (move.promotion() == PieceType::KNIGHT) {
                attacks = Attacks::knight(to);
            } else if (move.promotion() == PieceType::BISHOP) {
                attacks = Attacks::bishop(to, occ);
            } else if (move.promotion() == PieceType::ROOK) {
                attacks = Attacks::rook(to, occ);
            } else if (move.promotion() == PieceType::QUEEN) {
                attacks = Attacks::queen(to, occ);
            } else {
                assert(false);
            }
            if (attacks & Bitboard(kingSq)) {
                return true;
            } else {
                return false;
            }
        } else if (move.type() == MoveType::EN_PASSANT) {
            Square captureSq = Square(to.file(), from.rank());
            if (findSniper(kingSq, (occ ^ Bitboard(captureSq)) | toBitboard)) {
                return true;
            } else {
                return false;
            }
        } else if (move.type() == MoveType::CASTLING) {
            const CastlingRights::Side castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
            const Square rookTo = CastlingRights::rookTo(castlingSide);
            if (Attacks::rook(kingSq, occ) & Bitboard(rookTo)) {
                return true;
            } else {
                return false;
            }
        } else {
            assert(false);
            return false;
        }
    }

    constexpr bool legal(const Move move) const noexcept {
        if (move == Move::NULL_MOVE) {
            return false;
        }

        const Square from = move.from();
        const UInt8 fromIndex = from.index();
        const Square to = move.to();
        const UInt8 toIndex = to.index();
        const Piece piece = pieceAt(from);
        const Piece capturedPiece = pieceAt(to);

        if (piece == Piece::NONE || piece.color() != sideToMove_) {
            return false;
        }

        if (capturedPiece != Piece::NONE && ((capturedPiece.color() == sideToMove_ && (move.type() != MoveType::CASTLING || piece.type() != PieceType::KING || capturedPiece != Piece(PieceType::ROOK, sideToMove_))) || capturedPiece.type() == PieceType::KING)) {
            return false;
        }

        const Bitboard occ = occupied();

        if (piece.type() == PieceType::KING && move.type() == MoveType::NORMAL) {
            if (state().threats.get(toIndex) || !Attacks::king(from).get(toIndex)) {
                return false;
            }
            return true;
        }

        const Square kingSq = kingSquare(sideToMove_);

        if (state().checks >= 2 && piece.type() != PieceType::KING) {
            return false;
        }

        if (move.type() == MoveType::CASTLING) {
            if (state().checks != 0) {
                return false;
            }

            if (!from.backRank(sideToMove_) || !to.backRank(sideToMove_)) {
                return false;
            }

            if (piece != Piece(PieceType::KING, sideToMove_) || capturedPiece != Piece(PieceType::ROOK, sideToMove_)) {
                return false;
            }

            const CastlingRights::Side castlingSide = CastlingRights::closestSide(to, from, sideToMove_);
            if (!state().castlingRights.get(castlingSide)) {
                return false;
            }

            if (castlingPath(castlingSide) & occ) {
                return false;
            }

            const Square kingTo = CastlingRights::kingTo(castlingSide);
            if (Attacks::between(from, kingTo) & state().threats) {
                return false;
            }

            return true;
        } else if (move.type() == MoveType::EN_PASSANT) {
            if (state().enPassantSquare == Square::NONE || to != state().enPassantSquare) {
                return false;
            }

            assert(capturedPiece == Piece::NONE);

            if (piece.type() != PieceType::PAWN || !Attacks::pawn(from, sideToMove_).get(toIndex)) {
                return false;
            }

            const Square captureSq = Square(to.file(), from.rank());
            if (!state().checkMask.get(captureSq) && !state().checkMask.get(toIndex)) {
                return false;
            }

            if (state().orthogonalPinMask.get(fromIndex) || (state().diagonalPinMask.get(fromIndex) && !state().diagonalPinMask.get(toIndex))) {
                return false;
            }

            const Bitboard enPassantPawns = Bitboard(from) | Bitboard(captureSq);
            const Bitboard kingMask = Bitboard(kingSq) & Bitboard(captureSq.rank());
            const Bitboard enemyRooksQueens = pieces(PieceType::ROOK, ~sideToMove_) | pieces(PieceType::QUEEN, ~sideToMove_);
            const bool possiblePin = kingMask && enemyRooksQueens;
            if (possiblePin && (Attacks::rook(kingSq, occ ^ enPassantPawns) & enemyRooksQueens)) {
                return false;
            }

            return true;
        } else if (move.type() == MoveType::PROMOTION) {
            if (piece.type() != PieceType::PAWN) {
                return false;
            }
        }

        if (!state().checkMask.get(toIndex)) {
            return false;
        }

        if (piece.type() == PieceType::PAWN) {
            const Rank promotionRank = Rank(Rank::RANK_8, sideToMove_);
            if ((to.rank() == promotionRank) != (move.type() == MoveType::PROMOTION)) {
                return false;
            }

            if (capturedPiece != Piece::NONE) {
                if (state().orthogonalPinMask.get(fromIndex) || (state().diagonalPinMask.get(fromIndex) && !state().diagonalPinMask.get(toIndex))) {
                    return false;
                }

                return Attacks::pawn(from, sideToMove_).get(toIndex);
            }

            if (state().diagonalPinMask.get(fromIndex) || (state().orthogonalPinMask.get(fromIndex) && !state().orthogonalPinMask.get(toIndex))) {
                return false;
            }

            const Direction up = Direction(Direction::NORTH, sideToMove_);
            const Rank startRank = Rank(Rank::RANK_2, sideToMove_);

            if (to == from + up) {
                return true;
            }

            if (from.rank() == startRank && to == from + up + up) {
                return pieceAt(from + up) == Piece::NONE;
            }

            return false;
        } else if (piece.type() == PieceType::KNIGHT) {
            if (state().orthogonalPinMask.get(fromIndex) || state().diagonalPinMask.get(fromIndex)) {
                return false;
            }
            return Attacks::knight(from).get(toIndex);
        } else if (piece.type() == PieceType::BISHOP) {
            if (state().orthogonalPinMask.get(fromIndex) || (state().diagonalPinMask.get(fromIndex) && !state().diagonalPinMask.get(toIndex))) {
                return false;
            }
            return Attacks::bishop(from, occ).get(toIndex);
        } else if (piece.type() == PieceType::ROOK) {
            if (state().diagonalPinMask.get(fromIndex) || (state().orthogonalPinMask.get(fromIndex) && !state().orthogonalPinMask.get(toIndex))) {
                return false;
            }
            return Attacks::rook(from, occ).get(toIndex);
        } else if (piece.type() == PieceType::QUEEN) {
            if (state().diagonalPinMask.get(fromIndex)) {
                return (Attacks::bishop(from, occ) & state().diagonalPinMask).get(toIndex);
            }

            if (state().orthogonalPinMask.get(fromIndex)) {
                return (Attacks::rook(from, occ) & state().orthogonalPinMask).get(toIndex);
            }

            return Attacks::queen(from, occ).get(toIndex);
        } else {
            assert(false);
            return false;
        }

        return false;
    }

    constexpr Piece moved(const Move move) const noexcept {
        return pieceAt(move.from());
    }

    constexpr Piece captured(const Move move) const noexcept {
        if (move.type() == MoveType::EN_PASSANT) {
            return Piece(PieceType::PAWN, ~sideToMove_);
        }

        if (move.type() == MoveType::CASTLING) {
            return Piece::NONE;
        }

        return pieceAt(move.to());
    }

    constexpr bool nonPawnMaterial(Color color) const noexcept {
        assert(color != Color::NONE);
        return bool(friendly(color) ^ (pieces(PieceType::PAWN, color) | pieces(PieceType::KING, color)));
    }

    bool repetition3Fold(USize searchPly) const noexcept { return state().repetitions > 1 || (state().repetitions == 1 && static_cast<USize>(state().repetitionPly) < searchPly); }

    bool upcomingRepetition(USize searchPly) const noexcept {
        if (states_.size() <= 3) {
            return false;
        }

        UInt64 currHash = state().hash;
        UInt64 diff = currHash ^ states_[states_.size() - 2].hash ^ Zobrist::sideToMove();

        USize reversible = static_cast<USize>(std::min(state().halfmoveClock, state().nullPly));
        for (USize i = 3; i <= reversible; i += 2) {
            const State &pastState = states_[states_.size() - i - 1];
            diff ^= states_[states_.size() - i].hash ^ pastState.hash ^ Zobrist::sideToMove();
            if (diff != 0) {
                continue;
            }

            UInt64 moveHash = currHash ^ pastState.hash;
            const Move move = CuckooTable::probe(moveHash);
            if (move == Move::NULL_MOVE) {
                continue;
            }

            if (!((Attacks::between(move.from(), move.to()) ^ Bitboard(move.to())) & occupied())) {
                if (searchPly > i) {
                    return true;
                }

                if (pastState.repetitions > 0) {
                    return true;
                }

            }

        }
        return false;
    }

    constexpr bool halfMoveDraw(bool noMoves) const noexcept { return state().halfmoveClock >= 100 && !(inCheck() && noMoves); }

    constexpr bool insufficientMaterial() const noexcept {
        const UInt8 count = occupied().count();

        if (count == 2) {
            return true;
        }

        if (count == 3) {
            if (pieces(PieceType::BISHOP) || pieces(PieceType::KNIGHT)) {
                return true;
            }
        }

        if (count == 4) {
            if (pieces(PieceType::BISHOP, Color::WHITE) && pieces(PieceType::BISHOP, Color::BLACK) && Square::sameColor(pieces(PieceType::BISHOP, Color::WHITE).lsb(), pieces(PieceType::BISHOP, Color::BLACK).lsb())) {
                return true;
            }

            const Bitboard whiteBishops = pieces(PieceType::BISHOP, Color::WHITE);
            const Bitboard blackBishops = pieces(PieceType::BISHOP, Color::BLACK);
            if (whiteBishops.count() == 2) {
                if (Square::sameColor(whiteBishops.lsb(), whiteBishops.msb())) {
                    return true;
                }
            } else if (blackBishops.count() == 2) {
                if (Square::sameColor(blackBishops.lsb(), blackBishops.msb())) {
                    return true;
                }
            }
        }

        return false;
    }

    constexpr bool draw(USize searchPly, bool noMoves) const noexcept {
        return halfMoveDraw(noMoves) || insufficientMaterial() || repetition3Fold(searchPly);
    }

private:
    static constexpr USize MAX_STATES = 2048;

    static constexpr std::array<Bitboard, 4> CASTLING_PATH_BITBOARDS = {
        Bitboard(Square::SQUARE_F1) | Bitboard(Square::SQUARE_G1),
        Bitboard(Square::SQUARE_B1) | Bitboard(Square::SQUARE_C1) | Bitboard(Square::SQUARE_D1),
        Bitboard(Square::SQUARE_F8) | Bitboard(Square::SQUARE_G8),
        Bitboard(Square::SQUARE_B8) | Bitboard(Square::SQUARE_C8) | Bitboard(Square::SQUARE_D8)
    };

    struct State {
        Move lastMove;
        Piece capturedPiece;

        CastlingRights castlingRights;
        Square enPassantSquare;
        UInt16 halfmoveClock;
        UInt16 nullPly;

        UInt16 repetitionPly;
        UInt8 repetitions;

        UInt64 hash;
        UInt64 pawnHash;
        std::array<UInt64, 2> nonPawnHashes;
        UInt64 minorPieceHash;
        UInt64 majorPieceHash;

        UInt8 checks;
        Bitboard checkMask;
        Bitboard diagonalPinMask;
        Bitboard orthogonalPinMask;
        Bitboard blockMask;

        Bitboard threats;
        Bitboard winningThreats;
    };

    std::vector<State> states_;

    std::array<Bitboard, 6> pieceBitboards_;
    std::array<Bitboard, 2> occupancyBitboards_;
    std::array<Piece, 64> board_;

    Color sideToMove_;
    UInt16 ply_;

    constexpr State &state() noexcept { return states_.back(); }
    constexpr const State &state() const noexcept { return states_.back(); }

    template<bool UPDATE_HASH>
    void placePiece(Piece piece, Square square) noexcept {
        assert(piece != Piece::NONE && square != Square::NONE);
        assert(pieceAt(square) == Piece::NONE);

        const PieceType pieceType = piece.type();
        const Color color = piece.color();
        const UInt8 index = square.index();

        if constexpr (UPDATE_HASH) {
            state().hash ^= Zobrist::piece(piece, square);
            if (pieceType == PieceType::PAWN) {
                state().pawnHash ^= Zobrist::piece(piece, square);
            } else {
                state().nonPawnHashes[static_cast<USize>(color)] ^= Zobrist::piece(piece, square);
                if (pieceType == PieceType::KNIGHT || pieceType == PieceType::BISHOP || pieceType == PieceType::KING) {
                    state().minorPieceHash ^= Zobrist::piece(piece, square);
                } else if (pieceType == PieceType::ROOK || pieceType == PieceType::QUEEN || pieceType == PieceType::KING) {
                    state().majorPieceHash ^= Zobrist::piece(piece, square);
                }
            }
        }

        pieceBitboards_[static_cast<USize>(pieceType)].set(index);
        occupancyBitboards_[static_cast<USize>(color)].set(index);
        board_[static_cast<USize>(index)] = piece;
    }

    template<bool UPDATE_HASH>
    void removePiece(Piece piece, Square square) noexcept {
        assert(piece != Piece::NONE && square != Square::NONE);
        assert(pieceAt(square) == piece);

        const PieceType pieceType = piece.type();
        const Color color = piece.color();
        const UInt8 index = square.index();

        if constexpr (UPDATE_HASH) {
            state().hash ^= Zobrist::piece(piece, square);
            if (pieceType == PieceType::PAWN) {
                state().pawnHash ^= Zobrist::piece(piece, square);
            } else {
                state().nonPawnHashes[static_cast<USize>(color)] ^= Zobrist::piece(piece, square);
                if (pieceType == PieceType::KNIGHT || pieceType == PieceType::BISHOP || pieceType == PieceType::KING) {
                    state().minorPieceHash ^= Zobrist::piece(piece, square);
                } else if (pieceType == PieceType::ROOK || pieceType == PieceType::QUEEN || pieceType == PieceType::KING) {
                    state().majorPieceHash ^= Zobrist::piece(piece, square);
                }
            }
        }

        pieceBitboards_[static_cast<USize>(pieceType)].clear(index);
        occupancyBitboards_[static_cast<USize>(color)].clear(index);
        board_[static_cast<USize>(index)] = Piece::NONE;
    }

    constexpr void updateRepetitions() noexcept {
        state().repetitionPly = 0;
        state().repetitions = 0;

        if (states_.size() <= 4) {
            return;
        }

        USize reversible = static_cast<USize>(std::min(state().halfmoveClock, state().nullPly));
        for (USize i = 4; i <= reversible; i += 2) {
            State &pastState = states_[states_.size() - i - 1];
            if (pastState.hash == state().hash) {
                state().repetitionPly = static_cast<UInt16>(i);
                state().repetitions = pastState.repetitions + 1;
                return;
            }
        }
    }

    void updateChecks() noexcept {
        const Square kingSq = kingSquare(sideToMove_);
        const Bitboard occ = occupied();

        const Bitboard pawns = pieces(PieceType::PAWN, ~sideToMove_);
        const Bitboard knights = pieces(PieceType::KNIGHT, ~sideToMove_);
        const Bitboard bishops = pieces(PieceType::BISHOP, ~sideToMove_);
        const Bitboard rooks = pieces(PieceType::ROOK, ~sideToMove_);
        const Bitboard queens = pieces(PieceType::QUEEN, ~sideToMove_);

        Bitboard mask = Bitboard();
        UInt8 checks = 0;

        Bitboard pawnMask = Attacks::pawn(kingSq, sideToMove_) & pawns;
        mask |= pawnMask;
        checks += pawnMask.count();

        Bitboard knightMask = Attacks::knight(kingSq) & knights;
        mask |= knightMask;
        checks += knightMask.count();

        Bitboard bishopMask = Attacks::bishop(kingSq, occ) & (bishops | queens);
        while (bishopMask) {
            mask |= Attacks::between(kingSq, bishopMask.pop());
            checks++;
        }

        Bitboard rookMask = Attacks::rook(kingSq, occ) & (rooks | queens);
        while (rookMask) {
            mask |= Attacks::between(kingSq, rookMask.pop());
            checks++;
        }

        if (!mask) {
            mask = Bitboard(0xFFFFFFFFFFFFFFFFULL);
        }

        state().checks = checks;
        state().checkMask = mask;
    }

    void updatePins() noexcept {
        state().diagonalPinMask = pinMask<PieceType::BISHOP>();
        state().orthogonalPinMask = pinMask<PieceType::ROOK>();
    }

    void updateBlocks() noexcept {

        const Square enemyKingSq = kingSquare(~sideToMove_);
        const Bitboard friendlyOcc = friendly(sideToMove_);
        const Bitboard enemyOcc = enemy(sideToMove_);

        const Bitboard bishops = Attacks::bishop(enemyKingSq, enemyOcc) & (pieces(PieceType::BISHOP) | pieces(PieceType::QUEEN)) & friendlyOcc;
        const Bitboard rooks = Attacks::rook(enemyKingSq, enemyOcc) & (pieces(PieceType::ROOK) | pieces(PieceType::QUEEN)) & friendlyOcc;
        Bitboard snipers = bishops | rooks;
        Bitboard mask = Bitboard();
        while (snipers) {
            const Square sniperSquare = snipers.pop();
            const Bitboard possibleBlock = Attacks::between(enemyKingSq, sniperSquare) ^ Bitboard(sniperSquare);
            if ((possibleBlock & friendlyOcc).count() == 1) {
                mask |= possibleBlock;
            }
        }

        state().blockMask = mask;
    }

    void updateThreats() noexcept {
        Bitboard threats = Bitboard();
        Bitboard winningThreats = Bitboard();
        Bitboard targets = Bitboard();

        Bitboard occ = occupied() ^ Bitboard(kingSquare(sideToMove_));

        Bitboard pawns = pieces(PieceType::PAWN, ~sideToMove_);
        Bitboard knights = pieces(PieceType::KNIGHT, ~sideToMove_);
        Bitboard bishops = pieces(PieceType::BISHOP, ~sideToMove_);
        Bitboard rooks = pieces(PieceType::ROOK, ~sideToMove_);
        Bitboard queens = pieces(PieceType::QUEEN, ~sideToMove_);

        while (queens) {
            threats |= Attacks::queen(queens.pop(), occ);
        }
        targets |= pieces(PieceType::QUEEN, sideToMove_);

        while (rooks) {
            const Bitboard attacks = Attacks::rook(rooks.pop(), occ);
            threats |= attacks;
            winningThreats |= attacks & targets;
        }
        targets |= pieces(PieceType::ROOK, sideToMove_);

        while (bishops) {
            const Bitboard attacks = Attacks::bishop(bishops.pop(), occ);
            threats |= attacks;
            winningThreats |= attacks & targets;
        }

        while (knights) {
            const Bitboard attacks = Attacks::knight(knights.pop());
            threats |= attacks;
            winningThreats |= attacks & targets;
        }

        targets |= pieces(PieceType::BISHOP, sideToMove_) | pieces(PieceType::KNIGHT, sideToMove_);

        const Bitboard pawnAttacks = (sideToMove_ == Color::WHITE) ? Attacks::allPawns<Color::BLACK>(pawns) : Attacks::allPawns<Color::WHITE>(pawns);
        threats |= pawnAttacks;
        winningThreats |= pawnAttacks & targets;

        threats |= Attacks::king(kingSquare(~sideToMove_));

        state().threats = threats;
        state().winningThreats = winningThreats;
    }

    template<PieceType::PieceTypeEnum PIECE_TYPE_ENUM>
    Bitboard pinMask() const noexcept {
        static_assert(PIECE_TYPE_ENUM == PieceType::BISHOP || PIECE_TYPE_ENUM == PieceType::ROOK);

        const Square kingSq = kingSquare(sideToMove_);
        const Bitboard friendlyOcc = friendly(sideToMove_);
        const Bitboard enemyOcc = enemy(sideToMove_);

        Bitboard sliders = Attacks::slider<PIECE_TYPE_ENUM>(kingSq, enemyOcc) & (pieces(PIECE_TYPE_ENUM) | pieces(PieceType::QUEEN)) & enemyOcc;
        Bitboard mask = Bitboard();
        while (sliders) {
            const Bitboard possiblePin = Attacks::between(kingSq, sliders.pop());
            if ((possiblePin & friendlyOcc).count() == 1) {
                mask |= possiblePin;
            }
        }
        return mask;
    }
};

}
