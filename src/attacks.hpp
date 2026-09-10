#pragma once

#if defined(USE_PEXT)
#include <immintrin.h>
#endif

#include <array>
#include <functional>
#include <mutex>
#include <tuple>

#include "bitboard.hpp"
#include "color.hpp"
#include "coords.hpp"
#include "piece.hpp"
#include "types.hpp"


namespace Sift {

class Attacks {
public:
    static constexpr Bitboard RANK_MASKS[8] = {
        0xFF,         0xFF00,         0xFF0000,         0xFF000000,
        0xFF00000000, 0xFF0000000000, 0xFF000000000000, 0xFF00000000000000
    };

    static constexpr Bitboard FILE_MASKS[8] = {
        0x101010101010101,  0x202020202020202,  0x404040404040404,  0x808080808080808,
        0x1010101010101010, 0x2020202020202020, 0x4040404040404040, 0x8080808080808080,
    };

    template<Direction::DirectionEnum DIRECTION_ENUM>
    static constexpr Bitboard shift(const Bitboard bitboard) noexcept {
        static_assert(DIRECTION_ENUM != Direction::NONE);
        if constexpr (DIRECTION_ENUM == Direction::NORTH) {
            return bitboard << 8;
        } else if constexpr (DIRECTION_ENUM == Direction::EAST) {
            return (bitboard & ~FILE_MASKS[7]) << 1;
        } else if constexpr (DIRECTION_ENUM == Direction::SOUTH) {
            return bitboard >> 8;
        } else if constexpr (DIRECTION_ENUM == Direction::WEST) {
            return (bitboard & ~FILE_MASKS[0]) >> 1;
        } else if constexpr (DIRECTION_ENUM == Direction::NORTH_EAST) {
            return (bitboard & ~FILE_MASKS[7]) << 9;
        } else if constexpr (DIRECTION_ENUM == Direction::SOUTH_EAST) {
            return (bitboard & ~FILE_MASKS[7]) >> 7;
        } else if constexpr (DIRECTION_ENUM == Direction::SOUTH_WEST) {
            return (bitboard & ~FILE_MASKS[0]) >> 9;
        } else if constexpr (DIRECTION_ENUM == Direction::NORTH_WEST) {
            return (bitboard & ~FILE_MASKS[0]) << 7;
        } else {
            static_assert(false);
            return Bitboard();
        }
    }

    static void init() {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, []() {
            BISHOP_TABLE[0].attacks = BISHOP_ATTACKS;
            ROOK_TABLE[0].attacks = ROOK_ATTACKS;
            for (UInt8 i = 0; i < 64; i++) {
                initSliders(Square(i), BISHOP_TABLE, BISHOP_MAGICS[i], sliderSlow<PieceType::BISHOP>);
                initSliders(Square(i), ROOK_TABLE, ROOK_MAGICS[i], sliderSlow<PieceType::ROOK>);
            }

            std::tie(SQUARES_BETWEEN_BITBOARDS, SQUARES_ALIGNED_BITBOARDS) = initSquarePaths();
        });
    }

    template<Color::ColorEnum COLOR_ENUM>
    static constexpr Bitboard allPawns(const Bitboard pawns) noexcept {
        static_assert(COLOR_ENUM != Color::NONE);
        if constexpr (COLOR_ENUM == Color::WHITE) {
            return shift<Direction::NORTH_WEST>(pawns) | shift<Direction::NORTH_EAST>(pawns);
        } else if constexpr (COLOR_ENUM == Color::BLACK) {
            return shift<Direction::SOUTH_EAST>(pawns) | shift<Direction::SOUTH_WEST>(pawns);
        } else {
            static_assert(false);
            return Bitboard();
        }
    }

    static constexpr Bitboard pawn(Square square, Color color) noexcept {
        assert(color != Color::NONE);
        assert(square != Square::NONE);
        return PAWN_ATTACKS[color][square.index()];
    }

    static constexpr Bitboard knight(Square square) noexcept {
        assert(square != Square::NONE);
        return KNIGHT_ATTACKS[square.index()];
    }

    static Bitboard bishop(Square square, Bitboard occupied) noexcept {
        assert(square != Square::NONE);
        return BISHOP_TABLE[square.index()].attacks[BISHOP_TABLE[square.index()].hashIndex(occupied)];
    }

    static Bitboard rook(Square square, Bitboard occupied) noexcept {
        assert(square != Square::NONE);
        return ROOK_TABLE[square.index()].attacks[ROOK_TABLE[square.index()].hashIndex(occupied)];
    }

