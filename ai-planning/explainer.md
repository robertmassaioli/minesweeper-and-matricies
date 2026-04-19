# How This Minesweeper Solver Works

This document explains the logic of the codebase in plain language, from the rules of the game through to how the solver turns a game board into a set of equations and solves them.

---

## 1. The Rules (as the code sees them)

A Minesweeper board is a grid of squares. Each square is in one of four states:

- **Not clicked** — hidden, could be anything.
- **Clicked** — revealed. Shows either a blank (no adjacent mines) or a number 1–8 (how many of its 8 neighbours are mines).
- **Flagged** — the player has marked it as a mine.
- **Question** — marked with a `?` (unused by the solver).

The game is won when every non-mine square has been clicked. It is lost the moment a mine square is clicked.

---

## 2. The Safe First Click

The board is **not generated until the first click**. Only after you pick a square does the program randomly place mines — and it guarantees that neither the clicked square nor any of its immediate neighbours will be a mine. This means the first click always opens up a safe region and gives the solver something to work with.

---

## 3. What the Solver Sees

After the first click, the visible board contains a set of **numbered squares**. Each numbered square tells you exactly how many of its (up to 8) hidden neighbours contain mines.

The solver's job is to figure out, from those numbers, which hidden squares are definitely mines and which are definitely safe — without guessing.

---

## 4. Turning the Board Into Equations

This is the core idea. Each hidden (unclicked, unflagged) square is treated as an **unknown variable** that is either 0 (safe) or 1 (mine).

Each numbered square gives us one **equation**: the sum of its hidden neighbours equals its number (minus any neighbours already flagged, since those are already accounted for).

### Example

Imagine a small section of the board:

```
[ ][ ][ ]
[ ][2][ ]
[ ][ ][ ]
```

The `2` in the middle has been revealed. It has 8 hidden unflagged neighbours. We label them `a` through `h`. The `2` gives us:

```
a + b + c + d + e + f + g + h = 2
```

If another revealed number nearby overlaps some of those same unknowns, it gives us a second equation. Collect one equation per revealed-and-useful numbered square, and you have a **system of linear equations**.

In the code (`solver.cpp`), this is built up row by row into a `matrix<double>`. Each column corresponds to one unknown hidden square, and the final column holds the right-hand-side constant (the number, minus already-flagged neighbours).

---

## 5. Gaussian Elimination

Once we have the matrix, we apply **Gaussian elimination** — the same technique taught in high-school algebra for solving simultaneous equations.

The process works by repeatedly combining rows to eliminate variables, until (ideally) each row contains only one variable and its value. For example:

```
Before elimination:         After elimination:
a + b + c = 2               a = 0  (safe!)
    b + c = 1               b + c = 1   (unknown)
a         = 0               ...
```

The implementation lives in `matrix::gaussianEliminate()` in `matrix.h`. It uses **partial pivoting** — always picking the row with the largest leading value — to stay numerically stable.

---

## 6. Reading the Results

After elimination, the solver walks back through the reduced matrix. For each row it tries to determine the value of the unknown:

**Case A — direct solution.** The row has been reduced to a single variable equal to 0 or 1. Done: that square is safe (0) or a mine (1).

**Case B — constraint reasoning.** The row still has multiple unknowns, but we can reason about the possible range of their sum:

- All variables are 0 or 1, so their weighted sum has a **minimum** (set every positive-coefficient variable to 0) and a **maximum** (set every positive-coefficient variable to 1).
- If the right-hand side **equals the minimum**, every positive-coefficient variable must be 0 (safe) and every negative-coefficient variable must be 1 (mine).
- If the right-hand side **equals the maximum**, every positive-coefficient variable must be 1 (mine).

This lets the solver extract certainties even from under-determined rows.

---

## 7. Turning Results Into Moves

Once every unknown has been classified as "definitely mine", "definitely safe", or "unknown":

- **Definitely mine** → emit a `FLAG` move for that square.
- **Definitely safe** → emit a `NORMAL` (click) move for that square.
- **Unknown** → do nothing (the solver never guesses).

The moves are returned as a list and applied to the board. The process then repeats: clicking safe squares reveals new numbers, which feed new equations into the next round of solving.

---

## 8. When the Solver Stops

The solver halts when it can no longer find any provably safe or provably mined square. At that point the game is recorded as **PROGRESS** (stalled) rather than a win or a loss. In a real game you would have to guess; this solver simply refuses to.

---

## 9. The Whole Loop at a Glance

```
Start
  │
  ▼
Random first click  ──►  Board generated (first click guaranteed safe)
  │
  ▼
┌─────────────────────────────────────────────────────┐
│  Solver turn                                        │
│                                                     │
│  1. Find all revealed numbers with hidden neighbours│
│  2. Assign an ID to every hidden neighbour square   │
│  3. Build a matrix: one row per useful number       │
│  4. Gaussian-eliminate the matrix                   │
│  5. Read off definite mines and safe squares        │
│  6. Emit FLAG / NORMAL moves                        │
└───────────────────┬─────────────────────────────────┘
                    │
          ┌─────────┴──────────┐
          │                    │
     Moves found          No moves found
          │                    │
          ▼                    ▼
   Apply moves to board     Stop (PROGRESS)
          │
     ┌────┴────┐
     │         │
   Won?      Lost?
     │         │
   Done       Done
     (WON)   (LOST — should never happen: solver only clicks proven-safe squares)
```

---

## 10. Why It Never Loses

The solver only ever clicks a square when the matrix equations **prove** it cannot be a mine. Because the proof is derived directly from the numbers on the board, a click on a "definitely safe" square cannot hit a mine. The 0% loss rate in the simulation is not luck — it is a logical guarantee.

The 43% win rate reflects games where the solver successfully clears the entire board using only deductive moves. The remaining 57% are games that reached a genuinely ambiguous state where no further deductions were possible.
