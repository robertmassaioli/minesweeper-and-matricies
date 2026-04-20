# Performance Improvements

Ten concrete ways to improve the runtime performance of this codebase,
grounded in the actual hot paths identified in the code.

The program spends virtually all its time in two places: the outer loop
that runs 100,000 games (`main.cpp`) and the per-turn solver pipeline
(`solver::getMoves` → `matrix::gaussianEliminate`). All proposals below
target one of those two areas.

---

## 1. Parallelise the 100,000-Game Outer Loop

**Area:** Throughput / CPU utilisation

The 100,000 simulated games in `main.cpp` are completely independent of
each other — no shared mutable state except the final `results` counter.
This is textbook embarrassingly parallel work. Splitting the loop across
`std::thread` workers (or using OpenMP's `#pragma omp parallel for`) with
per-thread result accumulators merged at the end would give near-linear
speedup on any multi-core machine, cutting wall-clock time by roughly the
number of cores with zero change to the algorithm.

---

## 2. Replace `std::map` with `std::unordered_map` in the Solver

**Area:** Per-turn solver latency

`solver::getMoves` maintains two `std::map<int,int>` structures —
`idToPosition` and `positionToId` — that are built fresh every turn and
consulted repeatedly during matrix construction (line 82: `positionToId.find(position)`
and line 137: `positionToId[adjacentPosition]`). `std::map` is a red-black
tree with O(log n) lookup. For a 16×16 board the maps hold at most ~256
entries, making the constant factor dominate. Replacing both with
`std::unordered_map<int,int>` gives O(1) average lookup with no
algorithmic change.

---

## 3. Use a Flat Contiguous Matrix Instead of Heap-Allocated Rows

**Area:** Cache performance in Gaussian elimination

The matrix stores its rows as `std::vector<std::unique_ptr<Vector<A>>>`,
meaning each row is a separately heap-allocated object. During Gaussian
elimination every inner-loop iteration (`rows[iterRow]->add(rows[row].get())`)
chases two pointers to reach the actual data — one to the `unique_ptr`'s
target, one to the `std::vector<T>` inside `Vector<A>`. The row data for
different rows is scattered across the heap with no spatial locality.
Replacing this with a single `std::vector<A>` of size `rows * cols` laid
out in row-major order would eliminate all pointer chasing in the inner
loop and allow the hardware prefetcher to work effectively.

---

## 4. Eliminate the Full Board Scan Every Turn

**Area:** Per-turn solver latency

Every call to `solver::getMoves` performs two full O(width × height) scans
of the board: one to find all revealed number squares that still have
unclicked neighbours (`nonCompletedPositions`), and one to enumerate and
ID those neighbours (`positionToId` / `idToPosition`). On a 16×16 board
this is 256 cells × 8 neighbours = up to 2,048 checks per turn, repeated
every turn for every game. Maintaining an incrementally updated frontier
set — adding newly revealed squares after each move and removing squares
once all their neighbours are resolved — would reduce this to O(moves made
this turn × 8) rather than O(board size) per call.

---

## 5. Use Integer Arithmetic Instead of `double` in the Solver

**Area:** Arithmetic throughput in Gaussian elimination

The constraint matrix is built from board values that are integers (0–8)
and neighbour indicators that are 0 or 1. The variable solutions are also
binary (0 = safe, 1 = mine). Despite this, the solver uses `double`
throughout — `matrix<double>`, `Vector<double>`, all comparisons with
`1e-9` epsilon. On modern hardware, 64-bit integer SIMD throughput is
higher than double-precision floating-point throughput, and integer
arithmetic avoids the epsilon-comparison complexity introduced in
improvement #6 entirely. Switching to `matrix<int>` (or `matrix<int32_t>`)
with rational elimination would remove the floating-point noise problem at
its root and run faster.

---

## 6. Exploit Matrix Sparsity

**Area:** Arithmetic throughput in Gaussian elimination

Each row of the constraint matrix represents one revealed number square.
That square has at most 8 neighbours, so each row contains at most 8
non-zero coefficients out of potentially hundreds of columns. The current
Gaussian elimination iterates over every column in every row regardless.
Storing rows as sorted lists of `(column_index, value)` pairs and
iterating only over non-zero entries would reduce the inner-loop work from
O(columns) to O(non-zeros per row) — a factor of ~30× for a typical
mid-game board on Intermediate difficulty.

---

## 7. Reuse the Random Engine Across Board Generations

**Area:** Throughput — board generation cost

`game.cpp` currently constructs a fresh `std::mt19937{std::random_device{}()}`
on every call to `Board::generateGrid`. `std::random_device` involves a
system call on most platforms and is measurably slow. A single
`std::mt19937` engine initialised once at program startup and passed into
the game (or stored as a shared global) would eliminate this overhead for
all 100,000 board generations. As a side benefit, this makes the runs
reproducible when the initial seed is captured — addressing improvement #8.

---

## 8. Change `list<Move>` to `vector<Move>`

**Area:** Allocation and iteration overhead

`solver::getMoves` returns a `std::unique_ptr<std::list<Move>>`. A
`std::list` stores each `Move` in a separately heap-allocated node,
meaning every insertion allocates and every iteration chases pointers.
`Move` objects are small (a `Position` and a `ClickType` enum — roughly 12
bytes). Replacing `std::list<Move>` with `std::vector<Move>` gives
contiguous storage, removes per-element allocation overhead, and makes
iteration cache-friendly. Since moves are only ever appended and then
iterated once, there is no algorithmic reason to prefer a linked list.

---

## 9. Avoid Rebuilding `nonCompletedPositions` as a `std::list`

**Area:** Per-turn allocation overhead

`nonCompletedPositions` is built fresh each turn as a `std::list<Position>`,
iterated twice (once to ID neighbours, once to build matrix rows), and
then discarded. A `std::list` allocates one heap node per element. Since
the list is only ever appended to and forward-iterated, replacing it with a
`std::vector<Position>` would eliminate per-element allocations, improve
iteration locality, and allow `reserve()` with a reasonable upper bound
(e.g. `width * height`) to avoid any reallocation entirely.

---

## 10. Amortise `gaussianEliminate` Work Across Turns with Incremental Updates

**Area:** Algorithmic — Gaussian elimination cost per turn

Currently the solver rebuilds and fully re-eliminates the constraint matrix
from scratch on every turn, even though most rows are identical to the
previous turn. In practice, one solver turn typically reveals a small
number of new squares. Each newly revealed square adds at most one new row;
each newly flagged square only modifies the RHS constant of existing rows.
An incremental approach would maintain the matrix in already-eliminated
form between turns, apply the new rows via a single elimination pass against
the existing pivot structure, and update only the affected RHS constants for
flagged squares. This reduces per-turn Gaussian elimination cost from
O(rows² × cols) to O(new\_rows × rows × cols) — essentially free for turns
where only one or two squares are revealed.
