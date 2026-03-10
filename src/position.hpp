#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "attacks.hpp"
#include "bitboard.hpp"
#include "color.hpp"
#include "coordinates.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "zobrist.hpp"


namespace Clownfish {

enum class GameResult {
    WIN,
    LOSS,
    DRAW,
    NONE
};

enum class GameResultReason {
    CHECKMATE,
    STALEMATE,
    INSUFFICIENT_MATERIAL,
    THREEFOLD_REPETITION,
    FIFTY_MOVE_RULE,
    NONE
};

enum class CheckType {
    DIRECT,
    DISCOVERED,
    NONE
};

class Position {
public:
    class CastlingRights {
    public:
        enum class CastlingSide : std::uint8_t {
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
        constexpr CastlingRights(CastlingSide side) noexcept : rights_(static_cast<std::uint8_t>(side)) {}
        constexpr CastlingRights(std::uint8_t rights) noexcept : rights_(rights) { assert(rights >= 0 && rights < 16); }

        constexpr bool operator==(const CastlingRights &other) const noexcept { return rights_ == other.rights_; }
        constexpr operator int() const noexcept { return static_cast<int>(rights_); }

        constexpr void set(CastlingSide side) noexcept { rights_ |= static_cast<std::uint8_t>(side); }
        constexpr bool get(CastlingSide side) const noexcept { return (rights_ & static_cast<std::uint8_t>(side)) != 0; }

        constexpr bool get(Color color) const noexcept {
            assert(color != Color::NONE);
            if (color == Color::WHITE) {
                return get(CastlingSide::WHITE_KINGSIDE) || get(CastlingSide::WHITE_QUEENSIDE);
            } else {
                return get(CastlingSide::BLACK_KINGSIDE) || get(CastlingSide::BLACK_QUEENSIDE);
            }
        }

        constexpr void clear() noexcept { rights_ = 0; }

        constexpr void clear(CastlingSide side) noexcept { rights_ &= ~static_cast<std::uint8_t>(side); }

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

        constexpr int hash() const noexcept { return static_cast<int>(rights_); }
        static constexpr int hashIndex(CastlingSide side) noexcept { return std::countr_zero(static_cast<std::uint8_t>(side)); }

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
            std::uint8_t shift = 0;
            if (color == Color::BLACK) {
                shift = 2;
            }
            if (square < kingSquare) {
                shift += 1;
            }
            return static_cast<CastlingSide>(1 << shift);
        }

        static constexpr File rookFile(CastlingSide side) noexcept {
            if (side == CastlingSide::WHITE_KINGSIDE || side == CastlingSide::BLACK_KINGSIDE) {
                return File::FILE_H;
            } else if (side == CastlingSide::WHITE_QUEENSIDE || side == CastlingSide::BLACK_QUEENSIDE) {
                return File::FILE_A;
            } else {
                assert(false);
                return File::NONE;
            }
        }

        constexpr std::uint8_t internal() const noexcept { return rights_; }

    private:
        std::uint8_t rights_;
    };

    explicit Position(std::string_view fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
        stateHistory_.reserve(256);
        assert(set(fen));
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
        plies_ = 0;
        stateHistory_.clear();

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

        const std::vector<std::string_view> parts = splitStringView(fen, ' ');
        const std::string_view board = parts.size() > 0 ? parts[0] : "";
        const std::string_view side = parts.size() > 1 ? parts[1] : "w";
        const std::string_view castling = parts.size() > 2 ? parts[2] : "-";
        const std::string_view enPassant = parts.size() > 3 ? parts[3] : "-";
        const std::string_view halfmoves = parts.size() > 4 ? parts[4] : "0";
        const std::string_view fullmoves = parts.size() > 5 ? parts[5] : "1";

        if (board.empty()) {
            return false;
        }

        int index = 56;
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
                if (piece == Piece::NONE || pieceAt(Square(index)) != Piece::NONE) {
                    return false;
                }
                placePiece(piece, Square(index));
                hash_ ^= Zobrist::piece(piece, Square(index));
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
            if (enPassantSquare_ != Square::NONE && !((sideToMove_ == Color::WHITE && enPassantSquare_.rank() == Rank::RANK_6) ||
                (sideToMove_ == Color::BLACK && enPassantSquare_.rank() == Rank::RANK_3))) {
                return false;
            }
            hash_ ^= Zobrist::enPassant(enPassantSquare_.file());
        }

