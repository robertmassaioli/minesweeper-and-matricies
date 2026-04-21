#include <chrono>
#include <functional>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include "game.h"
#include "solver.h"
#include "config.h"
#include "logging.h"
#include "matrix.h"

// ---------------------------------------------------------------------------
// Minimal benchmark harness
// ---------------------------------------------------------------------------

struct BenchResult
{
    std::string name;
    int         iterations;
    double      totalSecs;
    double      meanMicros;
    double      itersPerSec;
};

// Runs `fn` repeatedly for at least `minSecs` seconds (after a short warmup)
// and returns timing statistics.
static BenchResult runBench(const std::string& name,
                            std::function<void()> fn,
                            double minSecs = 2.0)
{
    // Warmup: 5 iterations to bring code and data into cache.
    for (int i = 0; i < 5; ++i) fn();

    auto start = std::chrono::high_resolution_clock::now();
    int iters = 0;
    while (true)
    {
        fn();
        ++iters;
        auto now = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(now - start).count();
        if (elapsed >= minSecs) break;
    }
    auto end = std::chrono::high_resolution_clock::now();
    double total = std::chrono::duration<double>(end - start).count();

    BenchResult r;
    r.name        = name;
    r.iterations  = iters;
    r.totalSecs   = total;
    r.meanMicros  = (total / iters) * 1e6;
    r.itersPerSec = iters / total;
    return r;
}

static void printResult(const BenchResult& r)
{
    std::cout << std::left  << std::setw(42) << r.name
              << std::right << std::setw(10) << r.iterations << " iters  "
              << std::fixed << std::setprecision(2)
              << std::setw(10) << r.meanMicros  << " us/iter  "
              << std::setw(12) << r.itersPerSec << " iters/s\n";
}

// ---------------------------------------------------------------------------
// Benchmark 1: Full game pipeline
//
// Measures the end-to-end cost of one complete Minesweeper game: board
// generation, the iterative solve loop (solver::getMoves called until the
// game ends or the solver stalls), and result collection.
//
// This is the primary macro-benchmark; improvements to any part of the
// pipeline (board generation, solver, matrix) will show up here.
// ---------------------------------------------------------------------------

static void benchFullGame()
{
    Dimensions dim(16, 16);
    const int mines = 40;
    nop_logger log;

    Game game(dim, mines, &log);

    // Random first click.
    {
        Move first(Position(rand() % dim.getWidth(), rand() % dim.getHeight()), NORMAL);
        game.acceptMove(first);
    }

    solver s(GuessingStrategy::NONE);
    std::unique_ptr<std::vector<Move>> moves;
    do
    {
        moves = s.getMoves(game.getBoard(), &log);
        if (moves)
        {
            for (const Move& m : *moves)
            {
                Move mv = m;
                game.acceptMove(mv);
            }
        }
    } while (game.getState() == PROGRESS && moves && !moves->empty());
}

// ---------------------------------------------------------------------------
// Benchmark 2: Solver turn in isolation
//
// Measures a single call to solver::getMoves on a board that has already
// had its first click applied.  The board is generated once in setup and
// reused across iterations (the solver does not modify the board — only the
// caller does when it applies moves), so this isolates the cost of matrix
// construction + Gaussian elimination + back-substitution without the
// noise of board generation or repeated move application.
// ---------------------------------------------------------------------------

static void benchSolverTurn()
{
    static Dimensions dim(16, 16);
    static Game* game = nullptr;
    static nop_logger log;

    // One-time setup: generate a board and make the first click.
    if (!game)
    {
        srand(42);
        game = new Game(dim, 40, &log);
        Move first(Position(8, 8), NORMAL);
        game->acceptMove(first);
    }

    solver s(GuessingStrategy::NONE);
    // Call getMoves once on the same initial board state.  We intentionally
    // do NOT apply the returned moves so the board stays identical across
    // iterations, giving a stable, repeatable measurement.
    s.getMoves(game->getBoard(), &log);
}

// ---------------------------------------------------------------------------
// Benchmark 3: Gaussian elimination on a fixed matrix
//
// Measures just gaussianEliminate() on a realistic 22-row × 18-column
// constraint matrix taken directly from the testmatrix.cpp test suite.
// This isolates the linear algebra core from all game and solver overhead,
// making it the right benchmark to use when tuning matrix storage layout
// or the elimination algorithm itself.
// ---------------------------------------------------------------------------

static void benchGaussianEliminate()
{
    static const int array_matrix[22][19] = {
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,0,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,2},
        {0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,2},
        {0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,1},
        {0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,1,1,0,0,0,0,1,0,0,0,0,1,1,1,0,0,2},
        {0,0,1,0,1,0,0,0,0,0,0,0,0,1,1,0,1,0,3},
        {0,0,0,0,1,0,0,0,0,1,1,0,0,1,0,0,1,1,5},
        {0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,2},
        {0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,1},
        {0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,0,0,1}
    };

    // Rebuild and eliminate a fresh copy each iteration so the measurement
    // reflects the full cost of both construction and elimination.
    matrix<double> m;
    Vector<double> tempRow;
    for (int row = 0; row < 22; ++row)
    {
        tempRow.reset(0.0);
        for (int col = 0; col < 19; ++col)
            tempRow.setValue(col, array_matrix[row][col]);
        m.addRow(&tempRow);
    }

    m.gaussianEliminate();
}

// ---------------------------------------------------------------------------

int main()
{
    srand(12345);

    std::cout << "\nMinesweeper solver benchmarks (2 seconds per bench)\n";
    std::cout << std::string(80, '-') << "\n";
    std::cout << std::left  << std::setw(42) << "Benchmark"
              << std::right << std::setw(10) << "Iters"
              << "           us/iter      iters/sec\n";
    std::cout << std::string(80, '-') << "\n";

    printResult(runBench("Full game (16x16, 40 mines)",  benchFullGame));
    printResult(runBench("Solver turn (first move only)", benchSolverTurn));
    printResult(runBench("gaussianEliminate (22x19)",    benchGaussianEliminate));

    std::cout << std::string(80, '-') << "\n";
    return 0;
}
