#pragma once

#include "move.hpp"
#include "piece.hpp"
#include "types.hpp"


namespace Clownfish {

struct HistoryStack {
    Move playedMove;
    Piece movedPiece;

    Int32 score;
};

class History {
public:

private:

};

}
