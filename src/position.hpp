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

class Position {
public:
    class CastlingRights {
    public:
        enum class CastlingSide : UInt8 {
            WHITE_KINGSIDE = 1 << 0,
            WHITE_QUEENSIDE = 1 << 1,
            BLACK_KINGSIDE = 1 << 2,
            BLACK_QUEENSIDE = 1 << 3
        };

        static constexpr CastlingSide WHITE_KINGSIDE = CastlingSide::WHITE_KINGSIDE;
        static constexpr CastlingSide WHITE_QUEENSIDE = CastlingSide::WHITE_QUEENSIDE;
        static constexpr CastlingSide BLACK_KINGSIDE = CastlingSide::BLACK_KINGSIDE;
        static constexpr CastlingSide BLACK_QUEENSIDE = CastlingSide::BLACK_QUEENSIDE;

        constexpr CastlingRights() noexcept : rights_(0) {}
        constexpr CastlingRights(CastlingSide side) noexcept : rights_(static_cast<UInt8>(side)) {}
        constexpr CastlingRights(UInt8 rights) noexcept : rights_(rights) { assert(rights < 16); }

        constexpr bool operator==(const CastlingRights &other) const noexcept { return rights_ == other.rights_; }
        constexpr operator UInt8() const noexcept { return rights_; }

        constexpr void set(CastlingSide side) noexcept { rights_ |= static_cast<UInt8>(side); }
        constexpr bool get(CastlingSide side) const noexcept { return (rights_ & static_cast<UInt8>(side)) != 0; }

        constexpr bool get(Color color) const noexcept {
            assert(color != Color::NONE);
            if (color == Color::WHITE) {
                return get(CastlingSide::WHITE_KINGSIDE) || get(CastlingSide::WHITE_QUEENSIDE);
            } else {
                return get(CastlingSide::BLACK_KINGSIDE) || get(CastlingSide::BLACK_QUEENSIDE);
            }
        }

        constexpr void clear() noexcept { rights_ = 0; }

        constexpr void clear(CastlingSide side) noexcept { rights_ &= ~static_cast<UInt8>(side); }

        constexpr void clear(Color color) noexcept {
            assert(color != Color::NONE);
            if (color == Color::WHITE) {
                clear(CastlingSide::WHITE_KINGSIDE);
                clear(CastlingSide::WHITE_QUEENSIDE);
            } else {
                clear(CastlingSide::BLACK_KINGSIDE);
                clear(CastlingSide::BLACK_QUEENSIDE);
            }
        }

        constexpr bool empty() const noexcept { return rights_ == 0; }

        constexpr UInt8 hash() const noexcept { return rights_; }
        static constexpr UInt8 hashIndex(CastlingSide side) noexcept { return static_cast<UInt8>(std::countr_zero(static_cast<UInt8>(side))); }

        static constexpr Color color(CastlingSide side) noexcept {
            if (side == CastlingSide::WHITE_KINGSIDE || side == CastlingSide::WHITE_QUEENSIDE) {
                return Color::WHITE;
            } else if (side == CastlingSide::BLACK_KINGSIDE || side == CastlingSide::BLACK_QUEENSIDE) {
                return Color::BLACK;
            } else {
                assert(false);
                return Color::NONE;
            }
        }

        static constexpr bool kingside(CastlingSide side) noexcept {
            return side == CastlingSide::WHITE_KINGSIDE || side == CastlingSide::BLACK_KINGSIDE;
        }

        static constexpr CastlingSide closestSide(Square square, Square kingSquare, Color color) noexcept {
            assert(square != Square::NONE && kingSquare != Square::NONE && color != Color::NONE);
            UInt8 shift = 0;
            if (color == Color::BLACK) {
                shift = 2;
            }
            if (square < kingSquare) {
                shift += 1;
            }
            return static_cast<CastlingSide>(1 << shift);
        }

        static constexpr Square rookFrom(CastlingSide side) noexcept {
            if (kingside(side)) {
                return Square(Square::SQUARE_H1, color(side));
            } else {
                return Square(Square::SQUARE_A1, color(side));
            }
        }

