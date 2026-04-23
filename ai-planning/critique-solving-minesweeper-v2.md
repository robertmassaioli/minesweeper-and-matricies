# Critique of SOLVING-MINESWEEPER.md (v2)

This document records issues found in the current version of the algorithm description,
after the worked example was updated to use the `#####/12221` board.

Issues are ordered roughly by severity.

---

## 1. "Gaussian Elimination" vs Full RREF — Terminology Still Misleading

**What the document says:**

> After Gaussian elimination (full reduced row echelon form)

**What is actually true:**

The parenthetical correctly names the operation, but the primary label "Gaussian
elimination" is still wrong. Gaussian elimination conventionally means *forward
elimination only*, producing row echelon form (REF). What the code performs — and
what the document's own matrix shows — is *Gauss-Jordan elimination*, which eliminates
each pivot column from **all** rows (above and below), producing the reduced row echelon
form (RREF).

The distinction matters to any reader who knows the terms: a Gaussian-eliminated REF
matrix needs a separate back-substitution pass to read off variable values; an RREF
matrix does not. Since the worked example reads x₃ directly from a single row without
any back-substitution, the result is unambiguously RREF.

The algorithm step 5 compounds this by saying "applying back-substitution and the
special min/max rule, processing from the bottom row up" — back-substitution is REF
language. The code never does back-substitution; it reads pivot values directly from
the RREF.

**Fix:** Replace "Gaussian elimination" with "Gauss-Jordan elimination" (or simply
"RREF") throughout. Remove the mention of back-substitution in step 5.

---

## 2. The Worked Example Does Not Show Why the Remaining Rows Are Underdetermined

**What the document says:**

> The remaining rows each contain two unknowns and no row meets its min/max bound,
> so no further deductions are possible from this position alone.

**What is missing:**

The document asserts this without showing the work. A reader who has just been
introduced to the min/max rule in the very next section would have no way to verify it.
After the RREF the four remaining constrained rows are:

```
x₁ + x₅ = 1     (max=2, min=0, RHS=1 — not at either bound)
x₂ − x₅ = 0     (max=1, min=−1, RHS=0 — not at either bound)
x₄ + x₅ = 1     (max=2, min=0, RHS=1 — not at either bound)
```

Showing these three checks explicitly would close the loop on the example and also
give the reader a second concrete application of the special rule (the "rule fails to
fire" case) before the formal description.

It would also be useful to state that the position has exactly two valid mine
configurations — (x₁=1, x₃=1, x₄=1) and (x₂=1, x₃=1, x₅=1) — so the ambiguity is
genuine, not an artefact of incomplete elimination.

---

## 3. The Special Rule Section Is Disconnected from the Worked Example

**What the document does:**

Introduces the min/max rule using hypothetical rows `1x₁ + 1x₄ = 2`, `1x₁ − 1x₄ = 1`,
etc., that have no relation to any row in the worked example.

**What the problem is:**

The reader has just seen a 5×5 RREF matrix with specific rows and been told x₃=1 is
deducible from row 3. The natural question is: "how does the solver *read* x₃=1 from
row 3?" The answer is the min/max rule applied to `[0 0 1 0 0 | 1]` (max=1, RHS=1 →
upper bound met → x₃ is a mine). But the document never makes this connection.

Instead the special rule section jumps to unrelated example rows. The reader must
mentally connect the two sections themselves.

**Fix:** Before or within the special rule section, show the rule applied to row 3 of
the worked RREF as the primary illustration. Then show the non-firing cases from rows
1, 2, and 4.

---

## 4. Negative Coefficients Appear in the RREF Without Explanation

**What the document shows:**

The RREF contains `−1` in row 2 (`[0 1 0 0 −1 | 0]`). This is the first and only
negative coefficient in the document other than the artificial examples in the special
rule section.

**What is missing:**

The document never explains how a matrix whose initial entries are all 0 or 1 can
produce negative entries. The answer — that row subtraction during RREF can yield
negatives when a row with a 1 in column *j* is subtracted from a row with a 0 in
column *j* — is obvious to someone who has done the arithmetic, but non-obvious to the
target reader. A single sentence noting that RREF subtraction produces negatives would
prevent confusion.

---

## 5. The Special Rule Precondition (Substitute Known Values First) Is Absent

**What the document implies:**

The rule is applied to rows as they appear after RREF.

**What is actually required:**

The rule is only valid when every variable appearing in the row is still *unknown*. When
a variable has already been resolved (e.g., x₃ is flagged as a mine in turn 1), its
known value must be substituted into the RHS of every row that contains it before
re-applying the rule in subsequent turns. The implementation does this implicitly
(flagged squares are excluded from the column index set and their mine count is already
subtracted from the RHS at matrix build time), but the document says nothing about it.

