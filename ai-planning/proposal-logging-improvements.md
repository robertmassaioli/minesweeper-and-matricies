# Proposal: Logging Output Improvements

**Status:** Draft  
**Author:** Claude Code  
**Date:** 2026-04-20

## Background

The current log format (observable in `game.out`) is technically complete but practically unreadable. A reviewer trying to understand why the solver made a particular decision must mentally cross-reference four separate outputs — variable ID table, two matrix dumps, and cryptic pivot messages — none of which reference board coordinates. This proposal lists concrete improvements, ordered roughly by impact.

---

## Suggestion 1: Per-Turn Section Headers

### Problem
Consecutive solver turns run together with no visual separation. Turn 2 begins immediately after turn 1's output ends, making it impossible to tell where one turn's reasoning stops and the next begins, especially when the matrix dumps are long.

### Current output (excerpt)
```
0: 179
1: 180
...
Col 8 is a mine
8: mine
0: 179
1: 162
...
```

### Proposed output
```
════════════════════════════════════════
 SOLVER TURN 1
════════════════════════════════════════
...deductions...

════════════════════════════════════════
 SOLVER TURN 2
════════════════════════════════════════
...deductions...
```

### Implementation
Add a turn counter to the solver loop in `solver.cpp` / `main.cpp`. Print a header at the start of each `getMoves` call. Lightweight: one int and one log statement.

---

## Suggestion 2: Translate Variable IDs to Board Coordinates

### Problem
Variable IDs are printed as flat indices or sequential integers (e.g. `"0: 179"`, `"Col 8 is a mine"`). The position `179` in a 16×16 board is column 11, row 11. There is no way to connect a variable ID to a cell on the board without mental arithmetic and knowledge of the encoding.

### Current output (excerpt)
```
0: 179
1: 180
2: 195
...
Col 8 is a mine
8: mine
```

### Proposed output
```
Variable mapping:
  id=0  pos=(11,11)
  id=1  pos=(12,11)
  id=2  pos=(3,12)
  ...

Col 8 (pos=3,7) is a mine
```

### Implementation
The `positionToId` reverse map already exists in the solver. Add a helper to format `int flatIndex` → `(col, row)` using `flatIndex % width` and `flatIndex / width`. Use it everywhere a variable ID or column index is printed.

---

## Suggestion 3: Label Matrix Rows and Columns

### Problem
The matrix is printed as a wall of numbers with no row or column labels. Rows represent constraints from numbered board squares, but nothing in the output says which cell each row came from. Columns represent unknown squares, but their IDs are not printed above the matrix.

### Current output (excerpt)
```
1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 | 1
1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 | 1
0 0 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 | 1
```

### Proposed output
```
Constraint matrix (rows = numbered squares, cols = unknown variables):
              id: [  0   1   2   3   4   5 ...]
         (11,11): [  1   1   0   0   0   0 ...] = 1    (board(12,10) shows 2)
         (12,11): [  1   1   0   0   0   0 ...] = 1
          (3,12): [  0   0   1   1   0   0 ...] = 1
```

The column header row shows variable IDs; each data row is labelled with the board coordinate of the numbered square that generated the constraint, and an annotation of the clue value.

### Implementation
Requires passing coordinate metadata alongside the matrix. One approach: a `std::vector<std::string>` of row labels built in parallel with `matrix::addRow` calls in the solver. The labels are only used for logging, so they add no overhead to the solve path.

---

## Suggestion 4: Replace Cryptic Pivot/Value Messages with Human-Readable Deductions

### Problem
The current log contains messages like:
```
Pivot updated to: 12 => 1
value: 1 (true)
```

`"Pivot updated to: 12 => 1"` means "the pivot column changed to column 12 and its value is 1", but neither the column index nor the boolean `(true)` (which appears to be a `failedToFindValue` flag) is explained. A reader cannot tell whether this is progress or an error.

### Proposed output
```
[RREF] Row 4: pivot at col=12 (var id=12, pos=(5,9)), coefficient=1
[RREF] Deduced: var id=12 (pos=(5,9)) = 1.0  → MINE
[RREF] Deduced: var id=7  (pos=(8,6)) = 0.0  → SAFE
```

For the back-substitution phase, replace `"N: mine/not mine/NA"` with:
```
[Result] col=8, var id=8, pos=(3,7): MINE (value=1.0)
[Result] col=3, var id=3, pos=(6,4): SAFE (value=0.0)
[Result] col=5, var id=5, pos=(1,6): UNDECIDED (value=0.3)
```

### Implementation
In `matrix.cpp` and `solver.cpp`, replace raw index prints with formatted coordinate strings. Rename or remove the unexplained boolean flag; if it tracks a failure condition, log `[WARN] back-substitution found no unique value for var id=N` instead.

---