        try {
            halfmoveClock_ = static_cast<std::uint8_t>(std::stoi(std::string(halfmoves)));
            if (halfmoveClock_ < 0) {
                return false;
            }
        } catch (const std::invalid_argument &) {
            return false;
        }

        try {
            plies_ = static_cast<std::uint16_t>((std::stoi(std::string(fullmoves)) - 1) * 2 + (sideToMove_ == Color::BLACK ? 1 : 0));
            if (plies_ < 0) {
                return false;
            }
        } catch (const std::invalid_argument &) {
            return false;
        }

        assert(hash_ == zobrist());

        return true;
    }

    std::string fen() const {
        std::string str;
        str.reserve(100);

        for (int rank = 7; rank >= 0; rank--) {
            int emptyCount = 0;
            for (int file = 0; file < 8; file++) {
                const Piece piece = pieceAt(Square(file, rank));
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

    constexpr bool operator==(const Position &other) const noexcept {
        return hash_ == other.hash_ &&
            castlingRights_ == other.castlingRights_ &&
            enPassantSquare_ == other.enPassantSquare_ &&
            sideToMove_ == other.sideToMove_ &&
            halfmoveClock_ == other.halfmoveClock_ &&
            plies_ == other.plies_ &&
            pieceBitboards_ == other.pieceBitboards_ &&
            occupancyBitboards_ == other.occupancyBitboards_ &&
            board_ == other.board_ &&
            castlingPathBitboards_ == other.castlingPathBitboards_;
    }

    std::uint64_t hash() const noexcept { return hash_; }
    CastlingRights castlingRights() const noexcept { return castlingRights_; }
    Square enPassantSquare() const noexcept { return enPassantSquare_; }
    Color sideToMove() const noexcept { return sideToMove_; }
    std::uint8_t halfmoveClock() const noexcept { return halfmoveClock_; }
    std::uint32_t fullMoveNumber() const noexcept { return plies_ / 2 + 1; }

    constexpr Bitboard castlingPath(CastlingRights::CastlingSide castlingSide) const noexcept {
        return castlingPathBitboards_[static_cast<std::size_t>(CastlingRights::hashIndex(castlingSide))];
    }

    void placePiece(Piece piece, Square square) noexcept {
        assert(piece != Piece::NONE && square != Square::NONE);
        assert(pieceAt(square) == Piece::NONE);
        const PieceType pieceType = piece.type();
        const Color color = piece.color();
        const int index = square.index();
        pieceBitboards_[static_cast<std::size_t>(pieceType)].set(index);
        occupancyBitboards_[static_cast<std::size_t>(color)].set(index);
        board_[static_cast<std::size_t>(index)] = piece;
    }

    void removePiece(Piece piece, Square square) noexcept {
        assert(piece != Piece::NONE && square != Square::NONE);
        assert(pieceAt(square) == piece);
        const PieceType pieceType = piece.type();
        const Color color = piece.color();
        const int index = square.index();
        pieceBitboards_[static_cast<std::size_t>(pieceType)].clear(index);
        occupancyBitboards_[static_cast<std::size_t>(color)].clear(index);
        board_[static_cast<std::size_t>(index)] = Piece::NONE;
    }

    void make(Move move) {
        assert(move != Move::NULL_MOVE);
        assert(pieceAt(move.from()).color() == sideToMove_);

        const bool capture = isCapture(move);
        const Piece capturedPiece = pieceAt(move.to());
        const PieceType pieceType = pieceAt(move.from()).type();

        stateHistory_.emplace_back(State{hash_, castlingRights_, enPassantSquare_, halfmoveClock_, capturedPiece});

        halfmoveClock_++;
        plies_++;

        if (enPassantSquare_ != Square::NONE) {
            hash_ ^= Zobrist::enPassant(enPassantSquare_.file());
            enPassantSquare_ = Square::NONE;
        }

        if (capture) {
            halfmoveClock_ = 0;

            if (move.type() != Move::EN_PASSANT) {
                removePiece(capturedPiece, move.to());
                hash_ ^= Zobrist::piece(capturedPiece, move.to());

                if (capturedPiece.type() == PieceType::ROOK && move.to().rank().backRank(~sideToMove_)) {
                    const Square kingSq = kingSquare(~sideToMove_);
                    const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), kingSq, ~sideToMove_);
                    if (castlingRights_.get(castlingSide) && CastlingRights::rookFile(castlingSide) == move.to().file()) {
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
            if (castlingRights_.get(castlingSide) && CastlingRights::rookFile(castlingSide) == move.from().file()) {
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

        if (move.type() == Move::CASTLING) {
            assert(pieceAt(move.from()).type() == PieceType::KING);
            assert(pieceAt(move.to()).type() == PieceType::ROOK);

            const bool kingside = move.to() > move.from();
            const Square rookTo = Square::rookCastlingSquare(sideToMove_, kingside);
            const Square kingTo = Square::kingCastlingSquare(sideToMove_, kingside);
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
        } else if (move.type() == Move::PROMOTION) {
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

        if (move.type() == Move::EN_PASSANT) {
            assert(pieceAt(move.to().enPassantSquare()) == PieceType::PAWN);
            Piece pawn = Piece(PieceType::PAWN, ~sideToMove_);
            removePiece(pawn, move.to().enPassantSquare());
            hash_ ^= Zobrist::piece(pawn, move.to().enPassantSquare());
        }

        sideToMove_ = ~sideToMove_;
        hash_ ^= Zobrist::sideToMove();
    }

    void unmake(const Move move) noexcept {
        assert(move != Move::NULL_MOVE);

        const State &previousState = stateHistory_.back();


        hash_ = previousState.hash;
        castlingRights_ = previousState.castlingRights;
        enPassantSquare_ = previousState.enPassantSquare;
        halfmoveClock_ = previousState.halfmoveClock;
        plies_--;
        sideToMove_ = ~sideToMove_;

        if (move.type() == Move::CASTLING) {
            const bool kingside = move.to() > move.from();
            const Square rookTo = Square::rookCastlingSquare(sideToMove_, kingside);
            const Square kingTo = Square::kingCastlingSquare(sideToMove_, kingside);
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
        } else if (move.type() == Move::PROMOTION) {
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

            if (move.type() == Move::EN_PASSANT) {
                Piece pawn = Piece(PieceType::PAWN, ~sideToMove_);
                Square pawnSquare = Square(move.to().file(), move.from().rank());

                assert(pieceAt(pawnSquare) == Piece::NONE);

                placePiece(pawn, pawnSquare);
            } else if (previousState.capturedPiece != Piece::NONE) {
                assert(pieceAt(move.to()) == Piece::NONE);
                placePiece(previousState.capturedPiece, move.to());
            }
        }

        stateHistory_.pop_back();
    }

    void makeNull() {
        stateHistory_.emplace_back(State{hash_, castlingRights_, enPassantSquare_, halfmoveClock_, Piece::NONE});

        if (enPassantSquare_ != Square::NONE) {
            hash_ ^= Zobrist::enPassant(enPassantSquare_.file());
            enPassantSquare_ = Square::NONE;
        }

        sideToMove_ = ~sideToMove_;
        hash_ ^= Zobrist::sideToMove();

        plies_++;
    }

    void unmakeNull() noexcept {
        const State &previousState = stateHistory_.back();

        hash_ = previousState.hash;
        castlingRights_ = previousState.castlingRights;
        enPassantSquare_ = previousState.enPassantSquare;
        halfmoveClock_ = previousState.halfmoveClock;
        plies_--;
        sideToMove_ = ~sideToMove_;

        stateHistory_.pop_back();
    }

    constexpr std::uint64_t zobrist() const noexcept {
        std::uint64_t key = 0ULL;

        Bitboard pieces = occupied();
        while (pieces) {
            const Square square = static_cast<int>(pieces.pop());
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

    std::uint64_t zobristAfter(const Move move) const noexcept {
        std::uint64_t key = hash_;

        key ^= Zobrist::sideToMove();

        if (enPassantSquare_ != Square::NONE) {
            key ^= Zobrist::enPassant(enPassantSquare_.file());
        }

        if (move == Move::NULL_MOVE) {
            return key;
        }

        const bool capture = (pieceAt(move.to()) != Piece::NONE) && (move.type() != Move::CASTLING);
        const Piece captured = pieceAt(move.to());
        const PieceType pieceType = pieceAt(move.from()).type();

        if (capture) {
            key ^= Zobrist::piece(captured, move.to());

            if (captured.type() == PieceType::ROOK && move.to().rank().backRank(~sideToMove_)) {
                const Square kingSq = kingSquare(~sideToMove_);
                const CastlingRights::CastlingSide castlingSide = CastlingRights::closestSide(move.to(), kingSq, ~sideToMove_);
                if (castlingRights_.get(castlingSide) && CastlingRights::rookFile(castlingSide) == move.to().file()) {
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
            if (castlingRights_.get(castlingSide) && CastlingRights::rookFile(castlingSide) == move.from().file()) {
                key ^= Zobrist::castlingIndex(CastlingRights::hashIndex(castlingSide));
            }
        } else if (pieceType == PieceType::PAWN && Square::indexDistance(move.from(), move.to()) == 16) {
            Bitboard enPassantMask = Attacks::pawn(move.to().enPassantSquare(), sideToMove_);
            if (enPassantMask & pieces(PieceType::PAWN, ~sideToMove_)) {
                assert(pieceAt(move.to().enPassantSquare()) == Piece::NONE);
                key ^= Zobrist::enPassant(move.to().enPassantSquare().file());
            }
        }

        if (move.type() == Move::CASTLING) {
            assert(pieceAt(move.from()).type() == PieceType::KING);
            assert(pieceAt(move.to()).type() == PieceType::ROOK);

            const bool kingside = move.to() > move.from();
            const Square rookTo = Square::rookCastlingSquare(sideToMove_, kingside);
            const Square kingTo = Square::kingCastlingSquare(sideToMove_, kingside);
            const Piece king = pieceAt(move.from());
            const Piece rook = pieceAt(move.to());
            assert(king == Piece(PieceType::KING, sideToMove_) && rook == Piece(PieceType::ROOK, sideToMove_));

            key ^= Zobrist::piece(king, move.from());
            key ^= Zobrist::piece(king, kingTo);
            key ^= Zobrist::piece(rook, move.to());
            key ^= Zobrist::piece(rook, rookTo);
        } else if (move.type() == Move::PROMOTION) {
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

        if (move.type() == Move::EN_PASSANT) {
            assert(pieceAt(move.to().enPassantSquare()) == PieceType::PAWN);
            Piece pawn = Piece(PieceType::PAWN, ~sideToMove_);
            key ^= Zobrist::piece(pawn, move.to().enPassantSquare());
        }

        return key;
    }

    constexpr Bitboard friendly(Color color) const noexcept {
        assert(color != Color::NONE);
        return occupancyBitboards_[static_cast<std::size_t>(color)];
    }

    constexpr Bitboard enemy(Color color) const noexcept {
        assert(color != Color::NONE);
        return friendly(~color);
    }

    constexpr Bitboard occupied() const noexcept { return occupancyBitboards_[0] | occupancyBitboards_[1]; }

    constexpr Piece pieceAt(Square square) const noexcept {
        assert(square != Square::NONE);
        return board_[static_cast<std::size_t>(square.index())];
    }

    constexpr Bitboard pieces(PieceType pieceType) const noexcept {
        assert(pieceType != PieceType::NONE);
        return pieceBitboards_[static_cast<std::size_t>(pieceType)];
    }

    constexpr Bitboard pieces(PieceType pieceType, Color color) const noexcept {
        assert(pieceType != PieceType::NONE && color != Color::NONE);
        return pieceBitboards_[static_cast<std::size_t>(pieceType)] & occupancyBitboards_[static_cast<std::size_t>(color)];
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

    constexpr bool check() const noexcept { return attacked(kingSquare(sideToMove_), ~sideToMove_); }

    constexpr bool isCapture(const Move move) const noexcept {
        assert(move != Move::NULL_MOVE);
        return (pieceAt(move.to()) != Piece::NONE && move.type() != Move::CASTLING) || move.type() == Move::EN_PASSANT;
    }

    constexpr CheckType isCheck(const Move move) const noexcept {
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

        Bitboard threats = 0ULL;
        if (pieceType == PieceType::PAWN) {
            threats = Attacks::pawn(kingSq, ~sideToMove_);
        } else if (pieceType == PieceType::KNIGHT) {
            threats = Attacks::knight(kingSq);
        } else if (pieceType == PieceType::BISHOP) {
            threats = Attacks::bishop(kingSq, occupied());
        } else if (pieceType == PieceType::ROOK) {
            threats = Attacks::rook(kingSq, occupied());
        } else if (pieceType == PieceType::QUEEN) {
            threats = Attacks::queen(kingSq, occupied());
        }

        if (threats & toBitboard) {
            return CheckType::DIRECT;
        }

        const Bitboard fromBitboard = Bitboard(from);
        const Bitboard occ = occupied() ^ fromBitboard;

        Bitboard sniper = findSniper(kingSq, occ);
        if (sniper) {
            if (!(Attacks::between(kingSq, sniper.lsb()) & toBitboard) || move.type() == Move::CASTLING) {
                return CheckType::DISCOVERED;
            } else {
                return CheckType::NONE;
            }
        }

        if (move.type() == Move::NORMAL) {
            return CheckType::NONE;
        } else if (move.type() == Move::PROMOTION) {
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
                return CheckType::DIRECT;
            } else {
                return CheckType::NONE;
            }
        } else if (move.type() == Move::EN_PASSANT) {
            Square captureSq = Square(to.file(), from.rank());
            if (findSniper(kingSq, (occ ^ Bitboard(captureSq)) | toBitboard)) {
                return CheckType::DISCOVERED;
            } else {
                return CheckType::NONE;
            }
        } else if (move.type() == Move::CASTLING) {
            Square rookTo = Square::rookCastlingSquare(sideToMove_, to > from);
            if (Attacks::rook(kingSq, occ) & Bitboard(rookTo)) {
                return CheckType::DISCOVERED;
            } else {
                return CheckType::NONE;
            }
        } else {
            assert(false);
            return CheckType::NONE;
        }
    }

    constexpr bool nonPawnMaterial(Color color) const noexcept {
        assert(color != Color::NONE);
        return static_cast<bool>(friendly(color) ^ (pieces(PieceType::PAWN, color) | pieces(PieceType::KING, color)));
    }

    bool repetition(int count = 1) const noexcept {
        if (stateHistory_.size() < 2) {
            return false;
        }

        std::uint8_t seen = 0;
        const int size = static_cast<int>(stateHistory_.size());
        for (int i = size - 2; i >= 0 && i >= size - halfmoveClock_ - 1; i -= 2) {
            if (stateHistory_[static_cast<std::size_t>(i)].hash == hash_) {
                seen++;
            }
            if (seen == count) {
                return true;
            }
        }
        return false;
    }

    constexpr bool halfMoveDraw() const noexcept { return halfmoveClock_ >= 100; }

    std::pair<GameResultReason, GameResult> halfMoveDrawResult(const MoveList &moveList) const noexcept {
        if (moveList.empty() && check()) {
            return {GameResultReason::CHECKMATE, GameResult::LOSS};
        }
        return {GameResultReason::FIFTY_MOVE_RULE, GameResult::DRAW};
    }

    constexpr bool insufficientMaterial() const noexcept {
        const int count = occupied().count();

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

    constexpr std::pair<GameResultReason, GameResult> gameOver(const MoveList &moveList) const noexcept {
        if (halfMoveDraw()) {
            return halfMoveDrawResult(moveList);
        }
        if (insufficientMaterial()) {
            return {GameResultReason::INSUFFICIENT_MATERIAL, GameResult::DRAW};
        }
        if (repetition()) {
            return {GameResultReason::THREEFOLD_REPETITION, GameResult::DRAW};
        }

        if (moveList.empty()) {
            if (check()) {
                return {GameResultReason::CHECKMATE, GameResult::LOSS};
            }
            return {GameResultReason::STALEMATE, GameResult::DRAW};
        }

        return {GameResultReason::NONE, GameResult::NONE};
    }

private:
    struct State {
        std::uint64_t hash;
        CastlingRights castlingRights;
        Square enPassantSquare;
        std::uint8_t halfmoveClock;
        Piece capturedPiece;
    };

    std::vector<State> stateHistory_;
    std::array<Bitboard, 6> pieceBitboards_;
    std::array<Bitboard, 2> occupancyBitboards_;
    std::array<Piece, 64> board_;

    std::uint64_t hash_;
    CastlingRights castlingRights_;
    Square enPassantSquare_;
    Color sideToMove_;
    std::uint8_t halfmoveClock_;
    std::uint16_t plies_;

    std::array<Bitboard, 4> castlingPathBitboards_;

    static std::vector<std::string_view> splitStringView(std::string_view string, char delimiter = ' ') {
        std::vector<std::string_view> result;
        std::size_t start = 0;
        while (start < string.size()) {
            std::size_t end = string.find(delimiter, start);
            if (end == std::string_view::npos) {
                end = string.size();
            }
            result.push_back(string.substr(start, end - start));
            start = end + 1;
        }
        return result;
    }
};

}