This is one of the most common implementation mistakes for this kind of solver.

---

## 6. The Robust Algorithm's Step 5 Describes REF, Not RREF

Step 5 reads:

> Extract partial or full solutions by applying back-substitution and the special
> min/max rule, processing from the bottom row up.

"Back-substitution" and "processing from the bottom row up" are the procedure for
plain Gaussian elimination (REF). In the actual implementation, RREF has already
eliminated each pivot variable from all other rows, so there is no back-substitution
and no need to process rows in a particular order. Each row is checked independently
for a direct-value read-off (single non-zero coefficient) or a min/max bound hit.

---

## 7. The First-Click Problem Is Ignored

The algorithm begins "identify numbered squares adjacent to at least one
unclicked/unflagged square." On a freshly generated board before any click, there are
no numbered squares at all — every square is unclicked. The algorithm as written
cannot start.

The implementation defers mine placement until after the first move, guaranteeing the
first click opens a safe region. The document makes no mention of this, leaving a gap
between the algorithm description and a reader who tries to implement it from scratch.

---

## 8. The Monte Carlo Fallback Is Absent from the Algorithm Steps

The seven-step algorithm ends with "loop until no certain moves remain or the game
ends." In practice, the solver also loops when the Monte Carlo guesser fires — it does
not simply stop when deterministic moves run out. The Design section mentions Monte
Carlo, but the algorithm steps do not, so a reader following the steps would implement
a solver that stalls instead of guessing.

---

## 9. The Flagged-Neighbour Subtraction in Step 3 Is Understated

Step 3 says "subtract already-flagged neighbours from the RHS." This is correct but
terse. Flagged squares represent confirmed mines (value=1); failing to subtract them
makes the RHS wrong for every subsequent turn after the first flag is placed. This is
the correct interpretation of the constraint, but a reader unfamiliar with constraint
solvers will not recognise why this step is necessary or what goes wrong if it is
omitted.

---

## Summary

| # | Issue | Severity |
|---|---|---|
| 1 | "Gaussian elimination" still used; step 5 still mentions back-substitution | Medium |
| 2 | Underdetermined rows not shown with bound calculations; two valid configurations not stated | Medium |
| 3 | Special rule section not connected to worked example | Medium |
| 4 | Negative coefficients in RREF appear without explanation | Low |
| 5 | Special rule precondition (substitute knowns first) still absent | Medium |
| 6 | Step 5 describes REF procedure, not RREF | Medium |
| 7 | First-click problem still ignored | Low |
| 8 | Monte Carlo fallback absent from algorithm steps | Low |
| 9 | Flagged-neighbour subtraction deserves more explanation | Low |

---

## Status of Issues from the Previous Critique

The following maps each item from `critique-solving-minesweeper.md` to its current
status after the worked example update.

| Old # | Summary | Status |
|---|---|---|
| 1 | Worked example claims partial solution; full solution exists | **Resolved.** The new `#####/12221` board genuinely has only one deterministic deduction (x₃), so a partial solution is the correct outcome. |
| 2 | Shown matrix result not reachable by any standard elimination | **Resolved.** The RREF shown is correct and matches the solver's actual output. |
| 3 | x₃ identified as mine when it is safe | **Resolved.** x₃ is correctly identified as a mine (it is one in the truth map). |
| 4 | "Gaussian elimination" vs full RREF terminology mismatch | **Partially resolved.** The document now adds "(full reduced row echelon form)" as a parenthetical, but "Gaussian elimination" remains the primary term and step 5 still uses back-substitution language. See issue 1 above. |
| 5 | Special rule precondition (substitute knowns first) absent | **Still outstanding.** See issue 5 above. |
| 6 | Algorithm incompleteness (multi-row deductions) not explained | **Still outstanding** as issue 8 (Monte Carlo) and issue 6 (step 5 description). The deeper point — that the min/max lemma cannot detect mines deducible only by combining two rows — is still unaddressed. RREF handles this automatically, but the document does not explain why. |
| 7 | Negative coefficients appear without explanation | **Still outstanding.** Now more visible because the worked RREF itself contains −1. See issue 4 above. |
| 8 | Flagged-neighbour subtraction deserves more emphasis | **Still outstanding.** See issue 9 above. |
| 9 | First-click problem not addressed | **Still outstanding.** See issue 7 above. |
| 10 | Monte Carlo fallback entirely absent from documentation | **Partially resolved.** The Design section mentions Monte Carlo. The algorithm steps still do not include it. See issue 8 above. |
