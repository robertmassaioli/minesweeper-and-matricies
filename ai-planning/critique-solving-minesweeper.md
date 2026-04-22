# Critique of SOLVING-MINESWEEPER.md

This document records logical, mathematical, and presentation issues found in the algorithm
description. Issues are ordered roughly by severity.

---

## 1. The Worked Example Produces a Unique Solution — Not a Partial One

**What the document claims:**

After Gaussian elimination, only x₄ can be determined. The document says "The game
continues without further determinable information."

**What is actually true:**

The five-equation, five-unknown system as written has a *unique* solution. Tracing the
actual RREF algorithm (which uses partial pivoting and eliminates above and below each
pivot) on the input matrix:

```
[ 1 1 0 0 0 | 1 ]
[ 0 1 1 0 0 | 1 ]
[ 0 1 1 1 0 | 2 ]
[ 0 0 1 1 1 | 1 ]
[ 0 0 0 1 1 | 1 ]
```

produces:

```
[ 1 0 0 0 0 | 0 ]  →  x₁ = 0  (safe)
[ 0 1 0 0 0 | 1 ]  →  x₂ = 1  (mine)
[ 0 0 1 0 0 | 0 ]  →  x₃ = 0  (safe)
[ 0 0 0 1 0 | 1 ]  →  x₄ = 1  (mine)
[ 0 0 0 0 1 | 0 ]  →  x₅ = 0  (safe)
```

All five variables are determined immediately. The document's presentation of a partial,
ambiguous result is incorrect for this example and undermines the reader's understanding
of how powerful the method actually is.

---

## 2. The Document's Shown Matrix Result Is Not Reachable

**What the document shows after elimination:**

```
[ 1 1 0 0 0 | 1 ]
[ 0 1 1 0 0 | 1 ]
[ 0 0 0 1 0 | 1 ]
[ 0 0 1 0 1 | 0 ]
[ 0 0 0 0 0 | 0 ]
```

**Why this is wrong:**

Two separate errors:

1. **Invalid row echelon form.** Row 3 has its leading non-zero entry at column 3, while
   row 4 has its leading non-zero entry at column 2. Any correct row echelon form requires
   pivot positions to move strictly rightward as row index increases. This result cannot
   come from any standard Gaussian elimination implementation.

2. **Row 5 should not be all zeros.** The five constraints are linearly independent (the
   system has a unique solution), so no row can be annihilated to zero during forward
   elimination. The fifth row should be `[0 0 0 0 1 | 0]`, encoding x₅ = 0.

The result shown appears to be a partially hand-computed intermediate state that was
then abandoned, not the output of a complete elimination pass.

---

## 3. "Finishing the Simple Example" Claims x₃ Is a Mine — It Is Not

The document states that after applying the special rule, x₃ is identified as a mine.
The correct solution is x₃ = 0 (safe). This is easily verified: from the equations
`x₃ + x₄ + x₅ = 1` and `x₄ + x₅ = 1`, subtracting gives x₃ = 0. The document's
conclusion about x₃ contradicts its own system of equations.

This error compounds issue 1: because the document incorrectly showed an incomplete
elimination, it then derived a false conclusion about x₃ from that wrong intermediate
state.

---

## 4. "Gaussian Elimination" vs Full RREF — A Terminology Mismatch

**What the document calls it:** "Gaussian elimination," which conventionally means
*forward elimination only*, producing row echelon form (REF).

**What the code actually does:** Full *Reduced* Row Echelon Form (RREF). The
`gaussianEliminate()` function eliminates each pivot column from **all** other rows, not
just those below. This is Gauss-Jordan elimination, not plain Gaussian elimination.

The distinction matters because:

- In plain Gaussian elimination, you need a separate back-substitution pass to recover
  variable values. The special min/max lemma is needed partly to compensate for this.
- In full RREF, any row with a single non-zero variable coefficient directly encodes that
  variable's value without any further reasoning — the "special rule" reduces almost
  entirely to a direct read-off.

Using the wrong term throughout the document creates confusion about why the back-reading
pass exists at all.

---