        static constexpr Square kingTo(CastlingSide side) noexcept {
            if (kingside(side)) {
                return Square(Square::SQUARE_G1, color(side));
            } else {
                return Square(Square::SQUARE_C1, color(side));
            }
        }

        static constexpr Square rookTo(CastlingSide side) noexcept {
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

    static constexpr const std::string_view FEN_STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    static constexpr USize MAX_POSITION_DEPTH = 1024;

    explicit Position(std::string_view fen = FEN_STARTPOS) {
        if (!set(fen)) {
            assert(false);
            reset();
        }
    }

    void reset() noexcept {
        occupancyBitboards_.fill(0ULL);
        pieceBitboards_.fill(0ULL);
        board_.fill(Piece::NONE);

        hash_ = 0ULL;
        castlingRights_.clear();
        enPassantSquare_ = Square::NONE;
        sideToMove_ = Color::WHITE;
        halfmoveClock_ = 0;
        pliesFromNull_ = 0;
        plies_ = 0;
        repetitionPly_ = 0;
        repetitions_ = 0;

        depth_ = 0;

        castlingPathBitboards_ = {
            Bitboard(Square::SQUARE_F1) | Bitboard(Square::SQUARE_G1),
            Bitboard(Square::SQUARE_B1) | Bitboard(Square::SQUARE_C1) | Bitboard(Square::SQUARE_D1),
            Bitboard(Square::SQUARE_F8) | Bitboard(Square::SQUARE_G8),
            Bitboard(Square::SQUARE_B8) | Bitboard(Square::SQUARE_C8) | Bitboard(Square::SQUARE_D8)
        };
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
                placePiece(piece, Square(static_cast<UInt8>(index)));
                hash_ ^= Zobrist::piece(piece, Square(static_cast<UInt8>(index)));
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
            hash_ ^= Zobrist::sideToMove();
        }

        if (castling != "-" && !castling.empty()) {
            for (char c : castling) {
                if (c == 'K') {
                    castlingRights_.set(CastlingRights::WHITE_KINGSIDE);
                } else if (c == 'Q') {
                    castlingRights_.set(CastlingRights::WHITE_QUEENSIDE);
                } else if (c == 'k') {
                    castlingRights_.set(CastlingRights::BLACK_KINGSIDE);
                } else if (c == 'q') {
                    castlingRights_.set(CastlingRights::BLACK_QUEENSIDE);
                } else {
                    return false;
                }
            }
            hash_ ^= Zobrist::castling(castlingRights_.hash());
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

            enPassantSquare_ = Square(enPassant);
            hash_ ^= Zobrist::enPassant(enPassantSquare_.file());
        }

        halfmoveClock_ = static_cast<UInt16>(std::stoi(std::string(halfmoves)));
        plies_ = static_cast<UInt16>((std::stoi(std::string(fullmoves)) - 1) * 2 + (sideToMove_ == Color::BLACK ? 1 : 0));

        assert(hash_ == zobrist());

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
        if (castlingRights_.empty()) {
            str += '-';
        } else {
            if (castlingRights_.get(CastlingRights::WHITE_KINGSIDE)) {
                str += 'K';
            }
            if (castlingRights_.get(CastlingRights::WHITE_QUEENSIDE)) {
                str += 'Q';
            }
            if (castlingRights_.get(CastlingRights::BLACK_KINGSIDE)) {
                str += 'k';
            }
            if (castlingRights_.get(CastlingRights::BLACK_QUEENSIDE)) {
                str += 'q';
            }
        }

        str += ' ';
        if (enPassantSquare_ == Square::NONE) {
            str += '-';
        } else {
            str += static_cast<std::string>(enPassantSquare_);
        }

        str += ' ';
        str += std::to_string(halfmoveClock_);
        str += ' ';
        str += std::to_string(fullMoveNumber());

        return str;
    }