    static Bitboard queen(Square square, Bitboard occupied) noexcept {
        assert(square != Square::NONE);
        return bishop(square, occupied) | rook(square, occupied);
    }

    static constexpr Bitboard king(Square square) noexcept {
        assert(square != Square::NONE);
        return KING_ATTACKS[square.index()];
    }

    static constexpr Bitboard attacks(Piece piece, Square square, Bitboard occupied) noexcept {
        assert(piece != Piece::NONE);
        assert(square != Square::NONE);
        if (piece.type() == PieceType::PAWN) {
            return Attacks::pawn(square, piece.color());
        } else if (piece.type() == PieceType::KNIGHT) {
            return Attacks::knight(square);
        } else if (piece.type() == PieceType::BISHOP) {
            return Attacks::bishop(square, occupied);
        } else if (piece.type() == PieceType::ROOK) {
            return Attacks::rook(square, occupied);
        } else if (piece.type() == PieceType::QUEEN) {
            return Attacks::queen(square, occupied);
        } else if (piece.type() == PieceType::KING) {
            return Attacks::king(square);
        } else {
            assert(false);
            return Bitboard();
        }
    }

    template<PieceType::PieceTypeEnum PIECE_TYPE_ENUM>
    static Bitboard slider(Square square, Bitboard occupied) noexcept {
        static_assert(PIECE_TYPE_ENUM == PieceType::BISHOP || PIECE_TYPE_ENUM == PieceType::ROOK || PIECE_TYPE_ENUM == PieceType::QUEEN);
        assert(square != Square::NONE);
        if constexpr (PIECE_TYPE_ENUM == PieceType::BISHOP) {
            return bishop(square, occupied);
        } else if constexpr (PIECE_TYPE_ENUM == PieceType::ROOK) {
            return rook(square, occupied);
        } else if constexpr (PIECE_TYPE_ENUM == PieceType::QUEEN) {
            return queen(square, occupied);
        } else {
            static_assert(false);
            return Bitboard();
        }
    }

    static Bitboard between(Square from, Square to) noexcept {
        assert(from != Square::NONE && to != Square::NONE);
        return SQUARES_BETWEEN_BITBOARDS[static_cast<USize>(from.index())][static_cast<USize>(to.index())];
    }

    static Bitboard aligned(Square from, Square to) noexcept {
        assert(from != Square::NONE && to != Square::NONE);
        return SQUARES_ALIGNED_BITBOARDS[static_cast<USize>(from.index())][static_cast<USize>(to.index())];
    }

private:
#if defined(USE_PEXT)
    struct Magic {
        UInt64 mask;
        Bitboard *attacks;
        UInt64 hashIndex(Bitboard bitboard) const noexcept { return _pext_u64(bitboard.bits(), mask); }
    };
#else
    struct Magic {
        UInt64 mask;
        UInt64 magic;
        UInt64 shift;
        Bitboard *attacks;
        UInt64 hashIndex(Bitboard bitboard) const noexcept { return (bitboard & mask).bits() * magic >> shift; }
    };
#endif

    static constexpr Bitboard PAWN_ATTACKS[2][64] = {
        {0x200, 0x500, 0xa00, 0x1400,
        0x2800, 0x5000, 0xa000, 0x4000,
        0x20000, 0x50000, 0xa0000, 0x140000,
        0x280000, 0x500000, 0xa00000, 0x400000,
        0x2000000, 0x5000000, 0xa000000, 0x14000000,
        0x28000000, 0x50000000, 0xa0000000, 0x40000000,
        0x200000000, 0x500000000, 0xa00000000, 0x1400000000,
        0x2800000000, 0x5000000000, 0xa000000000, 0x4000000000,
        0x20000000000, 0x50000000000, 0xa0000000000, 0x140000000000,
        0x280000000000, 0x500000000000, 0xa00000000000, 0x400000000000,
        0x2000000000000, 0x5000000000000, 0xa000000000000, 0x14000000000000,
        0x28000000000000, 0x50000000000000, 0xa0000000000000, 0x40000000000000,
        0x200000000000000, 0x500000000000000, 0xa00000000000000, 0x1400000000000000,
        0x2800000000000000, 0x5000000000000000, 0xa000000000000000, 0x4000000000000000,
        0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0},

        {0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0,
        0x2, 0x5, 0xa, 0x14,
        0x28, 0x50, 0xa0, 0x40,
        0x200, 0x500, 0xa00, 0x1400,
        0x2800, 0x5000, 0xa000, 0x4000,
        0x20000, 0x50000, 0xa0000, 0x140000,
        0x280000, 0x500000, 0xa00000, 0x400000,
        0x2000000, 0x5000000, 0xa000000, 0x14000000,
        0x28000000, 0x50000000, 0xa0000000, 0x40000000,
        0x200000000, 0x500000000, 0xa00000000, 0x1400000000,
        0x2800000000, 0x5000000000, 0xa000000000, 0x4000000000,
        0x20000000000, 0x50000000000, 0xa0000000000, 0x140000000000,
        0x280000000000, 0x500000000000, 0xa00000000000, 0x400000000000,
        0x2000000000000, 0x5000000000000, 0xa000000000000, 0x14000000000000,
        0x28000000000000, 0x50000000000000, 0xa0000000000000, 0x40000000000000}
    };

