# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
mkdir localbuild && cd localbuild
cmake ..
make
```

Run the solver (from build dir):
```bash
./src/mnm [optional-log-file]
```

Run tests:
```bash
cd localbuild && ctest
# or directly:
./test/run-all-tests
```

The program runs 100,000 simulated Minesweeper games (16×16, 40 mines) and reports win/progress/loss statistics.

## Architecture

The solver works by converting the visible board state into a matrix equation and applying Gaussian elimination to determine which squares are definitively mines or safe.

**Core components:**

- `src/game.h` / `src/game.cpp` — Game model: `Board` (grid state, click logic), `Game` (wraps Board, tracks `GameState`), `Move`, `Position`, `Dimensions`, `Square`. Board generation is deferred until the first move to guarantee the first click is safe.

- `src/solver.h` / `src/solver.cpp` — `solver::getMoves(Board*, logger*)` inspects the board and returns a list of certain moves (NORMAL clicks or FLAGs). Returns `NULL` or an empty list when no deterministic moves exist — the solver never guesses.

- `src/matrix.h` — Template `matrix<A>` with `gaussianEliminate()` (forward elimination only) and `solve()` (back-substitution returning a `Vector<A>*`). Returns `UNIQUE_SOLUTION`, `INFINITE_SOLUTIONS`, or `NO_SOLUTIONS`.

- `src/vector.h` — Template `Vector<A>` used as matrix rows.

- `src/logging.h` — `logger` abstraction with `ostream_logger` and `nop_logger`. Pass `logger*` through the call stack; log file path is the optional CLI argument.

**Memory management:** Raw `new`/`delete` throughout (no smart pointers). The codebase acknowledges this as a known limitation.

**Test coverage:** `test/testmatrix.cpp` and `test/testvector.cpp` test the linear algebra primitives only; game and solver logic have no automated tests.