## Suggestion 5: Per-Turn Move Summary

### Problem
After all the matrix output, the log does not contain a clear summary of what the solver decided to do this turn. Moves are implied by the "Col N is a mine" / "N: mine" messages, but these are interleaved with debug output and easy to miss. There is no listing of all moves applied in a single turn.

### Proposed output
```
─── Turn 1 summary ──────────────────────
  Flagged as mines:  (3,7)  (11,4)
  Clicked as safe:   (2,5)  (6,8)  (9,3)
  Total moves this turn: 5
─────────────────────────────────────────
```

### Implementation
Collect the returned `vector<Move>` before applying it in `main.cpp` (it is already collected) and add a logging pass over it. A helper `printMoveSummary(logger*, const vector<Move>&)` keeps this out of the solver.

---

## Suggestion 6: Game-Level Header and Footer

### Problem
The log has no preamble explaining the game configuration. If you open `game.out` cold you do not know the board dimensions, mine count, random seed, or guessing strategy used.

### Proposed header
```
════════════════════════════════════════
 GAME START
 Board: 16 × 16,  mines: 40,  seed: 3748291
 Strategy: MONTE_CARLO
════════════════════════════════════════
```

### Proposed footer
```
════════════════════════════════════════
 GAME END: WON   (22 turns, 256 cells, 40 mines flagged)
════════════════════════════════════════
```

### Implementation
Print the header after `srand` in `solveRandomGame`. Print the footer with the `GameState` string after the solve loop. The random seed is already logged as `"Random Seed: N"`; this just reformats it into a structured header.

---

## Suggestion 7: Fix "-0" Display Artifacts

### Problem
Floating-point negation of zero prints as `-0` in the matrix dump:
```
-0  1  0  1  0  0 | 1
```
This is visually confusing and creates doubt about whether a coefficient is truly zero.

### Fix
Wrap matrix element printing with a zero check:
```cpp
double v = m.getValue(r, c);
if (std::fabs(v) < 1e-12) v = 0.0;  // collapse -0 and near-zero to 0
stream << v;
```
Alternatively, use `std::setprecision` with `std::fixed` so `-0.00` is obviously zero. This is a pure cosmetic fix with no algorithmic impact.

---

## Suggestion 8: Suppress or Condense the Variable ID Table

### Problem
The variable ID table is printed at full length every turn:
```
0: 179
1: 180
2: 195
...
41: 87
```
On turn 2 with 41 variables, this is 41 lines before any meaningful output. Combined with the matrix it dominates the log. For human review, the table's information is fully captured by the column headers in Suggestion 3.

### Options
- **Suppress entirely** if column-labelled matrix output (Suggestion 3) is implemented — the mapping is already embedded there.
- **Condense** to one line per row of board coordinates: `"Variables: (11,11) (12,11) (3,12) ..."`.
- **Gate on verbosity level** (see Suggestion 9).

---

## Suggestion 9: Verbosity Levels

### Problem
All of the above improvements still produce a long log for a single game. For bulk runs (100k games) the log is either completely suppressed (`nop_logger`) or overwhelmingly verbose. There is no middle ground.

### Proposal
Add a `LogLevel` to the `logger` interface:
```cpp
enum class LogLevel { ERROR, WARN, INFO, DEBUG, TRACE };
```
- `ERROR` / `WARN`: game outcome and any solver failures only.
- `INFO`: game header/footer + per-turn move summaries (Suggestions 5 & 6).
- `DEBUG`: adds human-readable deductions (Suggestion 4) and board prints.
- `TRACE`: adds full matrix dumps and variable tables (current behaviour).

The existing `--log` CLI option could gain an optional level suffix: `--log game.out:debug`.

### Implementation
Add `setLevel(LogLevel)` and `shouldLog(LogLevel) const` to the `logger` base class. Guard each log statement with a level check. This is the highest-effort suggestion but enables all others to co-exist without overwhelming low-verbosity runs.

---

## Priority Order

| # | Suggestion | Effort | Impact |
|---|---|---|---|
| 1 | Per-turn section headers | Low | High |
| 2 | Variable IDs → board coordinates | Medium | High |
| 5 | Per-turn move summary | Low | High |
| 6 | Game-level header and footer | Low | Medium |
| 7 | Fix -0 display artifacts | Trivial | Medium |
| 4 | Human-readable deductions | Medium | High |
| 3 | Labelled matrix rows and columns | High | Medium |
| 8 | Suppress variable ID table | Low | Low |
| 9 | Verbosity levels | High | Medium |

Suggestions 1, 5, 6, and 7 are quick wins requiring minimal refactoring. Suggestion 2 (coordinate translation) unlocks the readability of suggestions 3 and 4, so it is the highest-leverage single change. Suggestion 9 (verbosity levels) is the hardest but makes the others composable.