    static constexpr Bitboard KNIGHT_ATTACKS[64] = {
        0x0000000000020400, 0x0000000000050800, 0x00000000000A1100, 0x0000000000142200, 0x0000000000284400,
        0x0000000000508800, 0x0000000000A01000, 0x0000000000402000, 0x0000000002040004, 0x0000000005080008,
        0x000000000A110011, 0x0000000014220022, 0x0000000028440044, 0x0000000050880088, 0x00000000A0100010,
        0x0000000040200020, 0x0000000204000402, 0x0000000508000805, 0x0000000A1100110A, 0x0000001422002214,
        0x0000002844004428, 0x0000005088008850, 0x000000A0100010A0, 0x0000004020002040, 0x0000020400040200,
        0x0000050800080500, 0x00000A1100110A00, 0x0000142200221400, 0x0000284400442800, 0x0000508800885000,
        0x0000A0100010A000, 0x0000402000204000, 0x0002040004020000, 0x0005080008050000, 0x000A1100110A0000,
        0x0014220022140000, 0x0028440044280000, 0x0050880088500000, 0x00A0100010A00000, 0x0040200020400000,
        0x0204000402000000, 0x0508000805000000, 0x0A1100110A000000, 0x1422002214000000, 0x2844004428000000,
        0x5088008850000000, 0xA0100010A0000000, 0x4020002040000000, 0x0400040200000000, 0x0800080500000000,
        0x1100110A00000000, 0x2200221400000000, 0x4400442800000000, 0x8800885000000000, 0x100010A000000000,
        0x2000204000000000, 0x0004020000000000, 0x0008050000000000, 0x00110A0000000000, 0x0022140000000000,
        0x0044280000000000, 0x0088500000000000, 0x0010A00000000000, 0x0020400000000000
    };

    static constexpr Bitboard KING_ATTACKS[64] = {
        0x0000000000000302, 0x0000000000000705, 0x0000000000000E0A, 0x0000000000001C14, 0x0000000000003828,
        0x0000000000007050, 0x000000000000E0A0, 0x000000000000C040, 0x0000000000030203, 0x0000000000070507,
        0x00000000000E0A0E, 0x00000000001C141C, 0x0000000000382838, 0x0000000000705070, 0x0000000000E0A0E0,
        0x0000000000C040C0, 0x0000000003020300, 0x0000000007050700, 0x000000000E0A0E00, 0x000000001C141C00,
        0x0000000038283800, 0x0000000070507000, 0x00000000E0A0E000, 0x00000000C040C000, 0x0000000302030000,
        0x0000000705070000, 0x0000000E0A0E0000, 0x0000001C141C0000, 0x0000003828380000, 0x0000007050700000,
        0x000000E0A0E00000, 0x000000C040C00000, 0x0000030203000000, 0x0000070507000000, 0x00000E0A0E000000,
        0x00001C141C000000, 0x0000382838000000, 0x0000705070000000, 0x0000E0A0E0000000, 0x0000C040C0000000,
        0x0003020300000000, 0x0007050700000000, 0x000E0A0E00000000, 0x001C141C00000000, 0x0038283800000000,
        0x0070507000000000, 0x00E0A0E000000000, 0x00C040C000000000, 0x0302030000000000, 0x0705070000000000,
        0x0E0A0E0000000000, 0x1C141C0000000000, 0x3828380000000000, 0x7050700000000000, 0xE0A0E00000000000,
        0xC040C00000000000, 0x0203000000000000, 0x0507000000000000, 0x0A0E000000000000, 0x141C000000000000,
        0x2838000000000000, 0x5070000000000000, 0xA0E0000000000000, 0x40C0000000000000
    };