    constexpr UInt64 hash() const noexcept { return hash_; }
    constexpr CastlingRights castlingRights() const noexcept { return castlingRights_; }
    constexpr Square enPassantSquare() const noexcept { return enPassantSquare_; }
    constexpr Color sideToMove() const noexcept { return sideToMove_; }
    constexpr UInt16 halfmoveClock() const noexcept { return halfmoveClock_; }
    constexpr UInt16 pliesFromNull() const noexcept { return pliesFromNull_; }
    constexpr UInt32 fullMoveNumber() const noexcept { return plies_ / 2 + 1; }

    constexpr USize depth() const noexcept { return depth_; }

    constexpr const std::array<Piece, 64> &board() const noexcept { return board_; }

    constexpr Bitboard castlingPath(CastlingRights::CastlingSide castlingSide) const noexcept {
        return castlingPathBitboards_[static_cast<USize>(CastlingRights::hashIndex(castlingSide))];
    }

    void placePiece(Piece piece, Square square) noexcept {
        assert(piece != Piece::NONE && square != Square::NONE);
        assert(pieceAt(square) == Piece::NONE);
        const PieceType pieceType = piece.type();
        const Color color = piece.color();
        const UInt8 index = square.index();
        pieceBitboards_[static_cast<USize>(pieceType)].set(index);
        occupancyBitboards_[static_cast<USize>(color)].set(index);
        board_[static_cast<USize>(index)] = piece;
    }

    void removePiece(Piece piece, Square square) noexcept {
        assert(piece != Piece::NONE && square != Square::NONE);
        assert(pieceAt(square) == piece);
        const PieceType pieceType = piece.type();
        const Color color = piece.color();
        const UInt8 index = square.index();
        pieceBitboards_[static_cast<USize>(pieceType)].clear(index);
        occupancyBitboards_[static_cast<USize>(color)].clear(index);
        board_[static_cast<USize>(index)] = Piece::NONE;
    }

    void make(Move move) {
        assert(move != Move::NULL_MOVE);
        assert(pieceAt(move.from()).color() == sideToMove_);

        const bool captureMove = capture(move);
        const Piece capturedPiece = pieceAt(move.to());
        const PieceType pieceType = pieceAt(move.from()).type();

        stateHistory_[depth_++] = BoardState{hash_, castlingRights_, enPassantSquare_, halfmoveClock_, pliesFromNull_, repetitionPly_, repetitions_, capturedPiece};

        halfmoveClock_++;
        pliesFromNull_++;
        plies_++;

        if (enPassantSquare_ != Square::NONE) {
            hash_ ^= Zobrist::enPassant(enPassantSquare_.file());
            enPassantSquare_ = Square::NONE;
        }

        if (captureMove) {
            halfmoveClock_ = 0;

            if (move.type() != MoveType::EN_PASSANT) {
                removePiece(capturedPiece, move.to());
                hash_ ^= Zobrist::piece(capturedPiece, move.to());

                if (capturedPiece.type() == PieceType::ROOK && move.to().rank().backRank(~sideToMove_)) {
                    const Square kingSq = kingSquare(~sideToMove_);
                    const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), kingSq, ~sideToMove_);
                    if (castlingRights_.get(castlingSide) && CastlingRights::rookFrom(castlingSide) == move.to()) {
                        castlingRights_.clear(castlingSide);
                        hash_ ^= Zobrist::castlingIndex(CastlingRights::hashIndex(castlingSide));
                    }
                }
            }
        }

