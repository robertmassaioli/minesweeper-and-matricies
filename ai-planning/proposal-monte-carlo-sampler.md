# Proposal: Monte Carlo Probabilistic Solver (Option 3)

**Relates to:** `ai-planning/proposal-probabilistic-analysis.md` Option 3

---

## Problem

The solver halts with PROGRESS on roughly half of Intermediate games. At that
point it has exhausted all deterministic deductions but must still make a move
to have any chance of winning. Currently it simply stops. This proposal adds a
fallback: when no certain move exists, run a Monte Carlo sampler over the
remaining constraint matrix to estimate the mine probability for every unknown
square, then click the least dangerous one.

---

## Background: What the Solver Has at Stall Time

By the time `getMoves` returns an empty move list, the following structures
are already fully built and available:

| Structure | What it contains |
|-----------|-----------------|
| `solMat` | The RREF constraint matrix (`matrix<double>`, width = numVars + 1) |
| `positionToId` | Board position → variable column index (flat `vector<int>`) |
| `idToPosition` | Variable column index → board position (`vector<int>`) |
| `results` | Per-variable classification (`optional<bool>`, empty = unknown) |
| `grid` | Raw board state — used to find unconstrained unknowns |

The RREF matrix partitions the `numVars` unknown squares into two kinds:

- **Pivot variables** — their column contains a leading 1 in some row; their
  value is determined by the free variables via back-substitution.
- **Free variables** — their column has no pivot; they can be set
  independently. Any assignment of the free variables uniquely determines
  every pivot variable (though the result may violate the binary constraint
  `x ∈ {0, 1}`).

---

## The Two Populations of Unknown Squares

Before sampling we need to recognise that unknown squares fall into two
distinct groups:

**Constrained unknowns** — squares that appear in at least one constraint row.
These are exactly the squares tracked in `positionToId` / `idToPosition`.
The RREF encodes all linear relationships among them.

**Unconstrained unknowns** — squares that are `NOT_CLICKED` but are not
adjacent to any revealed number. No equation mentions them. Their only
relationship to the rest of the board is the global mine count:

```
mines_in_constrained + mines_in_unconstrained = total_remaining_mines
```

where `total_remaining_mines = board.mines - count(FLAG_CLICKED squares)`.

The sampler handles these two groups differently (see below).

---

## The Core Sampling Algorithm

### Step 1 — Identify free variables from RREF

Walk the RREF matrix row by row. For each non-zero row, find the pivot column
(the first column with a non-zero coefficient). Any variable column that is
never the pivot of any row is a free variable.

```
pivot_columns = {}
for row in 0..numNonZeroRows:
    for col in 0..numVars:
        if |RREF[row][col]| > EPSILON:
            pivot_columns.add(col)
            break   // first non-zero is the pivot

free_columns = {0..numVars-1} \ pivot_columns
```

After this, `|free_columns|` is typically small (0–15 for Intermediate
boards at stall time).

### Step 2 — Generate one candidate sample

Randomly assign each free variable to 0 or 1 with equal probability.

```
for col in free_columns:
    assignment[col] = bernoulli(0.5)
```

### Step 3 — Back-substitute to recover pivot values

For each non-zero row in the RREF, the pivot variable's value is:

```
pivot_value = RHS[row] - sum(RREF[row][col] * assignment[col]
                             for col != pivot_col)
```

If `pivot_value` is outside `[0, 1]` (with epsilon tolerance), or is not
close to an integer, this sample is **infeasible** — discard it.

Otherwise round to the nearest integer and store:

```
assignment[pivot_col] = round(pivot_value)
```

### Step 4 — Accept or reject

A sample is **accepted** if every pivot value was feasible (Step 3 did not
reject) AND, optionally, the global mine count constraint is satisfied (see
"Global Mine Count" below).

Accepted samples are accumulated into per-variable hit counters:

```
for col in 0..numVars:
    hitCount[col] += assignment[col]
acceptedCount++
```

### Step 5 — Repeat until budget is met

Run Steps 2–4 in a loop until `acceptedCount` reaches `targetSamples` or
the total attempt budget is exhausted (see "Acceptance Rate and Budgets").

### Step 6 — Compute constrained probabilities