    static constexpr UInt64 ROOK_MAGICS[64] = {
        0x8a80104000800020ULL, 0x140002000100040ULL,  0x2801880a0017001ULL,  0x100081001000420ULL,
        0x200020010080420ULL,  0x3001c0002010008ULL,  0x8480008002000100ULL, 0x2080088004402900ULL,
        0x800098204000ULL,     0x2024401000200040ULL, 0x100802000801000ULL,  0x120800800801000ULL,
        0x208808088000400ULL,  0x2802200800400ULL,    0x2200800100020080ULL, 0x801000060821100ULL,
        0x80044006422000ULL,   0x100808020004000ULL,  0x12108a0010204200ULL, 0x140848010000802ULL,
        0x481828014002800ULL,  0x8094004002004100ULL, 0x4010040010010802ULL, 0x20008806104ULL,
        0x100400080208000ULL,  0x2040002120081000ULL, 0x21200680100081ULL,   0x20100080080080ULL,
        0x2000a00200410ULL,    0x20080800400ULL,      0x80088400100102ULL,   0x80004600042881ULL,
        0x4040008040800020ULL, 0x440003000200801ULL,  0x4200011004500ULL,    0x188020010100100ULL,
        0x14800401802800ULL,   0x2080040080800200ULL, 0x124080204001001ULL,  0x200046502000484ULL,
        0x480400080088020ULL,  0x1000422010034000ULL, 0x30200100110040ULL,   0x100021010009ULL,
        0x2002080100110004ULL, 0x202008004008002ULL,  0x20020004010100ULL,   0x2048440040820001ULL,
        0x101002200408200ULL,  0x40802000401080ULL,   0x4008142004410100ULL, 0x2060820c0120200ULL,
        0x1001004080100ULL,    0x20c020080040080ULL,  0x2935610830022400ULL, 0x44440041009200ULL,
        0x280001040802101ULL,  0x2100190040002085ULL, 0x80c0084100102001ULL, 0x4024081001000421ULL,
        0x20030a0244872ULL,    0x12001008414402ULL,   0x2006104900a0804ULL,  0x1004081002402ULL
    };

    static constexpr UInt64 BISHOP_MAGICS[64] = {
        0x40040844404084ULL,   0x2004208a004208ULL,   0x10190041080202ULL,   0x108060845042010ULL,
        0x581104180800210ULL,  0x2112080446200010ULL, 0x1080820820060210ULL, 0x3c0808410220200ULL,
        0x4050404440404ULL,    0x21001420088ULL,      0x24d0080801082102ULL, 0x1020a0a020400ULL,
        0x40308200402ULL,      0x4011002100800ULL,    0x401484104104005ULL,  0x801010402020200ULL,
        0x400210c3880100ULL,   0x404022024108200ULL,  0x810018200204102ULL,  0x4002801a02003ULL,
        0x85040820080400ULL,   0x810102c808880400ULL, 0xe900410884800ULL,    0x8002020480840102ULL,
        0x220200865090201ULL,  0x2010100a02021202ULL, 0x152048408022401ULL,  0x20080002081110ULL,
        0x4001001021004000ULL, 0x800040400a011002ULL, 0xe4004081011002ULL,   0x1c004001012080ULL,
        0x8004200962a00220ULL, 0x8422100208500202ULL, 0x2000402200300c08ULL, 0x8646020080080080ULL,
        0x80020a0200100808ULL, 0x2010004880111000ULL, 0x623000a080011400ULL, 0x42008c0340209202ULL,
        0x209188240001000ULL,  0x400408a884001800ULL, 0x110400a6080400ULL,   0x1840060a44020800ULL,
        0x90080104000041ULL,   0x201011000808101ULL,  0x1a2208080504f080ULL, 0x8012020600211212ULL,
        0x500861011240000ULL,  0x180806108200800ULL,  0x4000020e01040044ULL, 0x300000261044000aULL,
        0x802241102020002ULL,  0x20906061210001ULL,   0x5a84841004010310ULL, 0x4010801011c04ULL,
        0xa010109502200ULL,    0x4a02012000ULL,       0x500201010098b028ULL, 0x8040002811040900ULL,
        0x28000010020204ULL,   0x6000020202d0240ULL,  0x8918844842082200ULL, 0x4010011029020020ULL
    };

    static inline Bitboard ROOK_ATTACKS[102400] = {};
    static inline Bitboard BISHOP_ATTACKS[5248] = {};

    static inline Magic ROOK_TABLE[64] = {};
    static inline Magic BISHOP_TABLE[64] = {};