        if (pieceType == PieceType::KING && castlingRights_.get(sideToMove_)) {
            hash_ ^= Zobrist::castling(castlingRights_.hash());
            castlingRights_.clear(sideToMove_);
            hash_ ^= Zobrist::castling(castlingRights_.hash());
        } else if (pieceType == PieceType::ROOK && move.from().rank().backRank(sideToMove_)) {
            const Square kingSq = kingSquare(sideToMove_);
            const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.from(), kingSq, sideToMove_);
            if (castlingRights_.get(castlingSide) && CastlingRights::rookFrom(castlingSide) == move.from()) {
                castlingRights_.clear(castlingSide);
                hash_ ^= Zobrist::castlingIndex(CastlingRights::hashIndex(castlingSide));
            }
        } else if (pieceType == PieceType::PAWN) {
            halfmoveClock_ = 0;

            if (Square::indexDistance(move.from(), move.to()) == 16) {
                Bitboard enPassantMask = Attacks::pawn(move.to().enPassantSquare(), sideToMove_);
                if (enPassantMask & pieces(PieceType::PAWN, ~sideToMove_)) {
                    assert(pieceAt(move.to().enPassantSquare()) == Piece::NONE);
                    enPassantSquare_ = move.to().enPassantSquare();
                    hash_ ^= Zobrist::enPassant(enPassantSquare_.file());
                }
            }
        }

        if (move.type() == MoveType::CASTLING) {
            assert(pieceAt(move.from()).type() == PieceType::KING);
            assert(pieceAt(move.to()).type() == PieceType::ROOK);

            const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
            const Square rookTo = CastlingRights::rookTo(castlingSide);
            const Square kingTo = CastlingRights::kingTo(castlingSide);
            const Piece king = pieceAt(move.from());
            const Piece rook = pieceAt(move.to());

            assert(king == Piece(PieceType::KING, sideToMove_));
            assert(rook == Piece(PieceType::ROOK, sideToMove_));

            removePiece(king, move.from());
            removePiece(rook, move.to());
            placePiece(king, kingTo);
            placePiece(rook, rookTo);

            hash_ ^= Zobrist::piece(king, move.from());
            hash_ ^= Zobrist::piece(king, kingTo);
            hash_ ^= Zobrist::piece(rook, move.to());
            hash_ ^= Zobrist::piece(rook, rookTo);
        } else if (move.type() == MoveType::PROMOTION) {
            const Piece pawn = Piece(PieceType::PAWN, sideToMove_);
            const Piece promotionPiece = Piece(move.promotion(), sideToMove_);
            assert(promotionPiece != Piece::NONE);

            removePiece(pawn, move.from());
            placePiece(promotionPiece, move.to());

            hash_ ^= Zobrist::piece(pawn, move.from());
            hash_ ^= Zobrist::piece(promotionPiece, move.to());
        } else {
            assert(pieceAt(move.from()) != Piece::NONE);
            assert(pieceAt(move.to()) == Piece::NONE);
            const Piece movedPiece = pieceAt(move.from());

            removePiece(movedPiece, move.from());
            placePiece(movedPiece, move.to());

            hash_ ^= Zobrist::piece(movedPiece, move.from());
            hash_ ^= Zobrist::piece(movedPiece, move.to());
        }

        if (move.type() == MoveType::EN_PASSANT) {
            assert(pieceAt(move.to().enPassantSquare()) == PieceType::PAWN);
            Piece pawn = Piece(PieceType::PAWN, ~sideToMove_);
            removePiece(pawn, move.to().enPassantSquare());
            hash_ ^= Zobrist::piece(pawn, move.to().enPassantSquare());
        }

        sideToMove_ = ~sideToMove_;
        hash_ ^= Zobrist::sideToMove();

        repetitionPly_ = 0;
        repetitions_ = 0;

        if (depth_ > 4) {
            USize reversible = static_cast<USize>(std::min(halfmoveClock_, pliesFromNull_));
            for (USize i = 3; i <= reversible && depth_ >= i + 1; i += 2) {
                const BoardState &state = stateHistory_[depth_ - i - 1];
                if (state.hash == hash_) {
                    repetitionPly_ = static_cast<UInt16>(i);
                    repetitions_ = state.repetitions + 1;
                    break;
                }
            }
        }
    }

    void unmake(const Move move) noexcept {
        assert(move != Move::NULL_MOVE);

        const BoardState &previousState = stateHistory_[depth_ - 1];

        hash_ = previousState.hash;
        castlingRights_ = previousState.castlingRights;
        enPassantSquare_ = previousState.enPassantSquare;
        halfmoveClock_ = previousState.halfmoveClock;
        pliesFromNull_ = previousState.pliesFromNull;
        repetitionPly_ = previousState.repetitionPly;
        repetitions_ = previousState.repetitions;
        plies_--;
        sideToMove_ = ~sideToMove_;

        if (move.type() == MoveType::CASTLING) {
            const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
            const Square rookTo = CastlingRights::rookTo(castlingSide);
            const Square kingTo = CastlingRights::kingTo(castlingSide);
            const Piece king = Piece(PieceType::KING, sideToMove_);
            const Piece rook = Piece(PieceType::ROOK, sideToMove_);

            assert(pieceAt(kingTo) == king);
            assert(pieceAt(rookTo) == rook);
            assert(king == Piece(PieceType::KING, sideToMove_));
            assert(rook == Piece(PieceType::ROOK, sideToMove_));

            removePiece(king, kingTo);
            removePiece(rook, rookTo);
            placePiece(king, move.from());
            placePiece(rook, move.to());
        } else if (move.type() == MoveType::PROMOTION) {
            const Piece pawn = Piece(PieceType::PAWN, sideToMove_);
            const Piece promotionPiece = pieceAt(move.to());

            assert(promotionPiece.type() == move.promotion());

            removePiece(promotionPiece, move.to());
            placePiece(pawn, move.from());

            if (previousState.capturedPiece != Piece::NONE) {
                assert(pieceAt(move.to()) == Piece::NONE);
                placePiece(previousState.capturedPiece, move.to());
            }
        } else {
            assert(pieceAt(move.to()) != Piece::NONE);
            assert(pieceAt(move.from()) == Piece::NONE);

            const Piece movedPiece = pieceAt(move.to());

            removePiece(movedPiece, move.to());
            placePiece(movedPiece, move.from());

            if (move.type() == MoveType::EN_PASSANT) {
                Piece pawn = Piece(PieceType::PAWN, ~sideToMove_);
                Square pawnSquare = Square(move.to().file(), move.from().rank());

                assert(pieceAt(pawnSquare) == Piece::NONE);

                placePiece(pawn, pawnSquare);
            } else if (previousState.capturedPiece != Piece::NONE) {
                assert(pieceAt(move.to()) == Piece::NONE);
                placePiece(previousState.capturedPiece, move.to());
            }
        }

        depth_--;
    }

    void makeNull() {
        stateHistory_[depth_++] = BoardState{hash_, castlingRights_, enPassantSquare_, halfmoveClock_, pliesFromNull_, repetitionPly_, repetitions_, Piece::NONE};

        if (enPassantSquare_ != Square::NONE) {
            hash_ ^= Zobrist::enPassant(enPassantSquare_.file());
            enPassantSquare_ = Square::NONE;
        }

        sideToMove_ = ~sideToMove_;
        hash_ ^= Zobrist::sideToMove();

        pliesFromNull_ = 0;
        repetitionPly_ = 0;
        repetitions_ = 0;
        plies_++;
    }

    void unmakeNull() noexcept {
        const BoardState &previousState = stateHistory_[depth_ - 1];

        hash_ = previousState.hash;
        castlingRights_ = previousState.castlingRights;
        enPassantSquare_ = previousState.enPassantSquare;
        halfmoveClock_ = previousState.halfmoveClock;
        pliesFromNull_ = previousState.pliesFromNull;
        repetitionPly_ = previousState.repetitionPly;
        repetitions_ = previousState.repetitions;
        plies_--;
        sideToMove_ = ~sideToMove_;

        depth_--;
    }

    constexpr UInt64 zobrist() const noexcept {
        UInt64 key = 0ULL;

        Bitboard pieces = occupied();
        while (pieces) {
            const Square square = pieces.pop();
            key ^= Zobrist::piece(pieceAt(square), square);
        }

        if (enPassantSquare_ != Square::NONE) {
            key ^= Zobrist::enPassant(enPassantSquare_.file());
        }

        key ^= Zobrist::castling(castlingRights_.hash());

        if (sideToMove_ == Color::WHITE) {
            key ^= Zobrist::sideToMove();
        }

        return key;
    }

    UInt64 zobristAfter(const Move move) const noexcept {
        UInt64 key = hash_;

        key ^= Zobrist::sideToMove();

        if (enPassantSquare_ != Square::NONE) {
            key ^= Zobrist::enPassant(enPassantSquare_.file());
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
                const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), kingSq, ~sideToMove_);
                if (castlingRights_.get(castlingSide) && CastlingRights::rookFrom(castlingSide) == move.to()) {
                    key ^= Zobrist::castlingIndex(CastlingRights::hashIndex(castlingSide));
                }
            }
        }

        if (pieceType == PieceType::KING && castlingRights_.get(sideToMove_)) {
            const CastlingRights oldRights = castlingRights_;
            CastlingRights newRights = oldRights;
            newRights.clear(sideToMove_);
            key ^= Zobrist::castling(oldRights.hash());
            key ^= Zobrist::castling(newRights.hash());
        } else if (pieceType == PieceType::ROOK && move.from().rank().backRank(sideToMove_)) {
            const Square kingSq = kingSquare(sideToMove_);
            const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.from(), kingSq, sideToMove_);
            if (castlingRights_.get(castlingSide) && CastlingRights::rookFrom(castlingSide) == move.from()) {
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

            const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
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

    template<Color::ColorEnum COLOR_ENUM>
    std::pair<Bitboard, UInt8> checkMask() const noexcept {
        static_assert(COLOR_ENUM != Color::NONE);
        constexpr Color COLOR = Color(COLOR_ENUM);

        const Square kingSq = kingSquare(COLOR);
        const Bitboard occ = occupied();

        const Bitboard pawns = pieces(PieceType::PAWN, ~COLOR);
        const Bitboard knights = pieces(PieceType::KNIGHT, ~COLOR);
        const Bitboard bishops = pieces(PieceType::BISHOP, ~COLOR);
        const Bitboard rooks = pieces(PieceType::ROOK, ~COLOR);
        const Bitboard queens = pieces(PieceType::QUEEN, ~COLOR);

        Bitboard mask = Bitboard();
        UInt8 checks = 0;

        Bitboard pawnMask = Attacks::pawn(kingSq, COLOR) & pawns;
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

        return {mask, checks};
    }

    template<Color::ColorEnum COLOR_ENUM, PieceType::PieceTypeEnum PIECE_TYPE_ENUM>
    Bitboard pinMask() const noexcept {
        static_assert(COLOR_ENUM != Color::NONE);
        static_assert(PIECE_TYPE_ENUM == PieceType::BISHOP || PIECE_TYPE_ENUM == PieceType::ROOK);
        constexpr Color COLOR = Color(COLOR_ENUM);

        const Square kingSq = kingSquare(COLOR);
        const Bitboard friendlyOcc = friendly(COLOR);
        const Bitboard enemyOcc = enemy(COLOR);

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

    template<Color::ColorEnum COLOR_ENUM>
    Bitboard blockMask() const noexcept {
        static_assert(COLOR_ENUM != Color::NONE);
        constexpr Color COLOR = Color(COLOR_ENUM);

        const Square enemyKingSq = kingSquare(~COLOR);
        const Bitboard friendlyOcc = friendly(COLOR);
        const Bitboard enemyOcc = enemy(COLOR);

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
        return mask;
    }

    template<Color::ColorEnum COLOR_ENUM>
    Bitboard attackMask() const noexcept {
        static_assert(COLOR_ENUM != Color::NONE);
        constexpr Color COLOR = Color(COLOR_ENUM);

        const Square kingSq = kingSquare(COLOR);
        const Square enemyKingSq = kingSquare(~COLOR);
        const Bitboard occ = occupied() ^ Bitboard(enemyKingSq);

        const Bitboard pawns = pieces(PieceType::PAWN, COLOR);
        Bitboard knights = pieces(PieceType::KNIGHT, COLOR);
        Bitboard queens = pieces(PieceType::QUEEN, COLOR);
        Bitboard bishops = pieces(PieceType::BISHOP, COLOR) | queens;
        Bitboard rooks = pieces(PieceType::ROOK, COLOR) | queens;

        Bitboard mask = Bitboard();

        mask |= Attacks::allPawns<COLOR_ENUM>(pawns);
        while (knights) {
            mask |= Attacks::knight(knights.pop());
        }
        while (bishops) {
            mask |= Attacks::bishop(bishops.pop(), occ);
        }
        while (rooks) {
            mask |= Attacks::rook(rooks.pop(), occ);
        }
        mask |= Attacks::king(kingSq);

        return mask;
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

    constexpr bool inCheck() const noexcept { return attacked(kingSquare(sideToMove_), ~sideToMove_); }

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
            const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
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

    constexpr bool pseudoLegal(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);

        if (move.from() == move.to()) {
            return false;
        }

        const Piece piece = pieceAt(move.from());
        if (piece == Piece::NONE || piece.color() != sideToMove_) {
            return false;
        }

        const Piece capturedPiece = pieceAt(move.to());

        Bitboard attackedSquares = Bitboard();
        if (sideToMove_ == Color::WHITE) {
            attackedSquares = attackMask<Color::BLACK>();
        } else {
            attackedSquares = attackMask<Color::WHITE>();
        }

        if (move.type() == MoveType::CASTLING) {
            if (piece.type() != PieceType::KING || inCheck()) {
                return false;
            }

            if (move.from() != Square(Square::SQUARE_E1, sideToMove_)) {
                return false;
            }

            const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), move.from(), sideToMove_);
            if (move.to() != CastlingRights::rookFrom(castlingSide) || capturedPiece != Piece(PieceType::ROOK, sideToMove_)) {
                return false;
            }

            if (!castlingRights_.get(castlingSide)) {
                return false;
            }

            return (castlingPath(castlingSide) & (occupied() | attackedSquares)) == Bitboard();
        }

        if (piece.type() != PieceType::PAWN && move.type() != MoveType::NORMAL) {
            return false;
        }

        if ((capturedPiece != Piece::NONE && capturedPiece.color() == sideToMove_) || capturedPiece.type() == PieceType::KING) {
            return false;
        }

        Bitboard checkSquares;
        UInt8 checks;
        if (sideToMove_ == Color::WHITE) {
            std::tie(checkSquares, checks) = checkMask<Color::WHITE>();
        } else {
            std::tie(checkSquares, checks) = checkMask<Color::BLACK>();
        }

        if (checks >= 2 && piece.type() != PieceType::KING) {
            return false;
        }

        if (piece.type() != PieceType::KING && move.type() != MoveType::EN_PASSANT && !(checkSquares & Bitboard(move.to()))) {
            return false;
        }

        if (piece.type() == PieceType::PAWN) {
            if (move.type() == MoveType::EN_PASSANT) {
                if (move.to() != enPassantSquare_) {
                    return false;
                }

                if (!(Attacks::pawn(move.from(), sideToMove_) & Bitboard(move.to()))) {
                    return false;
                }
                const Direction down = Direction(Direction::SOUTH, sideToMove_);
                const Square target = enPassantSquare_ + down;

                return (checks == 0) || (Bitboard(target) & checkSquares);
            }

            const Direction up = Direction(Direction::NORTH, sideToMove_);
            if (capturedPiece == Piece::NONE) {
                if (move.to() == move.from() + up) {
                    if (move.type() == MoveType::PROMOTION) {
                        return Rank(Rank::RANK_7, sideToMove_) == move.from().rank();
                    } else {
                        return move.from().rank() < Rank(Rank::RANK_7, sideToMove_) && move.from().rank() > Rank(Rank::RANK_1, sideToMove_);
                    }
                } else if (move.to() == (move.from() + up) + up) {
                    if (pieceAt(move.from() + up) != Piece::NONE) {
                        return false;
                    }
                    return move.from().rank() == Rank(Rank::RANK_2, sideToMove_);
                } else {
                    return false;
                }
            } else {
                if (!(Attacks::pawn(move.from(), sideToMove_) & Bitboard(move.to()))) {
                    return false;
                }

                if (move.type() == MoveType::PROMOTION) {
                    return Rank(Rank::RANK_7, sideToMove_) == move.from().rank();
                } else {
                    return move.from().rank() < Rank(Rank::RANK_7, sideToMove_) && move.from().rank() > Rank(Rank::RANK_1, sideToMove_);
                }
            }
        }

        Bitboard attacks;
        if (piece.type() == PieceType::KNIGHT) {
            attacks = Attacks::knight(move.from());
        } else if (piece.type() == PieceType::BISHOP) {
            attacks = Attacks::bishop(move.from(), occupied());
        } else if (piece.type() == PieceType::ROOK) {
            attacks = Attacks::rook(move.from(), occupied());
        } else if (piece.type() == PieceType::QUEEN) {
            attacks = Attacks::queen(move.from(), occupied());
        } else if (piece.type() == PieceType::KING) {
            attacks = Attacks::king(move.from()) & ~attackedSquares;
        } else {
            assert(false);
            attacks = Bitboard();
        }

        return bool(attacks & Bitboard(move.to()));
    }

    constexpr bool nonPawnMaterial(Color color) const noexcept {
        assert(color != Color::NONE);
        return bool(friendly(color) ^ (pieces(PieceType::PAWN, color) | pieces(PieceType::KING, color)));
    }

    bool repetition3Fold(Int32 searchPly) const noexcept { return repetitions_ > 1 || (repetitions_ == 1 && repetitionPly_ < searchPly); }

    bool upcomingRepetition(Int32 searchPly) const noexcept {
        if (depth_ < 3) {
            return false;
        }

        UInt64 originalHash = hash_;
        UInt64 diff = originalHash ^ stateHistory_[depth_ - 1].hash ^ Zobrist::sideToMove();

        USize reversible = static_cast<USize>(std::min(halfmoveClock_, pliesFromNull_));
        for (USize i = 2; i <= reversible && depth_ >= i + 1; i += 2) {
            const BoardState &state = stateHistory_[depth_ - i - 1];
            diff ^= stateHistory_[depth_ - i].hash ^ state.hash ^ Zobrist::sideToMove();
            if (diff != 0) {
                continue;
            }

            UInt64 moveHash = originalHash ^ state.hash;
            const Move move = CuckooTable::probe(moveHash);
            if (move == Move::NULL_MOVE) {
                continue;
            }

            if (!((Attacks::between(move.from(), move.to()) ^ Bitboard(move.to())) & occupied())) {
                if (searchPly > static_cast<Int32>(i)) {
                    return true;
                }

                if (state.repetitions > 0) {
                    return true;
                }

            }

        }
        return false;
    }

    constexpr bool halfMoveDraw(bool noMoves) const noexcept { return halfmoveClock_ >= 100 && !(inCheck() && noMoves); }

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

    constexpr bool draw(Int32 searchPly, bool noMoves) const noexcept {
        return halfMoveDraw(noMoves) || insufficientMaterial() || repetition3Fold(searchPly);
    }

private:
    struct BoardState {
        UInt64 hash;
        CastlingRights castlingRights;
        Square enPassantSquare;
        UInt16 halfmoveClock;
        UInt16 pliesFromNull;
        UInt16 repetitionPly;
        UInt8 repetitions;
        Piece capturedPiece;
    };

    std::array<BoardState, MAX_POSITION_DEPTH> stateHistory_;
    USize depth_;

    UInt64 hash_;
    CastlingRights castlingRights_;
    Square enPassantSquare_;
    Color sideToMove_;
    UInt16 halfmoveClock_;
    UInt16 pliesFromNull_;
    UInt16 repetitionPly_;
    UInt8 repetitions_;
    UInt16 plies_;

    std::array<Bitboard, 6> pieceBitboards_;
    std::array<Bitboard, 2> occupancyBitboards_;
    std::array<Piece, 64> board_;
    std::array<Bitboard, 4> castlingPathBitboards_;
};

}
