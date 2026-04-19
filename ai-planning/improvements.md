# Potential Improvements

Ten concrete improvements across different functional areas, ranked roughly from highest to lowest impact.

---

## 1. Solver Correctness — Probabilistic Guessing When Stuck

**Area:** Correctness / Win Rate

Currently when the solver runs out of deterministic moves it stops and records a PROGRESS result, leaving the game unfinished. In real Minesweeper you must guess to continue. Adding even a basic probabilistic layer — choose the hidden square with the lowest estimated mine probability — would turn a large fraction of the 57% stalled games into wins or outright losses. Studies on optimal Minesweeper play suggest a well-chosen guess lifts the win rate on Intermediate from ~43% to ~70%+. This is the single change with the biggest impact on the headline statistic the program reports.

---

## 2. Solver Correctness — Coupled Constraint Reasoning

**Area:** Correctness / Win Rate

The solver already applies a min/max lemma to individual rows after Gaussian elimination. The lemma works by checking whether the RHS of a row equals the minimum or maximum possible value for the sum of its unknowns. A row like `a + b + c = 3` hits the maximum, so all three variables must be mines. A row like `a + b + c = 0` hits the minimum, so all three are safe.

However, a row like `a + b + c = 2` has `min=0`, `max=3`, `val=2` — neither extreme is reached, so the lemma fires nothing and the solver silently gives up on that row. This is a genuinely underdetermined constraint: two of the three squares are mines but Gaussian elimination on that row alone cannot determine which two. This scenario is the primary driver of the 57% stall rate, as many mid-game board states produce constraints of this form.

The fix is to combine pairs of rows to derive new constraints. If row A says `x + y + z = 2` and row B says `x + y = 1`, subtracting gives `z = 1` (mine) — something neither row reveals alone. Adding a constraint-propagation pass that subtracts compatible row pairs after elimination would resolve more squares without any guessing, improving the win rate at zero cost in correctness.

---

## 3. Memory Management — Replace Raw Pointers with Smart Pointers

**Area:** Correctness / Safety

The codebase uses `new`/`delete` throughout and acknowledges this as a known limitation. There are several places where a `delete` is missing on an early-return path (e.g. `main.cpp` leaks the `tempLogger` if `printResults` throws). Replacing owning raw pointers with `std::unique_ptr` and non-owning ones with references or raw observing pointers would eliminate the entire class of leak and double-free bugs with no behavioural change and minimal code churn given the C++11 baseline already in use.

---

## 4. Test Coverage — Game and Solver Integration Tests

**Area:** Testing

The test suite covers only `matrix` and `Vector`. The `Board`, `Game`, and `solver` classes — which contain all the game logic and the core solving algorithm — have no automated tests at all. Adding integration tests that construct a known board state and assert that `solver::getMoves` returns the correct moves would catch regressions in the solving logic immediately. Specific cases worth covering: a board with all mines identified by the min/max lemma, a board where flagged neighbours reduce the RHS correctly, and a board where the solver correctly returns an empty list (no moves possible).

---

## 5. Performance — Avoid Re-solving the Entire Board Each Turn

**Area:** Performance

Every call to `solver::getMoves` rebuilds the full matrix from scratch, re-enumerates every revealed number square, and re-eliminates. For a 16×16 board this is fast enough that it does not matter at 100,000 games, but the approach does not scale. An incremental solver would only add new rows for squares revealed since the last turn and update existing rows for newly flagged neighbours. For larger board sizes (Expert: 30×16, 99 mines) this would reduce per-turn work substantially.

---

## 6. Correctness — Fix the Floating-Point Equality Comparisons

**Area:** Correctness

`matrix.h` and `solver.cpp` compare `double` values with `== 0.0`, `== 1.0`, and `!= 0.0` directly. This interacts badly with the direct-solve path in `solver.cpp` (line 316): after Gaussian elimination normalises the pivot to 1.0, the solver computes `pivot_var = val` and checks `if(val == 0.0 || val == 1.0)`. Because elimination accumulates floating-point rounding errors, a variable that should be exactly `1.0` may come out as `0.9999999999999998` or `1.0000000000000002`, causing the check to silently fail and leaving the square unclassified as unknown — a missed deduction on a board the solver should be able to solve. The same problem affects the min/max lemma's `val == minValue` and `val == maxValue` comparisons. Replacing all such comparisons with an epsilon test (e.g. `fabs(x - target) < 1e-9`) would make the solver robust to this class of numeric noise.

---

## 7. Code Quality — Replace the Hand-Rolled `optional<T>` with `std::optional`

**Area:** Code Quality / Maintainability

`solver.cpp` defines its own `optional<T>` template class to represent a result that may or may not be present. C++17 provides `std::optional<T>` in the standard library, which is semantically identical, better tested, and immediately recognisable to any C++ developer reading the code. Dropping the hand-rolled version reduces the file by ~30 lines and removes a maintenance burden.

---

## 8. Reproducibility — Seed Control and Deterministic Replays

**Area:** Usability / Debugging

The program seeds `rand()` from `time(NULL)` at startup, making runs non-reproducible. When a bug is found (e.g. the solver behaves incorrectly on a specific board), there is no way to reproduce that exact game without the seed, and even with the seed the `rand()` sequence is implementation-defined. Printing the initial seed to stdout (or accepting it as a CLI argument) would make any reported PROGRESS or unexpected result fully replayable. Switching to `<random>` with a `std::mt19937` would make the sequence portable across compilers.

---

## 9. Build System — Modernise CMake and Fix Warnings

**Area:** Build / Code Quality

Several pre-existing compiler warnings point to real issues:

- `random_shuffle` is deprecated since C++14 — replace with `std::shuffle` and a `std::mt19937`.
- Two `switch` statements compare `SquareState` against `ClickType` enumerators — the cases `FLAG` and `QUESTION` are `ClickType` values being matched against a `SquareState` variable, which silently always falls through.
- `logger` has no virtual destructor, so `delete log` where `log` is a `logger*` is undefined behaviour.

Fixing these removes all warnings and eliminates two latent bugs. The CMake minimum version could also be raised to `3.10` to silence the remaining deprecation notice.

---

## 10. Documentation — Inline Code Comments for Non-Obvious Logic

**Area:** Documentation

The solver's back-substitution loop in `solver.cpp` (lines 215–331) is the most complex part of the codebase and has almost no comments. The unsigned-integer overflow trick used to iterate backwards safely (noted in a comment at line 193) is the only place where a non-obvious invariant is called out. The min/max constraint lemma, the pivot-update logic, and the "swap known variables to the RHS" step would all benefit from a one-line comment explaining *why* each block exists, not just what it does. This would significantly lower the barrier for anyone wanting to extend the solver.
