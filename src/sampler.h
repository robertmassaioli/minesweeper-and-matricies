#ifndef MNM_SAMPLER_H
#define MNM_SAMPLER_H

#include <vector>

// A plain-data snapshot of the post-RREF constraint matrix.
// Decouples the sampler from matrix<double> and all game types.
struct RrefSnapshot
{
    int numVars;   // number of variable columns (width - 1)
    int numRows;   // number of non-zero rows
    // Row-major storage: element at (row, col) is data[row * (numVars+1) + col].
    // Column numVars is the RHS.
    std::vector<double> data;

    double getValue(int row, int col) const
    {
        return data[row * (numVars + 1) + col];
    }
};

struct SamplingConfig
{
    // Stop once this many valid samples have been accepted.
    int targetAccepted      = 200;
    // Absolute ceiling on total sampling attempts regardless of accepted count.
    int maxAttempts         = 10000;
    // Minimum accepted samples required for the result to be considered reliable.
    int minAccepted         = 30;
    // When true, also reject samples whose total mine count != totalRemainingMines.
    bool enforceGlobalCount = false;
    int totalRemainingMines = 0;
};

struct SamplingResult
{
    // Estimated P(mine) for each constrained variable, indexed by variable id.
    std::vector<double> probabilities;
    int accepted;
    int attempted;
    double acceptanceRate;
    // False if accepted < minAccepted; caller should treat the result as unreliable.
    bool reliable;
};

// Run the Monte Carlo sampler. Thread-safe: all state is local; no globals are read or written.
SamplingResult runMonteCarlo(const RrefSnapshot& rref,
                             unsigned seed,
                             const SamplingConfig& cfg);

#endif
