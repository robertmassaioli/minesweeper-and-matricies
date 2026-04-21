// An implementation of a matrix ADT
// The matrix should be able to be created, deleted
// Have rows and columns added
// Be gaussian eliminated

#ifndef __RMMATRIXADT
#define __RMMATRIXADT

#include "logging.h"
#include "vector.h"
#include <cmath>
#include <memory>

template<class A>
class matrix
{
   public:
      typedef typename std::vector<Vector<A>* >::size_type  height_size_type;
      typedef typename std::vector<A>::size_type            width_size_type;

      enum solution
      {
         INFINITE_SOLUTIONS,
         NO_SOLUTIONS,
         UNIQUE_SOLUTION
      };

      matrix()
      {
      }

      ~matrix()
      {
         clear();
      }

      matrix(const matrix& matrixSource)
      {
         for(height_size_type i = 0; i < matrixSource.getHeight(); ++i)
         {
            addRow(matrixSource.getRow(i));
         }
      }

      void copy(const matrix* matrixSource)
      {
         for(height_size_type i = 0; i < matrixSource->getHeight(); ++i)
         {
            addRow(matrixSource->getRow(i));
         }
      }

      void transpose()
      {
         matrix<A> temporary = *this;
         height_size_type height = temporary.getHeight();
         clear();

         for(height_size_type i = 0; i < height; ++i)
         {
            addColumn(temporary.getRow(i));
         }
      }

      /**
       * The addRow function adds the row directly to the matrix and copies it in. 
       * It does not use it directly.
       */
      void addRow(Vector<A>* row)
      {
         assert(row != NULL);

         auto newRow = std::make_unique<Vector<A>>();
         newRow->copy(row);

         rows.push_back(std::move(newRow));
      }

      Vector<A>* getRow(height_size_type rowIndex) const
      {
         if(rowIndex >= rows.size()) return nullptr;
         return rows[rowIndex].get();
      }

      /**
       * This function deletes a specific row from the vector.
       */
      void deleteRow(int rowIndex)
      {
         rows.erase(rows.begin() + rowIndex);
      }

      void addColumn(Vector<A>* vector)
      {
         assert (vector != NULL);

         if (rows.empty()) {
            height_size_type newHeight = vector->getDimension();
            rows.resize(newHeight);
            for (height_size_type i = 0; i < newHeight; i++) {
               rows[i] = std::make_unique<Vector<A>>();
               rows[i]->setValue(0, vector->getValue(i));
            }
         } else {
            height_size_type matrixHeight = getHeight();
            assert (vector->getDimension() == matrixHeight);
            
            width_size_type rowLen = getWidth();
            for (height_size_type i = 0; i < matrixHeight; ++i) {
               getRow(i)->setValue(rowLen, vector->getValue(i));
            }
         }
      }

      void clear()
      {
         rows.clear();
      }

      A getValue(height_size_type row, width_size_type col)
      {
         return rows[row]->getValue(col);
      }

      void setValue(height_size_type row, width_size_type col, A value)
      {
         rows[row]->setValue(col, value);
      }

      /*

         Wikipedia Sourcecode for Gaussian Elimination

      i := 1
      j := 1
      while (i ≤ m and j ≤ n) do
        Find pivot in column j, starting in row i:
        maxi := i
        for k := i+1 to m do
          if abs(A[k,j]) > abs(A[maxi,j]) then
            maxi := k
          end if
        end for
        if A[maxi,j] ≠ 0 then
          swap rows i and maxi, but do not change the value of i
          Now A[i,j] will contain the old value of A[maxi,j].
          divide each entry in row i by A[i,j]
          Now A[i,j] will have the value 1.
          for u := i+1 to m do
            subtract A[u,j] * row i from row u
            Now A[u,j] will be 0, since A[u,j] - A[i,j] * A[u,j] = A[u,j] - 1 * A[u,j] = 0.
          end for
          i := i + 1
        end if
        j := j + 1
      end while

      Roberts Notes:

      Keep in mind that in A[c, d] that:

       A is the matrix
       c is the ROW
       d is the COLUMN

      This is odd because it is in (Y, X) order rather than the usual co-ordinate geometry [X, Y]

      */
      void gaussianEliminate()
      {
         if(rows.empty()) return;

         // n => totalColumns
         int totalColumns = getWidth();
         // m => totalRows
         int totalRows = getHeight();

         // i => row
         int row = 0;
         // j => col
         int col = 0;
         while ((row < totalRows) && (col < totalColumns)) {
            int max_row = row;
            // k => currentRow
            // (*log) << "Current row: " << col << logging::endl;
            for (int currentRow = row + 1; currentRow < totalRows; ++currentRow) {
               if (abs(getValue(currentRow, col)) > abs(getValue(max_row, col))) {
                  max_row = currentRow;
               }
            }
            
            if (abs(getValue(max_row, col)) > A(1e-9))
            {
               // Swap the rows around. Put it in its own scope to remove the temp
               // variable asap.
               if(row != max_row)
               {
                  std::swap(rows[row], rows[max_row]);
               }

               A currentValue = getValue(row, col);
               rows[row]->multiply(A(1.0) / currentValue);

               // Full RREF: eliminate this pivot column from every other row, not just
               // those below. This ensures that deductions derivable by subtracting one
               // constraint from another are made automatically rather than requiring a
               // separate post-processing pass.
               // scaleAdd fuses the multiply/add/divide-back into one pass over the row.
               for(int iterRow = 0; iterRow < totalRows; ++iterRow)
               {
                  if(iterRow == row) continue;
                  A mulVal = -getValue(iterRow, col);
                  if(abs(mulVal) > A(1e-9))
                  {
                     rows[iterRow]->scaleAdd(mulVal, rows[row].get());
                  }
               }

               row++;
            }

            col++;
         }
      }

