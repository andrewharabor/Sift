#pragma once

#include "move.hpp"
#include "piece.hpp"
#include "types.hpp"


namespace Clownfish {

struct HistoryNode {
    Move playedMove;
    Piece movedPiece;

    Int32 score;
};

class History {
public:

private:

};

}
