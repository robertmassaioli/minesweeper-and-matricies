# Proposal: Probabilistic Analysis When No Certain Moves Exist

## Context

The solver currently halts and records **PROGRESS** whenever it cannot find a square that is provably safe or provably mined. On Intermediate difficulty this happens in roughly half of all games. At that point the only path forward is to guess, and a uniformed guess is little better than random.

This document proposes five approaches for estimating the probability that each remaining unknown square is a mine, so that when the solver must guess it picks the least dangerous square rather than an arbitrary one.

The goal of all five approaches is the same: for every unknown square `x_i`, produce an estimate `P(x_i = 1)` — the probability it contains a mine — and then click the square with the lowest such probability.

---

## Option 1 — LP Relaxation of the Constraint Matrix (Matrix-Based)

**Core idea:** The constraint system `Ax = b` with `x_i ∈ {0, 1}` is an integer program. Relax the binary constraint to `0 ≤ x_i ≤ 1`. For each unknown `x_i`, solve two linear programs:

```
minimise   x_i
subject to  Ax = b
            0 ≤ x_j ≤ 1  for all j
```

and the same with `maximise x_i`. The resulting interval `[lo_i, hi_i]` is the tightest possible range for `x_i` given all constraints. Since the variables are ultimately binary, this gives a hard lower and upper bound on the probability. A natural point estimate is the midpoint `(lo_i + hi_i) / 2`.

**Why this is a direct extension of existing code:** The constraint matrix in RREF form is already available at the point where the solver stalls. The RREF already encodes all linear consequences of the constraints. LP over a RREF-prefactored matrix reduces to a sequence of cheap simplex pivots — at most one per variable per direction.