```
for col in 0..numVars:
    if results[col].has_value():
        prob[col] = results[col].value() ? 1.0 : 0.0  // already determined
    else:
        prob[col] = hitCount[col] / acceptedCount
```

### Step 7 — Compute unconstrained probability

Count the expected total mines in the constrained region:

```
expected_constrained_mines = sum(prob[col] for col in 0..numVars)
```

Count unconstrained unknowns by scanning the full board for `NOT_CLICKED`
squares not present in `positionToId`:

```
unconstrained_squares = [pos for pos in board if
    grid[pos].state == NOT_CLICKED and positionToId[pos] == -1]
```

Probability for each unconstrained square (exchangeable — no constraint
distinguishes them):

```
remaining = total_remaining_mines - expected_constrained_mines
p_unconstrained = max(0.0, remaining) / unconstrained_squares.size()
```

### Step 8 — Choose a move

Pick the unknown square (constrained or unconstrained) with the lowest
probability. Emit a single `NORMAL` move for that square.

---

## Global Mine Count Constraint (Optional Refinement)

Including the global mine count as a hard accept/reject criterion makes the
probability estimates more accurate but lowers the acceptance rate.

With global count checking:

```
accept if:
    all pivot values feasible (as above)
    AND sum(assignment) == total_remaining_mines
```

Without it, the sampler produces probabilities conditioned only on the local
constraints. This is weaker but much cheaper when `total_remaining_mines` is
large relative to `numVars`.

**Recommendation:** Gate on a flag. Enable global mine count checking by
default for small boards and when `numVars >= total_remaining_mines` (i.e.
the constrained region is large relative to total mines).

---

## Acceptance Rate and Budgets

The acceptance rate depends on the tightness of the constraints:

- **Easy case** (few free variables, loose constraints): 50%+ acceptance.
  500 accepted samples costs ~1000 attempts — fast.
- **Typical midgame** (5–10 free variables): 10–40% acceptance.
  500 samples costs 1,250–5,000 attempts — still fast.
- **Hard endgame** (many free variables, tight constraints): acceptance can
  drop below 1%. 500 samples costs 50,000+ attempts — potentially slow.

Use a two-level budget:

```
MAX_ATTEMPTS   = 50,000    // absolute ceiling on total tries
MIN_ACCEPTED   = 100       // minimum for a usable estimate
TARGET_ACCEPTED = 500      // target for a good estimate
```

If `acceptedCount < MIN_ACCEPTED` when `MAX_ATTEMPTS` is exhausted, fall
back to a simple heuristic: pick the unknown that appears in the fewest
constraint rows (least constrained = least information = treat as globally
random).

Report the acceptance rate in the logger so tuning is possible.

---

## Parallelisation

Each sample attempt (Steps 2–4) is completely independent. The sampler is
therefore trivially parallelisable using the thread pool from performance
proposal #1.

Partition `TARGET_ACCEPTED` across `N = hardware_concurrency()` threads,
each with its own `std::mt19937` seeded from the program's main RNG:

```cpp
// In the sampler:
int perThread = targetAccepted / numThreads;

std::vector<std::thread> threads;
std::vector<SamplingResult> perThreadResults(numThreads);

for (int t = 0; t < numThreads; ++t)
{
    unsigned seed = static_cast<unsigned>(rand());
    threads.emplace_back(runSamplerThread,
                         std::ref(perThreadResults[t]),
                         seed, perThread, maxAttemptsPerThread,
                         std::cref(rrefData));
}
for (auto& th : threads) th.join();

// Merge per-thread hit counts and accepted counts.
```

No locking needed — each thread writes to its own `SamplingResult`.

---

## Data Passed Into the Sampler

To keep `sampler.cpp` independent of `solver.cpp`, extract the RREF into a
plain struct rather than passing the full `matrix<double>`:

```cpp
struct RrefSnapshot
{
    int numVars;          // number of unknown (variable) columns
    int numRows;          // number of non-zero rows after RREF
    // Stored row-major: rref[row * (numVars+1) + col]
    std::vector<double> data;
};
```

`solver.cpp` fills `RrefSnapshot` after calling `gaussianEliminate()`, before
calling the sampler. The snapshot owns no matrix memory and is cheap to copy
into worker threads.

---

## New Files and Changed Files

### New: `src/sampler.h`

