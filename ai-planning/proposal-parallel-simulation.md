# Proposal: Parallelise the 100,000-Game Simulation Loop

**Relates to:** `ai-planning/performance-improvements.md` item 1

---

## Problem

`main.cpp` runs 100,000 independent Minesweeper games sequentially in a
single thread. Each game is self-contained: it constructs its own `Game`,
runs `solver::getMoves` in a loop, and contributes one `GameState` to the
final counters. There is no shared mutable state between games during
execution. This is textbook embarrassingly parallel work, but the current
code uses none of the available CPU cores beyond one.

On a modern 8-core machine the simulation takes roughly 30–60 seconds.
Parallelising would reduce this proportionally to the core count with
essentially zero algorithmic change.

---

## Proposed Design

Use `std::thread` from the C++ standard library (already available at the
C++17 baseline). Divide the 100,000 runs evenly across N worker threads,
where N is `std::thread::hardware_concurrency()`. Each thread runs its own
sub-loop, accumulates its own local `results` object, and the main thread
merges the per-thread results after joining.

### Thread function

```cpp
static void runGames(int count, Dimensions dim, int mines, results& out)
{
    for (int i = 0; i < count; ++i)
    {
        unsigned int seed = rand();   // see note on rand() below
        srand(seed);
        nop_logger log;
        GameState state = solveRandomGame(dim, mines, &log);
        out.count(state, seed);
    }
}
```

### Main loop replacement

```cpp
const int numThreads = std::max(1u, std::thread::hardware_concurrency());
const int runsPerThread = testRuns / numThreads;

std::vector<results> perThreadResults(numThreads);
std::vector<std::thread> threads;

for (int t = 0; t < numThreads; ++t)
{
    int count = (t == numThreads - 1)
        ? testRuns - runsPerThread * (numThreads - 1)   // absorb remainder
        : runsPerThread;
    threads.emplace_back(runGames, count, dim, mines,
                         std::ref(perThreadResults[t]));
}

for (auto& th : threads) th.join();

results merged;
for (auto& r : perThreadResults) merged.merge(r);
```

A `merge()` method needs to be added to the `results` class to combine
win/loss/progress counters and loss-seed lists from two instances.

---

## Thread Safety Concerns

### `rand()` / `srand()`

`rand()` and `srand()` use global state and are not thread-safe. Under
concurrent calls from multiple threads the behaviour is undefined and the
seed sequences will interfere with each other, producing non-reproducible
results even with a fixed initial seed.

**Fix:** Replace the global `rand()`/`srand()` with per-thread
`std::mt19937` engines (already introduced in `game.cpp` for board
shuffling — performance improvement #7 proposes this more broadly).
Each thread initialises its own engine from a seed derived from the
initial random:

```cpp
std::mt19937 rng(initialSeed + threadIndex);
```

Pass the engine by reference into `solveRandomGame` and down to
`Board::generateGrid`, replacing the remaining `rand()` calls.

### `Board`, `Game`, `solver`, `matrix`

All of these are constructed locally within each game call. There is no
shared mutable state among them. No locking is required.

### `std::cout` progress reporting

The `"Processed N"` progress prints in the current loop are not
thread-safe. Options:
- Remove them entirely (the parallel version completes faster, making
  progress less useful).
- Replace with an `std::atomic<int>` counter incremented by each thread
  and a dedicated reporting thread that wakes every 500ms.
- Report only at completion.

---

## Expected Outcome

On an 8-core machine: ~8× throughput improvement, reducing a ~45-second
run to ~6 seconds. The win/loss/progress statistics are unaffected because
each game is independent and the RNG change preserves statistical
properties.

---

## Files to Change

| File | Change |
|------|--------|
| `src/main.cpp` | Replace sequential loop with thread pool; add `results::merge()`; replace `rand()`/`srand()` with per-thread `std::mt19937` |
| `src/game.h` / `src/game.cpp` | Accept `std::mt19937&` in `Board::generateGrid` instead of calling `rand()` |
| `src/solver.cpp` | No change required |

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Non-reproducible results if `rand()` is kept | Replace with per-thread `std::mt19937` seeded deterministically |
| Different result counts due to remainder division | Assign remainder to last thread; total always equals `testRuns` |
| Platform without `hardware_concurrency()` returning 0 | `std::max(1u, ...)` clamp ensures at least one thread |