## 5. The Special Rule's Scope Is Understated

The document presents the min/max lemma as a novel insight specific to Minesweeper. In
fact it is an instance of a well-known technique in **integer linear programming**: given
a linear constraint over binary (0/1) variables, if the right-hand side equals the sum of
all positive coefficients (or all negative coefficients), the assignment of every variable
is forced.

More importantly, the document does not clearly state the *precondition* for the rule:
it is only valid when every variable appearing with a non-zero coefficient in the row is
still **unknown** (i.e., not yet resolved). The implementation handles this by
substituting already-resolved variables into the RHS before applying the lemma, but this
step is entirely absent from the document's description. A reader implementing the
algorithm from the document would likely miss it.

---

## 6. The Algorithm Is Incomplete — It Misses Deducible Mines

The document correctly notes that the solver sometimes stalls. What it does not explain
is *why*: the single-row min/max lemma cannot detect mines that are deducible only by
**combining** two or more constraints. For example:

```
Row A:  x₁ + x₂ = 1
Row B:  x₂ + x₃ = 1
```

Neither row meets its min/max bound (min=0, max=1 for both; RHS=1 = max, but two
variables exist so neither is individually forced). Yet subtracting B from A gives
`x₁ − x₃ = 0`, meaning x₁ = x₃. This derived constraint, combined with additional
board context, can sometimes resolve variables that the single-row rule cannot.

Full RREF handles this automatically through the column elimination step, but the
document never explains this key mechanism or its limits.

---

## 7. Negative Coefficients Appear Without Explanation

The "Special Rule" section introduces negative coefficients with examples like
`1x₁ − 1x₄ = 1`. The document never explains how a matrix whose initial entries are all
0 or 1 can produce negative entries after elimination. The answer is that RREF subtraction
of rows with overlapping unknown squares generates negatives, but a reader unfamiliar with
RREF would be confused about where these come from and whether the rule still applies.

---

## 8. The Robust Algorithm Omits a Critical Detail in Step 3

Step 3 says: "subtract already-flagged neighbours from the RHS."

This is correct but understated. Flagged squares are confirmed mines (value = 1). Failing
to subtract them causes the RHS to be wrong — the solver would think more unknowns need
to be mines than actually remain. This is one of the most common implementation errors
for this kind of solver and deserves a fuller explanation.

---

## 9. The First Click Problem Is Ignored

Standard Minesweeper boards are generated *after* the first click so that it is
guaranteed safe. The document presents the algorithm as if a complete board is available
from the start. In practice, the solver loop cannot begin until at least one safe square
has been revealed, which requires either a lucky first click or a generator that ensures
the first click is always safe. The implementation defers mine placement until the first
move, but the document makes no mention of this.

---

## 10. No Documentation of the Probabilistic Fallback

The current implementation includes a Monte Carlo sampler that estimates mine
probabilities when no certain move exists and clicks the least dangerous square. The
document's "Next Steps" section lists this as future work but it is already done. The
description of the algorithm is therefore incomplete: a significant portion of the
solver's behaviour in practice is undocumented.

---

## Summary

| # | Issue | Severity |
|---|---|---|
| 1 | Worked example claims partial solution; full solution exists | High |
| 2 | Shown matrix result is not reachable by any standard elimination | High |
| 3 | x₃ identified as mine when it is safe | High |
| 4 | "Gaussian elimination" vs full RREF terminology mismatch | Medium |
| 5 | Special rule precondition (substitute knowns first) is absent | Medium |
| 6 | Algorithm incompleteness not explained | Medium |
| 7 | Negative coefficients appear without explanation of origin | Low |
| 8 | Flagged-neighbour subtraction deserves more emphasis | Low |
| 9 | First-click problem not addressed | Low |
| 10 | Monte Carlo fallback entirely absent from documentation | Medium |

The most impactful fix would be to redo the worked example correctly — either choose a
genuinely ambiguous board position to illustrate partial solutions, or follow through the
full RREF result on the current example and show that all five variables are resolved.
Correcting the example would cascade into fixing issues 1–3 and would give the document
a much stronger foundation.