```cpp
#ifndef MNM_SAMPLER_H
#define MNM_SAMPLER_H

#include <vector>

struct RrefSnapshot
{
    int numVars;
    int numRows;
    std::vector<double> data;      // row-major, width = numVars + 1
    double getValue(int row, int col) const;
};

struct SamplingConfig
{
    int targetAccepted    = 500;
    int maxAttempts       = 50000;
    int minAccepted       = 100;
    bool enforceGlobalCount = false;
    int totalRemainingMines = 0;   // required if enforceGlobalCount
};

struct SamplingResult
{
    std::vector<double> probabilities;  // length = numVars, constrained only
    int accepted;
    int attempted;
    double acceptanceRate;
    bool reliable;   // false if accepted < minAccepted
};

SamplingResult runMonteCarlo(const RrefSnapshot& rref,
                             unsigned seed,
                             const SamplingConfig& cfg);
#endif
```

### New: `src/sampler.cpp`

Contains `runMonteCarlo`, helper `identifyFreeColumns`, and the inner
sampling loop. No knowledge of `Board`, `Game`, or `solver` — only the
`RrefSnapshot` struct. This keeps the linear-algebra concern isolated from
the game concern.

### Changed: `src/solver.h` / `src/solver.cpp`

After the existing result-extraction loop, if `moves` is empty:

1. Build `RrefSnapshot` from `solMat`.
2. Count `totalRemainingMines` from the board.
3. Call `runMonteCarlo`.
4. If result is reliable: find the variable with the lowest probability,
   emit one `NORMAL` move via `idToPosition`.
5. If result is unreliable (low acceptance): fall back to a simple
   heuristic (pick the variable with the fewest non-zero entries in the RREF).

### Changed: `src/CMakeLists.txt`

Add `sampler.cpp` to the `game-solver` shared library.

---

## Integration Point in `solver.cpp`

```cpp
// After the existing moves loop (currently returns empty moves when stalled):

if (moves->empty())
{
    RrefSnapshot snap = buildSnapshot(solMat);

    SamplingConfig cfg;
    cfg.enforceGlobalCount  = (currentSquareId >= totalRemainingMines);
    cfg.totalRemainingMines = totalRemainingMines;

    SamplingResult sr = runMonteCarlo(snap, static_cast<unsigned>(rand()), cfg);

    (*log) << "MC: " << sr.accepted << "/" << sr.attempted
           << " samples accepted (" << sr.acceptanceRate * 100 << "%)"
           << logger::endl;

    if (sr.reliable)
    {
        int bestVar = pickLowestProbability(sr.probabilities,
                                           results,           // skip determined vars
                                           unconstrainedProb, // from step 7 above
                                           unconstrained);    // list of positions
        Position bestPos = board->posLoc(idToPosition[bestVar]);
        moves->push_back(Move(bestPos, NORMAL));
    }
}
```

---

## Expected Impact on Win Rate

At PROGRESS time the solver currently makes no move. With the sampler it
always makes one move — the move least likely to be a mine given the known
constraints. The expected improvement depends on how well the probability
estimates discriminate between squares.

Conservative estimate: on a stalled Intermediate board with ~20 remaining
unknowns and a correct probability estimate, the safest square has roughly
a 15–25% mine probability vs. a uniform-random 19% (40 mines / 212 non-mine
squares). The gain per stall is modest but compounds over many games.

An optimistic scenario: the RREF sometimes isolates a small region where one
square is nearly certain to be safe (probability < 5%). In these cases the
sampler turns a guaranteed-stall into a near-certain win. This is the case
where the improvement matters most.

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Very low acceptance rate makes sampler too slow | MAX_ATTEMPTS cap + reliable flag; fall back to heuristic |
| Acceptance rate varies wildly per board | Log acceptance rate; tune caps from actual data |
| Unconstrained probability estimate is inaccurate | Use expected_constrained_mines as a running correction after sampling |
| Sampler always picks the same square for a given board | Acceptable — the probability estimate is deterministic given the RREF |
| Results differ across runs (non-reproducible without `--seed`) | `runMonteCarlo` takes an explicit seed; caller passes from the program's RNG |
| `sampler.cpp` is hard to test without a full board | `RrefSnapshot` is a plain struct; unit tests can construct it directly from known matrices |
