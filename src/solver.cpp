#include "game.h"
#include "solver.h"
#include "matrix.h"

#include "logging.h"
#include <cmath>
#include <optional>
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

std::unique_ptr<std::vector<Move>> solver::getMoves(Board* board, logger* log)
{
   Square* grid = board->getGrid();

   // 1 List number squares that touch non-clicked squares
   Dimensions gridDim = board->getDimensions();
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

   // print out every element in the id map
   (*log) << "Id: Position" << logger::endl;
   for(int id = 0; id < currentSquareId; ++id)
   {
      (*log) << id << ": " << idToPosition[id] << logger::endl;
   }
   (*log) << logger::endl;

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
      bool failedToFindValue = false;
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
            (*log) << "Pivot updated to: " << pivot << " => " << currentValue << logger::endl;
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
               failedToFindValue = true;
            }
         }
      }
      (*log) << "value: " << val << " (" << (failedToFindValue ? "true" : "false") << ")" << logger::endl;
      solMat.setValue(row, maxVariableColumn, val);

      if(fabs(pivotVal) > EPSILON)
      {
         if(failedToFindValue)
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
                     (*log) << "Col " << col << " is not a mine." << logger::endl;
                     results[col] = std::optional<bool>(false);
                  }
                  if(currentValue < -EPSILON)
                  {
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
                     (*log) << "Col " << col << " is a mine." << logger::endl;
                     results[col] = std::optional<bool>(true);
                  }
                  if(currentValue < -EPSILON)
                  {
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
               (*log) << "Already found pivot for: " << pivot << logger::endl;
            }
            else
            {
               (*log) << "Found standard result: " << pivot << " => " << val << logger::endl;
               if(fabs(val) < EPSILON || fabs(val - 1.0) < EPSILON)
               {
                  results[pivot] = optional<bool>(fabs(val - 1.0) < EPSILON);
               }
               else
               {
                  (*log) << "Found pivot value is not 0 or 1 ... what do we do?" << logger::endl;
               }
            }
         }
      }
      else
      {
         (*log) << "There in now pivot in row: " << row << logger::endl;
      }
   }

   // print out results
   auto moves = std::make_unique<std::vector<Move>>();
   for(matrix<double>::width_size_type i = 0; i < matrixWidth - 1; ++i)
   {
      (*log) << i << ": ";
      if(results[i].has_value())
      {
         if(results[i].value())
         {
            (*log) << "mine";
            moves->push_back(Move(board->posLoc(idToPosition[(int) i]), FLAG));
         }
         else
         {
            (*log) << "not mine";
            moves->push_back(Move(board->posLoc(idToPosition[(int) i]), NORMAL));
         }
      }
      else
      {
         (*log) << "NA";
      }
      (*log) << logger::endl;
   }

   return moves;
}
