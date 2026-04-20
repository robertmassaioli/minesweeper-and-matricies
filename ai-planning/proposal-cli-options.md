# Proposal: Command-Line Options for the Main Program

## Problem

`main.cpp` has four values that anyone experimenting with the solver needs to change:

| Variable | Current value | Why you'd change it |
|----------|--------------|---------------------|
| `dim` (width × height) | 16 × 16 | Test Beginner (9×9) or Expert (30×16) |
| `mines` | 40 | Match a different difficulty |
| `testRuns` | 100,000 | Quick smoke-test vs. high-confidence run |
| log file path | (none) | Capture per-game trace |

Right now every change requires editing source and recompiling. A proper CLI would let you do:

```
./mnm --runs 1000 --width 9 --height 9 --mines 10
./mnm --preset expert --runs 50000 --log trace.txt
./mnm --help
```

---

## Implementation Approach

Use the C++ standard library only — no third-party dependencies. Parse `argv` manually with a small helper that recognises `--key value` and `--flag` forms. This keeps the build system unchanged and adds no new files beyond `main.cpp` itself.

The parser is straightforward for a fixed set of known flags and can be contained in a single `parseArgs` function returning a plain config struct.

---

## Proposed Interface

```
Usage: mnm [options]

Options:
  --width N         Board width  (default: 16)
  --height N        Board height (default: 16)
  --mines N         Number of mines (default: 40)
  --runs N          Number of simulated games (default: 100000)
  --preset NAME     Shorthand: beginner | intermediate | expert
                    Sets width, height, and mines together.
                    Individual flags override preset values.
  --seed N          Fix the initial RNG seed (default: time-based)
  --log FILE        Write per-game trace to FILE
  --help            Print this message and exit
```

### Presets

| Name | Width | Height | Mines |
|------|-------|--------|-------|
| `beginner` | 9 | 9 | 10 |
| `intermediate` | 16 | 16 | 40 |
| `expert` | 30 | 16 | 99 |

Presets are applied first; individual flags win if both are given.

### Validation

Print an error and exit with a non-zero code if:
- `mines >= width * height` (more mines than squares)
- `width` or `height` is less than 1
- `runs` is less than 1
- An unrecognised flag is encountered
- A flag that expects a value is given without one

---

## What Changes in main.cpp

1. Add a `Config` struct holding all the options with their defaults.
2. Add a `parseArgs(int argc, char** argv) -> Config` function. Iterates `argv`, matches known flags, fills the struct. Prints usage and exits on `--help` or bad input.
3. Replace the four hardcoded variables with fields from `Config`.
4. Pass `config.seed` to `srand` (and print it, as is done now for `initialRandom`) so runs are reproducible when `--seed` is used.

No other files change. `game.h`, `solver.h`, `matrix.h` are unaffected.

---

## Files to Change

| File | Change |
|------|--------|
| `src/main.cpp` | Add `Config` struct, `parseArgs`, replace hardcoded values |

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Existing positional `argv[1]` log-file arg breaks | Replace with `--log FILE`; document the change |
| Integer overflow / negative values passed as N | Validate after `std::stoi`; catch `std::invalid_argument` and `std::out_of_range` |
| `mines` close to `width*height` causes infinite loop in board generation | Existing assert catches this; CLI validation adds a friendlier message before it fires |
