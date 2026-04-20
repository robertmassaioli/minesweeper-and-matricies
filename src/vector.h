// This is an implementation of a Vector ADT
#ifndef __RMVECTORADT
#define __RMVECTORADT

#include <vector>
#include <cstdlib>
#include <cassert>
#include <cmath>

template<class T>
class Vector
{
   public:
      typedef typename std::vector<T>::size_type size_type;

      /**
       * Construct a vector of size 1.
       */
      Vector()
         : values(1)
      {
         values[0] = 0;
      }

      Vector(const Vector& vectorSource)
      {
         for(
               typename std::vector<T>::const_iterator it = vectorSource.values.begin();
               it != vectorSource.values.end();
               ++it
            )
         {
            values.push_back(*it);
         }
      }


      void copy(const Vector<T>* vectorSource)
      {
         values.clear();
         for(size_type i = 0; i < vectorSource->getDimension(); ++i)
         {
            values.push_back(vectorSource->getValue(i));
         }
      }

      void reset(T value)
      {
         for(size_type i = 0; i < getDimension(); ++i)
         {
            values[i] = value;
         }
      }

      size_type getDimension() const
      {
         return values.size();
      }

      void setDimension(size_type dimension)
      {
         values.resize(dimension);
      }

      /**
       * Compare, elementwise, with another vector and return true if every element is 
       * identical.
       */
      bool equal(Vector* vector)
      {
         if(values.size() != vector->values.size()) return false;

         size_type vectorLength = values.size();
         for(size_type i = 0; i < vectorLength; ++i)
         {
            if(values[i] != vector->values[i]) return false;
         }

         return true;
      }

      T getValue(size_type index) const
      {
         return values[index];
      }

      void setValue(size_type index, T value)
      {
         // Automatically resize the vector.
         if(index >= values.size())
         {
            setDimension(index + 1);
         }

         values[index] = value;
      }
      
      /**
       * Add the given vector to the current vector. The current vector will be modified
       * to contain the final result.
       * \param toAdd the vector to add to this vector.
       */
      void add(Vector* toAdd)
      {
         const size_type vectorLength = values.size();
         assert(vectorLength == toAdd->values.size());

         for(size_type i = 0; i < vectorLength; ++i)
         {
            values[i] += toAdd->values[i];
         }
      }

      void multiply(T value)
      {
         const size_type vectorLength = values.size();
         for(size_type i = 0; i < vectorLength; ++i)
         {
            values[i] *= value;
         }
      }

      // Fused multiply-add: this[i] += scale * other[i].
      // Replaces the multiply/add/multiply-back pattern in gaussianEliminate with a
      // single pass, reducing work from 3 × N floating-point ops to 1 × N.
      void scaleAdd(T scale, const Vector<T>* other)
      {
         const size_type vectorLength = values.size();
         assert(vectorLength == other->values.size());
         for(size_type i = 0; i < vectorLength; ++i)
         {
            values[i] += scale * other->values[i];
         }
      }

      double length()
      {
         const size_type vectorLength = values.size();
         double result = 0;

         for (size_type i = 0; i < vectorLength; ++i) {
            result += values[i] * values[i];
         }
         
         return sqrt(result);
      }

      double dot(Vector* b)
      {
         const size_type vectorLength = values.size();
         assert(vectorLength == b->values.size());

	      double dp = 0;
         for (size_type i = 0; i < vectorLength; ++i) {
            dp += values[i] * b->values[i];
         }
	
         return dp;
      }

      Vector* projection(Vector* b)
      {
         Vector* result = new Vector();
         *result = *b;

         double length = b->length();
	      double scalar = dot(b) / (length * length);
         result->multiply(scalar);
	
         return result;
      }

      void round()
      {
         for (size_type i = 0; i < values.size(); ++i) {
            values[i] = ::floor (values[i] + 0.5);
         }
      }

      void ceil()
      {
         for (size_type i = 0; i < values.size(); ++i) {
            values[i] = ::ceil(values[i]);
         }
      }

      void floor()
      {
         for (size_type i = 0; i < values.size(); ++i) {
            values[i] = ::floor(values[i]);
         }
      }

   private:
      std::vector<T> values;
};

/*
double dotProduct (Vector x, Vector y);

// the projection of a onto b
Vector projection (Vector a, Vector b);
*/

#endif
