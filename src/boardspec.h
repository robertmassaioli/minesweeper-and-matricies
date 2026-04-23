#ifndef MNM_BOARDSPEC_H
#define MNM_BOARDSPEC_H

#include "game.h"
#include <optional>
#include <string>
#include <vector>

struct BoardSpec {
    int width, height;
    std::vector<bool> mines;       // true = mine at this cell (row-major)
    std::vector<SquareState> states;
    std::vector<int> values;       // EMPTY/1-8/MINE for CLICKED cells; NA otherwise
};

// Returns nullopt and prints an error to stderr if the file is malformed.
std::optional<BoardSpec> loadBoardSpec(const std::string& path);

#endif
