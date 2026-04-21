#include "game.h"
#include "solver.h"
#include "matrix.h"
#include "sampler.h"

#include "logging.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

static const double EPSILON = 1e-9;

using namespace std;

static int adjMap[8][2] = {
   {-1,-1},
   {0,-1},
   {1,-1},
   {1, 0},
   {1, 1},
   {0, 1},
   {-1,1},
   {-1,0}
};

solver::solver(GuessingStrategy strat) : strategy(strat), turnCount(0) {}

std::unique_ptr<std::vector<Move>> solver::getMoves(Board* board, logger* log)
{
   ++turnCount;
   (*log) << std::string(60, '=') << logger::endl;
   (*log) << " SOLVER TURN " << turnCount << logger::endl;
   (*log) << std::string(60, '=') << logger::endl;

   Square* grid = board->getGrid();

   // 1 List number squares that touch non-clicked squares
   Dimensions gridDim = board->getDimensions();
   const int boardWidth = gridDim.getWidth();

   // coordStr: flat board index → "(col,row)" string
   auto coordStr = [boardWidth](int flatPos) -> std::string {
      return "(" + std::to_string(flatPos % boardWidth) + ","
                 + std::to_string(flatPos / boardWidth) + ")";
   };

   std::vector<Position> nonCompletedPositions;
   for(int row = 0; row < gridDim.getHeight(); ++row)
   {
      for(int col = 0; col < gridDim.getWidth(); ++col)
      {
         Square* currentSquare = grid + board->locPos(col, row);
         if(currentSquare->state == CLICKED)
         {
            if((currentSquare->value > EMPTY) && (currentSquare->value < MINE)) // convert to helper function
            {
               bool unclickedFound = false;
               for(int i = 0; i < 8 && !unclickedFound; ++i)
               {
                  Position tempPos(col + adjMap[i][0], row + adjMap[i][1]);
                  if(board->isValidPos(tempPos))
                  {
                     int position  = board->locPos(tempPos);
                     if(grid[position].state == NOT_CLICKED)
                     {
                        unclickedFound = true;
                     }
                  }
               }

               // If the square is not completely flagged
               if(unclickedFound)
               {
                  nonCompletedPositions.push_back(Position(col, row));
               }
            }
         }
      }
   }

   // 2 Get all of the adjacent squares that have not been clicked and identify them.
   // positionToId is a flat array indexed by board position (no hash overhead).
   // -1 means "not yet assigned an id".
   int currentSquareId = 0;
   const int totalBoardSquares = gridDim.getWidth() * gridDim.getHeight();
   std::vector<int> positionToId(totalBoardSquares, -1);
   std::vector<int> idToPosition;
   for(const Position& pos : nonCompletedPositions)
   {
      for(int i = 0; i < 8; ++i)
      {
         Position tempPos(pos.getX() + adjMap[i][0], pos.getY() + adjMap[i][1]);
         if(board->isValidPos(tempPos))
         {
            int position = board->locPos(tempPos);
            if(grid[position].state == NOT_CLICKED)
            {
               if(positionToId[position] == -1)
               {
                  positionToId[position] = currentSquareId;
                  idToPosition.push_back(position);
                  currentSquareId++;
               }
            }
         }
      }
   }

   (*log) << "Non flagged positions: " << nonCompletedPositions.size() << logger::endl;
   (*log) << "Total Squares: " << currentSquareId << logger::endl;

   if(nonCompletedPositions.empty() || currentSquareId == 0)
   {
      return nullptr;
   }

   // varStr: variable id → "id=N pos=(col,row)" string
   auto varStr = [&](int id) -> std::string {
      return "id=" + std::to_string(id) + " pos=" + coordStr(idToPosition[id]);
   };

   // 3 Build the constraint matrix.
   // Each revealed number square contributes one row: sum(unknown_neighbours) = number - flagged_neighbours.
   // Columns correspond to unknown squares (variables); the final column is the RHS constant.
   int totalSquares = currentSquareId;
   matrix<double> solMat;
   Vector<double> tempRow;
   tempRow.setDimension(totalSquares + 1);
   for(const Position& pos : nonCompletedPositions)
   {
      int position = board->locPos(pos.getX(), pos.getY());

      tempRow.reset(0.0);
      tempRow.setValue(totalSquares, grid[position].value);
      for(int i = 0; i < 8; ++i)
      {
         Position adjacent(pos.getX() + adjMap[i][0], pos.getY() + adjMap[i][1]);
         if(board->isValidPos(adjacent))
         {
            int adjacentPosition = board->locPos(adjacent);
            if(grid[adjacentPosition].state == NOT_CLICKED)
            {
               int matrixColumn = positionToId[adjacentPosition];
               tempRow.setValue(matrixColumn, 1.0);
            }
            else if (grid[adjacentPosition].state == FLAG_CLICKED)
            {
               // Already-flagged neighbours are confirmed mines: subtract them from the
               // RHS so the equation counts only the remaining unknown neighbours.
               tempRow.setValue(totalSquares, tempRow.getValue(totalSquares) - 1);
            }
         }
      }

      solMat.addRow(&tempRow);
   }

   // 4 Gaussian Eliminate the Matrix
   solMat.render(log);
   solMat.gaussianEliminate();
   solMat.render(log);

   // 5 Read the RREF matrix back and classify each unknown square as mine, safe, or
   // indeterminate. Two strategies are applied per row:
   //   A) Direct solve: if all other variables in the row were already resolved, the
   //      row reduces to a single equation "x = 0 or 1" that can be read off directly.
   //   B) Min/max lemma: each variable is binary (0 or 1). If RHS == sum of all positive
   //      coefficients (max), every positive-coefficient variable must be 1 (mine). If
   //      RHS == sum of all negative coefficients (min), they must all be 0 (safe).

   // Scan from the bottom to find the last row that contains any data; RREF pushes all-zero
   // rows to the bottom so everything below firstNonZeroRow can be ignored.
   matrix<double>::width_size_type matrixWidth = solMat.getWidth();
   matrix<double>::height_size_type matrixHeight = solMat.getHeight();
   matrix<double>::height_size_type firstNonZeroRow = 0;
   // height_size_type is unsigned: decrementing past 0 wraps to a huge value, making
   // row < matrixHeight false and terminating the loop safely.
   for(matrix<double>::height_size_type row = matrixHeight - 1; row >= 0 && row < matrixHeight; --row)
   {
      Vector<double>* currentRow = solMat.getRow(row);
      bool foundNonZero = false;
      for(Vector<double>::size_type col = 0; col < matrixWidth && !foundNonZero; ++col)
      {
         foundNonZero |= currentRow->getValue(col) != 0;
      }
      if(foundNonZero)
      {
         firstNonZeroRow = row;
         break;
      }
   }

   // results[i] holds the resolved value for variable i: true = mine, false = safe, empty = unknown.
   vector<std::optional<bool>> results;
   results.resize(matrixWidth - 1);

   // Process rows from the last non-zero row up to row 0. The unsigned loop condition mirrors
   // the one above: decrementing past 0 wraps, making row <= firstNonZeroRow false.
   matrix<double>::width_size_type maxVariableColumn = matrixWidth - 1;
   for(matrix<double>::height_size_type row = firstNonZeroRow; row >= 0 && row <= firstNonZeroRow; --row)
   {
      // Find the pivot: the first non-zero coefficient in this row. In RREF each pivot column
      // has been normalised to 1; if column `row` is zero a free variable shifted the pivot right.
      bool hasUnresolvedVars = false;
      matrix<double>::height_size_type pivot = row;
      double pivotVal = solMat.getValue(row, pivot);
      double val = solMat.getValue(row, maxVariableColumn);

      // Back-substitute: for every already-resolved variable in this row, move its known
      // contribution to the RHS so only the unknown pivot (and any other unknowns) remain.
      for(matrix<double>::width_size_type col = row + 1; col < maxVariableColumn; ++col)
      {
         double currentValue = solMat.getValue(row, col);

         // If the diagonal entry was zero, slide right to the next non-zero entry as the pivot.
         if(fabs(pivotVal) < EPSILON && fabs(currentValue) > EPSILON)
         {
            pivot = col;
            pivotVal = currentValue;
            (*log) << "[RREF] Row " << row << ": pivot at " << varStr(pivot)
                   << ", coefficient=" << currentValue << logger::endl;
         }

         if(fabs(currentValue) > EPSILON)
         {
            if(results[col].has_value())
            {
               // Variable already resolved: substitute its value into this row's RHS.
               val -= currentValue * (results[col].value() ? 1.0 : 0.0);
               solMat.setValue(row, col, 0.0);
            }
            else
            {
               // At least one variable in this row is still unknown; direct solve is impossible.
               hasUnresolvedVars = true;
            }
         }
      }
      if(hasUnresolvedVars)
      {
         (*log) << "[RREF] Row " << row << ": has unresolved variables, trying min/max lemma"
                << logger::endl;
      }
      solMat.setValue(row, maxVariableColumn, val);

      if(fabs(pivotVal) > EPSILON)
      {
         if(hasUnresolvedVars)
         {
            // Direct solve failed. Try the min/max lemma instead.
            // Compute the minimum and maximum possible values for the LHS given that each
            // variable is binary: max = sum of positive coefficients, min = sum of negatives.
            double minValue = 0;
            double maxValue = 0;

            for(matrix<double>::width_size_type col = row; col < maxVariableColumn; ++col)
            {
               double currentValue = solMat.getValue(row, col);
               if(currentValue > EPSILON) maxValue += currentValue;
               if(currentValue < -EPSILON) minValue += currentValue;
            }

            if(fabs(val - minValue) < EPSILON)
            {
               // RHS equals the minimum possible sum, so every positive-coefficient variable
               // must be 0 (safe) and every negative-coefficient variable must be 1 (mine).
               for(matrix<double>::width_size_type col = row; col < maxVariableColumn; ++col)
               {
                  double currentValue = solMat.getValue(row, col);
                  if(currentValue > EPSILON)
                  {
                     (*log) << "[Deduced] " << varStr(col) << ": SAFE (min/max lemma)"
                            << logger::endl;
                     results[col] = std::optional<bool>(false);
                  }
                  if(currentValue < -EPSILON)
                  {
                     (*log) << "[Deduced] " << varStr(col) << ": MINE (min/max lemma)"
                            << logger::endl;
                     results[col] = std::optional<bool>(true);
                  }
               }
            }
            else if (fabs(val - maxValue) < EPSILON)
            {
               // RHS equals the maximum possible sum, so every positive-coefficient variable
               // must be 1 (mine) and every negative-coefficient variable must be 0 (safe).
               for(matrix<double>::width_size_type col = row; col < maxVariableColumn; ++col)
               {
                  double currentValue = solMat.getValue(row, col);
                  if(currentValue > EPSILON)
                  {
                     (*log) << "[Deduced] " << varStr(col) << ": MINE (min/max lemma)"
                            << logger::endl;
                     results[col] = std::optional<bool>(true);
                  }
                  if(currentValue < -EPSILON)
                  {
                     (*log) << "[Deduced] " << varStr(col) << ": SAFE (min/max lemma)"
                            << logger::endl;
                     results[col] = std::optional<bool>(false);
                  }
               }
            }
            // If val is strictly between min and max the constraint is underdetermined;
            // we cannot classify any variable in this row without guessing.
         }
         else
         {
            // All other variables were substituted out. After RREF the pivot coefficient is 1,
            // so the row now reads "1 * x = val", meaning x = val. Since x is a binary mine
            // indicator, val must be 0 (safe) or 1 (mine) on any valid board.
            if(results[pivot].has_value())
            {
               (*log) << "[RREF] " << varStr(pivot) << " already resolved, skipping"
                      << logger::endl;
            }
            else
            {
               if(fabs(val) < EPSILON || fabs(val - 1.0) < EPSILON)
               {
                  bool isMine = fabs(val - 1.0) < EPSILON;
                  (*log) << "[Deduced] " << varStr(pivot) << ": "
                         << (isMine ? "MINE" : "SAFE") << " (direct solve, val="
                         << val << ")" << logger::endl;
                  results[pivot] = optional<bool>(isMine);
               }
               else
               {
                  (*log) << "[WARN] " << varStr(pivot)
                         << " direct-solve value=" << val
                         << " is not 0 or 1 (numerical error?)" << logger::endl;
               }
            }
         }
      }
      else
      {
         (*log) << "[RREF] Row " << row << " has no pivot (all-zero row)" << logger::endl;
      }
   }

   // print out results
   auto moves = std::make_unique<std::vector<Move>>();
   (*log) << logger::endl << "Results:" << logger::endl;
   for(matrix<double>::width_size_type i = 0; i < matrixWidth - 1; ++i)
   {
      (*log) << "  [Result] " << varStr(i) << ": ";
      if(results[i].has_value())
      {
         if(results[i].value())
         {
            (*log) << "MINE";
            moves->push_back(Move(board->posLoc(idToPosition[(int) i]), FLAG));
         }
         else
         {
            (*log) << "SAFE";
            moves->push_back(Move(board->posLoc(idToPosition[(int) i]), NORMAL));
         }
      }
      else
      {
         (*log) << "UNDECIDED";
      }
      (*log) << logger::endl;
   }

   // -------------------------------------------------------------------
   // Probabilistic fallback: if no certain moves were found, run the
   // Monte Carlo sampler to estimate mine probabilities and click the
   // least dangerous unknown square.
   // -------------------------------------------------------------------
   if (moves->empty() && strategy == GuessingStrategy::MONTE_CARLO)
   {
      // Scan the board for: flagged squares (to compute remaining mines)
      // and unconstrained unknowns (NOT_CLICKED, not in any constraint row).
      int flagCount = 0;
      std::vector<int> unconstrainedPositions;
      for (int pos = 0; pos < totalBoardSquares; ++pos)
      {
         if (grid[pos].state == FLAG_CLICKED)
         {
            ++flagCount;
         }
         else if (grid[pos].state == NOT_CLICKED && positionToId[pos] == -1)
         {
            unconstrainedPositions.push_back(pos);
         }
      }

      int remainingMines = board->getMineCount() - flagCount;

      // Build a plain snapshot of the post-RREF constraint matrix.
      RrefSnapshot snap;
      snap.numVars = static_cast<int>(matrixWidth - 1);
      snap.numRows = static_cast<int>(firstNonZeroRow + 1);
      snap.data.resize(snap.numRows * (snap.numVars + 1));
      for (int r = 0; r < snap.numRows; ++r)
         for (int c = 0; c <= snap.numVars; ++c)
            snap.data[r * (snap.numVars + 1) + c] = solMat.getValue(r, c);

      // Enable global mine count enforcement only when the constrained frontier
      // covers most of the remaining unknowns (unconstrained region is small or
      // empty), so the constraint is useful rather than just a rejection filter.
      int totalUnknowns = snap.numVars + static_cast<int>(unconstrainedPositions.size());
      SamplingConfig cfg;
      cfg.enforceGlobalCount  = (unconstrainedPositions.empty() ||
                                  remainingMines <= snap.numVars);
      cfg.totalRemainingMines = remainingMines;

      SamplingResult sr = runMonteCarlo(snap,
                                        static_cast<unsigned>(rand()),
                                        cfg);

      (*log) << "MC sampler: " << sr.accepted << "/" << sr.attempted
             << " accepted (" << sr.acceptanceRate * 100.0 << "%) reliable="
             << (sr.reliable ? "yes" : "no") << logger::endl;

      if (sr.reliable)
      {
         // Find the constrained variable with the lowest mine probability.
         int    bestVar  = -1;
         double bestProb = 2.0;
         for (int i = 0; i < snap.numVars; ++i)
         {
            if (sr.probabilities[i] < bestProb)
            {
               bestProb = sr.probabilities[i];
               bestVar  = i;
            }
         }

         // Estimate probability for the unconstrained region.
         double expectedConstrainedMines = 0.0;
         for (int i = 0; i < snap.numVars; ++i)
            expectedConstrainedMines += sr.probabilities[i];
         double remainingForUnconstrained =
            std::max(0.0, remainingMines - expectedConstrainedMines);
         double unconstrainedProb =
            unconstrainedPositions.empty()
               ? 2.0
               : remainingForUnconstrained / unconstrainedPositions.size();

         (*log) << "MC best constrained prob=" << bestProb
                << " unconstrained prob=" << unconstrainedProb << logger::endl;

         if (!unconstrainedPositions.empty() && unconstrainedProb < bestProb)
         {
            // All unconstrained squares are exchangeable — pick the first.
            moves->push_back(
               Move(board->posLoc(unconstrainedPositions[0]), NORMAL));
         }
         else if (bestVar >= 0)
         {
            moves->push_back(
               Move(board->posLoc(idToPosition[bestVar]), NORMAL));
         }
      }
      else
      {
         // Acceptance rate was too low for reliable estimates.  Fall back to
         // the least-constrained variable: the one with the fewest non-zero
         // entries in the RREF (fewest constraints bearing on it).
         (*log) << "MC sampler unreliable — using least-constrained fallback"
                << logger::endl;

         int bestVar        = -1;
         int fewestNonZeros = INT_MAX;
         for (int col = 0; col < snap.numVars; ++col)
         {
            int count = 0;
            for (int r = 0; r < snap.numRows; ++r)
               count += (std::fabs(snap.getValue(r, col)) > EPSILON) ? 1 : 0;
            if (count < fewestNonZeros)
            {
               fewestNonZeros = count;
               bestVar        = col;
            }
         }

         if (!unconstrainedPositions.empty())
         {
            moves->push_back(
               Move(board->posLoc(unconstrainedPositions[0]), NORMAL));
         }
         else if (bestVar >= 0)
         {
            moves->push_back(
               Move(board->posLoc(idToPosition[bestVar]), NORMAL));
         }
      }
   }

   // Turn summary
   {
      int mineCount = 0, safeCount = 0;
      for (Move m : *moves)
         m.getClickType() == FLAG ? ++mineCount : ++safeCount;
      (*log) << logger::endl;
      (*log) << std::string(60, '-') << logger::endl;
      (*log) << " Turn " << turnCount << " summary: "
             << mineCount << " mine(s) flagged, "
             << safeCount << " safe square(s) clicked, "
             << (mineCount + safeCount) << " total move(s)" << logger::endl;
      (*log) << std::string(60, '-') << logger::endl;
   }

   return moves;
}
