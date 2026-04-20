#pragma once

#include "move.hpp"
#include "piece.hpp"
#include "types.hpp"


namespace Syft {

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
