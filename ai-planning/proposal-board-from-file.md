# Proposal: Load a Board from a Spec File

**Status:** Draft  
**Author:** Claude Code  
**Motivation:** Allow a specific board state to be fed to the solver for debugging,
verification, and cross-checking worked examples (e.g. the one in SOLVING-MINESWEEPER.md).

---

## Goal

Run the solver's full reasoning loop on a hand-crafted board — one whose mine positions
and revealed squares are entirely under the user's control — and observe exactly what
moves it deduces and why. The primary use case is verifying that the algorithm matches
the claims made in the documentation.

---

## File Format

A board spec file is a plain text file with two sections separated by a blank line.

**Section 1 — the visible board** (what the solver sees): the same character grid that
`Board::print()` already emits. Each character is one cell:

| Char | Meaning |
|---|---|
| `#` | unclicked unknown |
| `F` | flagged (player has already marked as mine) |
| `1`–`8` | revealed square showing neighbour mine count |
| ` ` (space) | revealed empty square (0 mine neighbours) |

**Section 2 — the mine truth map**: a second grid of the same dimensions using only two
characters:

| Char | Meaning |
|---|---|
| `*` | mine here |
| `.` | no mine |

The truth map is used solely to (a) know how many total mines the board has (needed by
the sampler's remaining-mines calculation) and (b) let the game engine respond correctly
if the solver ever clicks a mine (so the run exits with LOST rather than crashing).

### Example — the SOLVING-MINESWEEPER.md worked example

The document describes a 3×3 visible snippet with five unknowns (x₁–x₅) touching
numbered squares. Rendered as a 5×4 board (choosing the smallest grid that reproduces
the equations):

```
# # . .
1 # # .
. 2 # #
. 1 # #
```

Truth map for the same board (x₂ and x₄ are mines; x₁, x₃, x₅ are safe):

```
. * . .
. . * .
. . . *
. . . .
```

Running `./src/mnm --board-file example.board` should:
1. Load the visible state directly without random generation.
2. Run the solver loop until stall or win.
3. Print the full reasoning log to stdout (or to `--log` if specified).

---

## Architecture

### New: `BoardSpec` struct and parser

A self-contained `src/boardspec.h` / `src/boardspec.cpp` pair:

```cpp
struct BoardSpec {
    int width, height;
    // Parallel to grid: true = mine.
    std::vector<bool> mines;
    // Visible state for each cell.
    std::vector<SquareState> states;
    // Revealed value (EMPTY / 1–8 / MINE) for CLICKED cells; NA otherwise.
    std::vector<int> values;
};

// Returns nullopt and prints an error if the file is malformed.
std::optional<BoardSpec> loadBoardSpec(const std::string& path);
```

The parser validates that:
- Both grids are present and have identical dimensions.
- Every character is from the allowed sets above.
- Every CLICKED cell's revealed value is consistent with the truth map (i.e. the number
  equals the count of `*` in the eight surrounding truth-map cells). This catches
  hand-crafted files that are internally inconsistent before running the solver.

### New: `Board` constructor overload

Add a second constructor to `Board` that accepts a `BoardSpec` and populates `grid`
directly, bypassing `generateGrid`:

```cpp
Board(const BoardSpec& spec, logger* log);
```

Key differences from the random constructor:
- `generated = true` (no deferred generation needed; the grid is fully known up-front).
- `squaresLeft` is computed from the spec (number of `#` cells that are not mines).
- `mines` is taken from `spec.width * spec.height` minus the non-mine count, or
  equivalently the count of `*` in the truth map.
- The `clickSquare` path still works normally: clicking a mine cell reads `grid[pos].value
  == MINE` and returns LOST.

### New: `Game` constructor overload

Mirrors the Board change:

```cpp
Game(const BoardSpec& spec, logger* log);
```

This lets `main.cpp` construct a `Game` from spec without any changes to the
`solveRandomGame` function signature — the returned `Game*`/`Board*` is
indistinguishable from a randomly-generated one once constructed.

### CLI changes

Add `--board-file PATH` to `cli.cpp` and store it in `Config`:

```cpp
std::string boardFile;   // empty = random generation (current behaviour)
```

In `main.cpp`, branch on `cfg.boardFile`:

```cpp
if (!cfg.boardFile.empty()) {
    auto spec = loadBoardSpec(cfg.boardFile);
    if (!spec) return EXIT_FAILURE;
    // Run a single solve pass; --runs is ignored.
    GameState result = solveSpecGame(*spec, cfg.strategy, log.get());
    // print result
} else {
    // existing random loop
}
```

`solveSpecGame` is identical to `solveRandomGame` except it constructs `Game` from
`BoardSpec` instead of from random dimensions.

---

## Validation Pass in the Parser

When the parser reads both grids, it cross-checks every revealed cell:

```
for each cell (col, row) where visible[col,row] is '1'–'8':
    expected = count of '*' in truth map at the 8 neighbours
    if expected != digit:
        error: "cell (col,row) shows N but truth map has M mine neighbours"
```

This catches the most common authoring mistake (placing mines in a position that
contradicts the displayed clue number) before the solver ever runs.

---

## Usage

```bash
# Write a board file
cat > example.board << 'EOF'
# # . .
1 # # .
. 2 # #
. 1 # #

. * . .
. . * .
. . . *
. . . .
EOF

# Run the solver on it with full trace output
./src/mnm --board-file example.board --log /dev/stdout --log-level trace
```

With the worked example from SOLVING-MINESWEEPER.md, the expected output should show
the solver deducing all five unknowns in one turn via RREF back-substitution, directly
contradicting the document's claim that only x₄ can be determined.

---

## Scope and Non-Goals

- This feature runs one game only (`--runs` is ignored when `--board-file` is set).
- The board file does not support multiple turns or scripted move sequences; that would
  require a separate "replay" feature.
- The feature does not modify the random game loop in any way.
- No new test infrastructure is proposed here, though the board file format would make it
  straightforward to add regression tests for specific solver behaviours in future.

---

## Implementation Order

1. `boardspec.h` / `boardspec.cpp` — struct, parser, validator.
2. `Board(const BoardSpec&, logger*)` constructor.
3. `Game(const BoardSpec&, logger*)` constructor.
4. `solveSpecGame` function in `main.cpp`.
5. `--board-file` CLI option.
6. Write the SOLVING-MINESWEEPER.md worked example as `examples/worked-example.board`.