**What determinism looks like here:** When `lo_i = hi_i = 0`, the variable is provably safe (the solver's current output). When `lo_i = hi_i = 1`, it is provably a mine. The LP generalises rather than replaces the existing logic.

**Implementation sketch:**
- After `gaussianEliminate()`, add a simplex phase that treats the RREF as a starting basis.
- For each free variable (no pivot in its column), parametrically slide its value between 0 and 1 and propagate through the pivot equations to find the feasible interval.
- Choose the unknown with the smallest `hi_i` as the safest guess.

**Honest limitation:** LP relaxation gives bounds, not exact probabilities. The true probability is the fraction of valid binary solutions that have `x_i = 1`. LP gives an upper bound on this, which may be loose when many free variables are correlated.

**Verdict:** The most natural next step given the existing matrix infrastructure. Low implementation cost, exact bounds rather than approximations, no new dependencies.

---

## Option 2 — Exhaustive Solution Counting

**Core idea:** Use the RREF matrix to enumerate every valid binary assignment `x ∈ {0,1}^n` that satisfies `Ax = b`. For each unknown `x_i`, count how many solutions have `x_i = 1`. The probability is this count divided by the total.

The RREF form makes backtracking efficient: the pivot variables are determined by the free variables, so the search tree is over free variables only. Each node in the tree can be pruned early if any pivot equation produces a value outside `[0, 1]`.

**Example:** If RREF leaves 4 free variables, the search tree has at most `2^4 = 16` leaves — fully tractable. On a typical stalled Intermediate board there are usually 5–15 free variables in the active frontier, giving 32–32768 nodes.

**Exact result, combinatorial cost:** This is the only option that gives a provably correct probability. Every other option approximates or bounds it.

**What can go wrong:** In pathological late-game boards with large undetermined regions, the number of free variables can reach 20+, making the tree too large to enumerate quickly (1M+ nodes). A depth limit or time budget can fall back to Option 1 in those cases.

**Verdict:** The gold standard for correctness. Best suited as a complement to Option 1 — use exhaustive counting when the free-variable count is small (≤ 15), fall back to LP relaxation otherwise.

---

## Option 3 — Monte Carlo Constraint-Satisfying Sampling

**Core idea:** Instead of enumerating all solutions, randomly sample valid mine configurations. Repeat many times and estimate `P(x_i = 1)` as the fraction of samples in which square `i` is a mine.

**Sampling procedure:**
1. Identify the unknown squares not constrained by any equation (globally unconstrained — they appear in no number's neighbourhood). These are truly independent; assign each mine probability `k/m` where `k` is the total remaining mine count and `m` is the count of globally unconstrained squares.
2. For the constrained unknowns, use RREF to identify free variables, randomly assign each to 0 or 1 with uniform probability, compute dependent pivots, and accept the sample if all pivot values lie in `{0, 1}`.
3. Repeat until the desired number of accepted samples is obtained.

**Strengths:**
- Parallelises trivially — each sample is independent.
- Accuracy improves predictably with sample count.
- Handles correlated free variables correctly in expectation.

**Weaknesses:**
- Acceptance rate can be low (many random assignments produce infeasible pivots), so collecting enough accepted samples can be slow.
- Result is approximate and non-reproducible unless seeded.

**Verdict:** Good fit if the parallel simulation loop (performance proposal #1) is implemented — the same thread pool could generate samples in parallel. Higher implementation effort than Options 1 or 2 for modest accuracy gains.

---

## Option 4 — Bayesian Combinatorial Weighting (the "Tank" Method)

**Core idea:** This approach is widely used in competitive Minesweeper solvers. Partition the unknown squares into two groups:

- **Constrained:** appear in at least one constraint row.
- **Unconstrained:** appear in no constraint row; their only relationship to the board is the global mine count.

For the constrained group: for each subset of constrained unknowns, count how many assignments satisfy all constraints. Weight each square's mine probability by its frequency across valid assignments, normalised to the global mine count.

The key insight is that squares in the same constraint group are interdependent, while the unconstrained group is exchangeable and contributes a single probability `(remaining_mines - constrained_mines) / unconstrained_count`.

**How it differs from exhaustive counting (Option 2):** Option 2 enumerates individual solutions. The Tank method counts groups of equivalent solutions using combinatorics (`n choose k` formulas), making it faster for boards with large unconstrained regions.

**Verdict:** Effective in practice and widely used in the Minesweeper AI community. Requires more domain-specific reasoning than Options 1–3 and is harder to derive from the existing matrix code. A reasonable second-generation improvement after Option 2 is validated.

---

## Option 5 — Belief Propagation on the Constraint Factor Graph

**Core idea:** Model the board as a **factor graph**:
- A variable node for each unknown square `x_i`.
- A factor node for each constraint equation (each visible number square).
- Edges connecting each factor node to the variable nodes it references.

Run **loopy belief propagation**: iteratively pass probability messages between variable nodes and factor nodes until the messages converge. The resulting marginal belief at each variable node estimates `P(x_i = 1)`.

This is a standard technique from probabilistic graphical models. On tree-structured graphs it gives exact probabilities. On graphs with cycles (common in Minesweeper) it gives approximations that are often good in practice.

**Strengths:**
- Runs in time proportional to edges × iterations — typically fast.
- Naturally handles the global mine count as an additional factor.
- No enumeration or sampling required.

**Weaknesses:**
- On heavily loopy graphs (dense areas of the board) the approximation can be poor or oscillate without converging.
- Requires implementing message-passing infrastructure from scratch; does not reuse the matrix code.
- Harder to reason about correctness compared to Options 1–3.

**Verdict:** Theoretically attractive and fast, but the approximation quality is hard to predict and the implementation is the most complex of the five options. Suitable as a long-term research direction rather than a near-term improvement.

---

## Comparison Summary

| Option | Exact? | Uses matrix code? | Worst-case cost | Implementation effort |
|--------|--------|-------------------|-----------------|-----------------------|
| 1 — LP relaxation | Bounds only | Yes — direct extension of RREF | O(n²) per variable | Low |
| 2 — Exhaustive counting | Yes | Yes — RREF prunes tree | O(2^free_vars) | Medium |
| 3 — Monte Carlo sampling | Approximate | Partially | O(samples) | Medium |
| 4 — Bayesian / Tank | Yes (combinatorial) | No | O(2^frontier) | High |
| 5 — Belief propagation | Approximate | No | O(edges × iters) | High |

## Recommended Path

1. **Implement Option 1 (LP relaxation)** as the immediate next step. It extends the code we already have with minimal new machinery and upgrades every stalled game from "random guess" to "best guess given all linear constraints."
2. **Add Option 2 (exhaustive counting)** as a refinement, activated only when the free-variable count is small enough to be tractable. This gives exact probabilities for the cases that matter most.
3. Treat Options 3–5 as future research if the first two do not close the win-rate gap sufficiently.