      /**
       * Tries to solve the matrix and returns type of success.
       *
       * FAIL return NULL and result is INFINITE_SOLUTIONS or NO_SOLUTIONS.
       * SUCCESS return Vector solution and result is UNIQUE_SOLUTION
       */
      std::unique_ptr<Vector<A>> solve(matrix::solution* result)
      {
         int matrixHeight = getHeight();
         int matrixWidth = getWidth();
         assert (!rows.empty());						// make sure that the matrix has numbers in it
         assert (matrixWidth > 1);	            // and that is has enough numbers to be solvable
         
         *result = INFINITE_SOLUTIONS;
         
         // Run gaussian elimination as the first step.
         gaussianEliminate();

         // I think that after you gaussian eliminate then and identical rows will have one brought to zero, 
         // delete them for this to make handling that case better (also, the algorithm will push those rows to the bottom)
         bool shouldDeleteRow = true;
         for(int row = matrixHeight - 1; row >= 0 && shouldDeleteRow; --row)
         {
            for (int col = 0; col < matrixWidth && shouldDeleteRow; ++col) 
            {
               if (abs(getValue(row, col)) > A(1e-9)) {
                  shouldDeleteRow = false;
                  if (col == matrixWidth - 1) {
                     *result = NO_SOLUTIONS;
                     return NULL;
                  }
               }
            }
            
            if (shouldDeleteRow) 
            {
               deleteRow(row);
            }
         }	
               
         // for every row
         //		if every item in that row is zero then delete the row (and if any row only has the last row with an item then NO_SOLUTIONS)
         std::unique_ptr<Vector<A>> solution;
         matrixHeight = getHeight(); // the height may have changed from above
         if (matrixHeight == matrixWidth - 1) {
            int maxVar = matrixWidth - 1;
            solution = std::make_unique<Vector<A>>();
            solution->setDimension(maxVar);
         
            for (int row = maxVar - 1; row >= 0; --row) 
            {
               A var = getValue(row, maxVar);
            
               for (int col = row + 1; col < maxVar; ++col) {
                  var -= solution->getValue(col) * getValue(row, col);
               }
            
               //(*log) << "Row " << row << " value is " << var << logging::endl;
               solution->setValue(row, var);
            }
         
            *result = UNIQUE_SOLUTION;
         }
         
         return solution;
      }

      std::unique_ptr<matrix<A>> multiply(matrix<A>* other)
      {
         auto result = std::make_unique<matrix<A>>();

         other->transpose();
         
         int matrixHeight = getHeight();
         int matrixWidth = getWidth();
         for (int row = 0; row < matrixHeight; ++row) {
            Vector<A> temp;
            for (int col = 0; col < matrixWidth; ++col) {
               temp.setValue(col, getRow(row).dot(other->getRow(col)));
            }
            result->addRow(&temp);
         }
         
         other->transpose();
         
         return result;
      }

      height_size_type getHeight() const
      {
         return rows.size();
      }

      width_size_type getWidth() const
      {
         if(rows.empty()) return 0;
         return rows[0]->getDimension();
      }

      void render(logger* log)
      {
         for(height_size_type row = 0; row < getHeight(); ++row)
         {
            if(row < 10)
            {
               (*log) << '0';
            }

            (*log) << row << "| ";

            for(width_size_type col = 0; col < getWidth(); ++col)
            {
               A value = getValue(row, col);
               if(std::fabs(static_cast<double>(value)) < 1e-9)
               {
                  (*log) << "0 ";
               }
               else
               {
                  (*log) << value << ' ';
               }
            }

            (*log) << logger::endl;
         }

         (*log) << logger::endl;
      }

   private:
      std::vector<std::unique_ptr<Vector<A>>> rows;

      A abs(A x)
      {
         if(x < A(0.0)) return -x;
         return x;
      }
};

#endif
