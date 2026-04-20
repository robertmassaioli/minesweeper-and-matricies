#include "sampler.h"

#include <cmath>
#include <random>
#include <vector>

static const double SAMPLER_EPSILON = 1e-9;

// -------------------------------------------------------------------------
// Pivot analysis
// -------------------------------------------------------------------------

struct PivotInfo
{
    // pivotColForRow[r] = column index of the pivot in row r, or -1 if the
    // row is all-zero (should not appear in a snapshot with numRows non-zero rows).
    std::vector<int> pivotColForRow;

    // For each variable column, the row in which it is a pivot (-1 = free).
    std::vector<int> pivotRowForCol;

    // Indices of free-variable columns (no pivot).
    std::vector<int> freeColumns;
};

static PivotInfo analyzePivots(const RrefSnapshot& rref)
{
    PivotInfo info;
    info.pivotColForRow.assign(rref.numRows, -1);
    info.pivotRowForCol.assign(rref.numVars, -1);

    for (int row = 0; row < rref.numRows; ++row)
    {
        for (int col = 0; col < rref.numVars; ++col)
        {
            if (std::fabs(rref.getValue(row, col)) > SAMPLER_EPSILON)
            {
                info.pivotColForRow[row] = col;
                info.pivotRowForCol[col] = row;
                break;
            }
        }
    }

    for (int col = 0; col < rref.numVars; ++col)
    {
        if (info.pivotRowForCol[col] == -1)
            info.freeColumns.push_back(col);
    }

    return info;
}

// -------------------------------------------------------------------------
// Per-row precomputed coefficients for the free variables
//
// In full RREF, every pivot row has zeros in all other pivot columns.
// Therefore the back-substitution for pivot row r simplifies to:
//
//   x[pivotCol[r]] = RHS[r] - sum( coeff[r][j] * x[freeCol[j]] )
//
// Precomputing these coefficients avoids iterating over all numVars columns
// in the inner sampling loop.
// -------------------------------------------------------------------------

struct RowCoeffs
{
    int    pivotCol;
    double rhs;
    // Parallel to freeColumns: coefficient for free variable j in this row.
    std::vector<double> freeCoeffs;
};

static std::vector<RowCoeffs> buildRowCoeffs(const RrefSnapshot&  rref,
                                              const PivotInfo&     pivots)
{
    int numFree = static_cast<int>(pivots.freeColumns.size());
    std::vector<RowCoeffs> rows;
    rows.reserve(rref.numRows);

    for (int row = 0; row < rref.numRows; ++row)
    {
        int pc = pivots.pivotColForRow[row];
        if (pc == -1) continue;

        RowCoeffs rc;
        rc.pivotCol  = pc;
        rc.rhs       = rref.getValue(row, rref.numVars);
        rc.freeCoeffs.resize(numFree);

        for (int j = 0; j < numFree; ++j)
            rc.freeCoeffs[j] = rref.getValue(row, pivots.freeColumns[j]);

        rows.push_back(std::move(rc));
    }

    return rows;
}

// -------------------------------------------------------------------------
// Core sampler
// -------------------------------------------------------------------------

SamplingResult runMonteCarlo(const RrefSnapshot& rref,
                             unsigned            seed,
                             const SamplingConfig& cfg)
{
    SamplingResult result;
    result.probabilities.assign(rref.numVars, 0.0);
    result.accepted      = 0;
    result.attempted     = 0;
    result.acceptanceRate = 0.0;
    result.reliable      = false;

    if (rref.numVars == 0)
        return result;

    PivotInfo            pivots   = analyzePivots(rref);
    std::vector<RowCoeffs> rowData = buildRowCoeffs(rref, pivots);

    int numFree = static_cast<int>(pivots.freeColumns.size());

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> coin(0, 1);

    // Per-variable mine hit counter (integer to avoid fp accumulation error).
    std::vector<int> hitCount(rref.numVars, 0);

    // Working arrays reused each attempt.
    std::vector<int>    freeAssign(numFree);
    std::vector<int>    assignment(rref.numVars, 0);

    while (result.accepted < cfg.targetAccepted &&
           result.attempted < cfg.maxAttempts)
    {
        ++result.attempted;

        // Randomly assign free variables.
        for (int j = 0; j < numFree; ++j)
        {
            freeAssign[j]                      = coin(rng);
            assignment[pivots.freeColumns[j]]  = freeAssign[j];
        }

        // Back-substitute each pivot row to derive its variable's value.
        bool feasible = true;
        for (const RowCoeffs& rc : rowData)
        {
            double val = rc.rhs;
            for (int j = 0; j < numFree; ++j)
                val -= rc.freeCoeffs[j] * freeAssign[j];

            // val must be a binary value — if not, this assignment is infeasible.
            if (val < -SAMPLER_EPSILON || val > 1.0 + SAMPLER_EPSILON)
            {
                feasible = false;
                break;
            }
            assignment[rc.pivotCol] = static_cast<int>(std::round(val));
        }

        if (!feasible) continue;

        // Optional: reject samples that violate the global mine count.
        if (cfg.enforceGlobalCount)
        {
            int sum = 0;
            for (int x : assignment) sum += x;
            if (sum != cfg.totalRemainingMines) continue;
        }

        // Accept: accumulate hit counts.
        ++result.accepted;
        for (int i = 0; i < rref.numVars; ++i)
            hitCount[i] += assignment[i];
    }

    result.reliable       = (result.accepted >= cfg.minAccepted);
    result.acceptanceRate = result.attempted > 0
        ? static_cast<double>(result.accepted) / result.attempted
        : 0.0;

    if (result.accepted > 0)
    {
        for (int i = 0; i < rref.numVars; ++i)
            result.probabilities[i] =
                static_cast<double>(hitCount[i]) / result.accepted;
    }

    return result;
}