    static inline MultiArray<Bitboard, 64, 64> SQUARES_BETWEEN_BITBOARDS = {};
    static inline MultiArray<Bitboard, 64, 64> SQUARES_ALIGNED_BITBOARDS = {};

    static void initSliders(Square square, Magic table[], [[maybe_unused]] UInt64 magic, const std::function<Bitboard(Square, Bitboard)> &attacks) {
        assert(square != Square::NONE);
        const Bitboard edges = ((Bitboard(Rank::FIRST) | Bitboard(Rank::EIGHTH)) & ~Bitboard(square.rank())) | ((Bitboard(File::A) | Bitboard(File::H)) & ~Bitboard(square.file()));
        UInt64 occupied = 0ULL;
        Magic &entry = table[static_cast<USize>(square.index())];
        entry.mask = (attacks(square, occupied) & ~edges).bits();
#if !defined(USE_PEXT)
        entry.magic = magic;
        entry.shift = 64 - static_cast<UInt64>(Bitboard(entry.mask).count());
#endif

        if (square.index() < 63) {
            table[static_cast<USize>(square.index() + 1)].attacks = entry.attacks + (1ULL << Bitboard(entry.mask).count());
        }

        do {
            entry.attacks[entry.hashIndex(occupied)] = attacks(square, occupied);
            occupied = (occupied - entry.mask) & entry.mask;
        } while (occupied);
    }

    template<PieceType::PieceTypeEnum PIECE_TYPE_ENUM>
    static Bitboard sliderSlow(Square square, Bitboard occupied) noexcept {
        static_assert(PIECE_TYPE_ENUM == PieceType::BISHOP || PIECE_TYPE_ENUM == PieceType::ROOK);
        assert(square != Square::NONE);
        static constexpr Int8 directions[2][4][2] = {
            {{1, 1},{1, -1},{-1, -1},{-1, 1}},
            {{0, 1},{1, 0},{0, -1},{-1, 0}}
        };
        const bool isRook = (PIECE_TYPE_ENUM == PieceType::ROOK);
        Bitboard attacks = 0ULL;

        Int32 file = square.file();
        Int32 rank = square.rank();
        for (UInt8 i = 0; i < 4; i++) {
            Int32 fileOffset = directions[isRook][i][0];
            Int32 rankOffset = directions[isRook][i][1];
            Int32 currentFile;
            Int32 currentRank;
            for (currentFile = file + fileOffset, currentRank = rank + rankOffset; currentFile >= 0 && currentFile < 8 && currentRank >= 0 && currentRank < 8; currentFile += fileOffset, currentRank += rankOffset) {
                const UInt8 index = Square(File(static_cast<UInt8>(currentFile)), Rank(static_cast<UInt8>(currentRank))).index();
                attacks.set(index);
                if (occupied.get(index)) {
                    break;
                }
            }
        }

        return attacks;
    }

    static std::pair<MultiArray<Bitboard, 64, 64>, MultiArray<Bitboard, 64, 64>> initSquarePaths() noexcept {
        MultiArray<Bitboard, 64, 64> betweenBitboards = {};
        MultiArray<Bitboard, 64, 64> alignedBitboards = {};

        auto path = [](PieceType pieceType, Square square, Bitboard occupied) {
            if (pieceType == PieceType::BISHOP) {
                return bishop(square, occupied);
            } else {
                return rook(square, occupied);
            }
        };

        for (UInt8 from = 0; from < 64; from++) {
            for (UInt8 to = 0; to < 64; to++) {
                Square square1 = Square(from);
                Square square2 = Square(to);
                const USize index1 = static_cast<USize>(square1.index());
                const USize index2 = static_cast<USize>(square2.index());

                if (from == to) {
                    betweenBitboards[index1][index2] = Bitboard(square1);
                    alignedBitboards[index1][index2] = Bitboard(square1);
                } else {
                    for (PieceType pieceType : {PieceType::BISHOP, PieceType::ROOK}) {
                        if (path(pieceType, square1, 0ULL).get(square2.index())) {
                            betweenBitboards[index1][index2] = (path(pieceType, square1, Bitboard(square2)) & path(pieceType, square2, Bitboard(square1))) | Bitboard(square2);
                            alignedBitboards[index1][index2] = (path(pieceType, square1, Bitboard()) & path(pieceType, square2, Bitboard())) | Bitboard(square1) | Bitboard(square2);
                        }
                    }
                }
            }
        }

        return {betweenBitboards, alignedBitboards};
    }
};

}
