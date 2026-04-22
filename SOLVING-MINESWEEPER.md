# Solving Minesweeper with Matrices

*Originally published January 12, 2013 by Robert Massaioli.*

---

## What is your motivation for writing this?

*Note: skip to the next section if you don't care about the back-story and want to get straight to the actual algorithm.*

Back in 2008 I was starting Computer Science at UNSW, enrolled in courses that became famous YouTube video lectures on Computer Science by Richard Buckland. I was also taking a standard first year Maths course covering Matrix mathematics. While observing a friend who loved playing Minesweeper and played it very quickly, I realized the game was simple enough to be solved computationally.

After writing out a simple Minesweeper game, the key insight struck: you could solve Minesweeper with matrices. I then created a program implementing this exact approach, solving Minesweeper without probabilities.

In December 2012, I discovered a blog post about Minesweeper AI solving methods. While commending the author, I noted that nobody seemed to realise Minesweeper could be solved using matrices with a special lemma specific to the game. After commenting on Reddit, I gained interest from people wanting to understand this method, prompting this detailed explanation and working implementation.

I acknowledge not being unique in discovering this approach. Someone else discovered it two years later, and I believe others found it previously. Matrices are just the natural way to solve this kind of problem.

---

## Quick Overview

This document covers:

- A simple example demonstrating how this method finds solutions to Minesweeper configurations
- A robust and reasonably efficient general algorithm for real-world application
- A brief description of the C++ implementation in this repository

### Prerequisites

- Linear algebra including matrices — they are valuable programming tools, especially for game development
- How to play Minesweeper

---

## The General Idea

When approaching a new problem, starting with a simple example helps conceptualisation. The simple example develops a rigorous model for general problem-solving, which then applies to more complicated scenarios.

### A Simple Example

Consider a small Minesweeper configuration with five unclicked squares — the "unknowns." Each square either contains a mine or doesn't. These unknowns become variables x₁ through x₅.

For each unclicked square xᵢ: if it contains a mine, its value equals 1; if not, it equals 0.

Looking at the top-right numbered square (containing 1), it is adjacent to exactly one mine. The adjacent unclicked squares are x₁ and x₂, so:

```
x₁ + x₂ = 1
```

Similar equations derive from the other numbered squares:

```
x₂ + x₃ = 1
x₂ + x₃ + x₄ = 2
x₃ + x₄ + x₅ = 1
x₄ + x₅ = 1
```

Writing with explicit coefficients:

```
1x₁ + 1x₂ + 0x₃ + 0x₄ + 0x₅ = 1
0x₁ + 1x₂ + 1x₃ + 0x₄ + 0x₅ = 1
0x₁ + 1x₂ + 1x₃ + 1x₄ + 0x₅ = 2
0x₁ + 0x₂ + 1x₃ + 1x₄ + 1x₅ = 1
0x₁ + 0x₂ + 0x₃ + 1x₄ + 1x₅ = 1
```

The augmented matrix:

```
[ 1 1 0 0 0 | 1 ]
[ 0 1 1 0 0 | 1 ]
[ 0 1 1 1 0 | 2 ]
[ 0 0 1 1 1 | 1 ]
[ 0 0 0 1 1 | 1 ]
```

After Gaussian elimination:

```
[ 1 1 0 0 0 | 1 ]
[ 0 1 1 0 0 | 1 ]
[ 0 0 0 1 0 | 1 ]
[ 0 0 1 0 1 | 0 ]
[ 0 0 0 0 0 | 0 ]
```

At first glance there is no unique solution. However, in boolean systems where each value equals either 0 or 1, this incomplete elimination still provides useful information.

Looking at row three: x₄ is the only non-zero coefficient column, equalling 1. Therefore x₄ = 1 — x₄ is a mine. Gaussian elimination simplified the matrix and enabled a partial solution even without a complete answer.

---

### A Special Rule

Consider these example rows after Gaussian elimination:

- **`1x₁ + 1x₄ = 2`** — both x₁ and x₄ are mines (the only way to sum to 2 with binary values)
- **`1x₁ + 1x₄ = 0`** — both are not mines
- **`1x₁ − 1x₄ = 1`** — x₁ is a mine and x₄ is not (maximum possible value is 1; the upper bound is met)
- **`1x₁ − 1x₄ = −1`** — x₁ is not a mine and x₄ is (minimum possible value is −1; the lower bound is met)

The general rule: calculate the upper bound (sum of positive coefficients) and lower bound (sum of negative coefficients) for each row. If the right-hand side equals either bound, the mine configuration for that row is fully determined.

| Coefficient sign | Row at lower bound | Row at upper bound |
|---|---|---|
| Positive | Not a mine | Mine |
| Negative | Mine | Not a mine |

**Algorithm for extracting partial solutions:**

1. Set maximum and minimum bounds to zero
2. For each column (excluding the augmented column): if the coefficient is positive, add it to the maximum; if negative, add it to the minimum
3. If the augmented value equals the minimum: negative-coefficient variables are mines, positive-coefficient variables are not
4. If the augmented value equals the maximum: positive-coefficient variables are mines, negative-coefficient variables are not

---

### The Robust Algorithm

Steps for the general algorithm:

1. **Identify** numbered squares adjacent to at least one unclicked/unflagged square
2. **Assign** unique matrix column indices to each unclicked neighbouring square
3. **Build** one matrix row per numbered square: set coefficients to 1 for adjacent unknowns, 0 elsewhere, and subtract already-flagged neighbours from the RHS
4. **Gaussian-eliminate** the matrix
5. **Extract** partial or full solutions by applying back-substitution and the special min/max rule, processing from the bottom row up
6. **Generate moves**: flag known mines, click known safe squares, leave undetermined squares alone
7. **Loop** steps 1–6 until no certain moves remain or the game ends

---

## The Implementation

The C++ source code is in this repository under the MIT licence.

### Design

The solver converts the visible board state into a constraint matrix and applies Gaussian elimination to determine which squares are definitively mines or safe. When no certain move exists, a Monte Carlo sampler estimates mine probabilities from the RREF constraint matrix and clicks the least dangerous square.

### Results

Testing 100,000 games at each difficulty level with the deterministic solver only (`--strategy none`):

#### Beginner (9×9, 10 mines)

```
WINs:      74090  (74.09%)
STALLed:   25910  (25.91%)
LOSSes:    0
```

#### Intermediate (16×16, 40 mines)

```
WINs:      43432  (43.43%)
STALLed:   56568  (56.57%)
LOSSes:    0
```

#### Expert (30×16, 99 mines)

```
WINs:      1707   (1.71%)
STALLed:   98293  (98.29%)
LOSSes:    0
```

Zero losses confirm the deterministic solver never clicks a mine. The stalled games are positions that genuinely require guessing; the Monte Carlo fallback (enabled by default) resolves most of these.

---

## Next Steps

- ~~Probabilistic solver for stalled games~~ — implemented via Monte Carlo sampling
- Multi-threading for parallel game simulation
- Improved test coverage for game and solver logic (currently only the matrix primitives are tested)
