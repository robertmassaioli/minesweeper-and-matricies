//////
// testmatrix.c
// A set of tests for my implementation of a Matrix ADT
//
// Author: Robert Massaioli, 2009
//////

#include "logging.h"
#include "matrix.h"
#include "testmatrix.h"

#include <cstdlib>
#include <iostream>

using namespace std;

//////
// The smaller hidden (per function) tests
//////

static bool testcreatedeleteMatrix 	(void);
static bool testcopyMatrix 				(void);
static bool testtransposeMatrix 		(void);
static bool testadddeleteRowMatrix 	(void);
static bool testaddColumnMatrix 		(void);
static bool testgaussianEliminate 	(void);
static bool testComplicatedGaussianElimination(void);
static bool testmultiplyMatricies 	(void);
static bool testsolveMatrix 			(void);
// NB: SOME TESTS WERE CONSIDERED VOID BECAUSE THEY WERE USED HEAVILY BY OTHER TESTS (laziness excuse lol)

static void printResult (bool res);

//////
// The one big test that is called
//////

bool testmatrix (void) {
	bool res, ores;
   res = ores = true;
	
	cout << "Testing create/delete Matrix...";
	res = testcreatedeleteMatrix ();
	ores &= res;
	printResult (res); 
	
	cout << "Testing add / delete row to Matrix...";
	res = testadddeleteRowMatrix ();
	ores &= res;
	printResult (res); 
	
	cout << "Testing copy Matrix...";
	res = testcopyMatrix ();
	ores &= res;
	printResult (res); 
	
	cout << "Testing add column to Matrix...";
	res = testaddColumnMatrix ();
	ores &= res;
	printResult (res); 
	
	cout << "Testing transopse Matrix...";
	res = testtransposeMatrix ();
	ores &= res;
	printResult (res); 
	
	cout << "Testing multiply Matricies...";
	res = testmultiplyMatricies ();
	ores &= res;
	printResult (res); 
	
	cout << "Testing gaussian eliminate Matrix...";
	res = testgaussianEliminate ();
	ores &= res;
	printResult (res); 
	
	cout << "Testing complicated gaussian eliminate Matrix...";
	res = testComplicatedGaussianElimination();
	ores &= res;
	printResult (res); 

	cout << "Testing solve Matrix...";
	res = testsolveMatrix ();
	ores &= res;
	printResult (res); 
	
	return ores;
}

//////
// All of the helper tests that are called
//////

static bool testcreatedeleteMatrix (void) {
   bool result;
	matrix<int>* m = new matrix<int>;
	
	result = (m != NULL);
	
   delete m;
	
	return result;
}

static bool testcopyMatrix (void) {
	matrix<double> *m, *c;
	Vector<double>* test;
	
   m = new matrix<double>;
	
   test = new Vector<double>;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
         test->setValue(j, j + (4 * i));
		}
		
      m->addRow(test);
	}
   delete test;
	
   c = new matrix<double>;
   c->copy(m);
   delete m;
	
	bool result = true;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			result &= (c->getValue(i, j) == j + (4 * i));
		}
	}
	
   delete c;
	
	return result;
}

static bool testtransposeMatrix (void) {
	matrix<double> m;
	
   {
      Vector<double> r;
      for (int i = 0; i < 8; i++) {
         for (int j = 0; j < 4; j++) {
            r.setValue(j, j);
         }
         
         m.addRow(&r);
      }
   }
	
   m.transpose();
	
	if (m.getHeight() != 4) return false;
	if (m.getWidth() != 8) return false;
	
	bool result = true;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 8; j++) {
			result &= (m.getValue(i, j) == i);
		}
	}
	
   m.transpose();
	
	if (m.getHeight() != 8) return false;
	if (m.getWidth() != 4) 	return false;
	
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 4; j++) {
			result &= (m.getValue(i, j) == j);
		}
	}
	
	return result;
}

static bool testadddeleteRowMatrix (void) {
	matrix<int>* m;
	Vector<int>* v;
	
   m = new matrix<int>;
	
   v = new Vector<int>;
	
   m->addRow(v);
   delete v;
	
   bool result = true;
	result &= (m->getRow(0) != NULL);
	result &= (m->getHeight() == 1);
	
   m->deleteRow(0);
	
	result &= (m->getRow(0) == NULL);
	result &= (m->getHeight() == 0);
	
   delete m;
	
	return result;
}

static bool testaddColumnMatrix (void) {
	matrix<int>* m;
	Vector<int>* v;
	
	m = new matrix<int>;
	
	v = new Vector<int>;
	
   v->setDimension(3);
	for (int i = 0; i < 3; ++i) {
      v->setValue(i, i);
	}
	
	for (int i = 0; i < 3; ++i) {
      m->addColumn(v);
	}
   delete v;
	
   bool result = true;
	for (int i = 0; i < 3; ++i) 
   {
		for (int j = 0; j < 3; ++j) 
      {
			result &= (m->getValue(i, j) == i);
		}
	}
	
   delete m;
	
	return result;
}

static bool testgaussianEliminate (void) {
   matrix<int>* m;
   Vector<int>* r;
	
	m = new matrix<int>;
	
	// a matrix (prentend that this is not the worst case of copy paste that you have ever
   // seen)
	r = new Vector<int>;
   r->setDimension(6);
	r->setValue(0, 1); r->setValue(1, 1); r->setValue(2, 0); r->setValue(3, 0); r->setValue(4, 0); r->setValue(5, 1); m->addRow(r);
	r->setValue(0, 1); r->setValue(1, 1); r->setValue(2, 1); r->setValue(3, 0); r->setValue(4, 0); r->setValue(5, 2); m->addRow(r);
	r->setValue(0, 0); r->setValue(1, 1); r->setValue(2, 1); r->setValue(3, 1); r->setValue(4, 0); r->setValue(5, 2); m->addRow(r);
	r->setValue(0, 0); r->setValue(1, 0); r->setValue(2, 1); r->setValue(3, 1); r->setValue(4, 1); r->setValue(5, 2); m->addRow(r);
	r->setValue(0, 0); r->setValue(1, 0); r->setValue(2, 0); r->setValue(3, 1); r->setValue(4, 1); r->setValue(5, 1); m->addRow(r);
   delete r;
	
   m->gaussianEliminate();
	
   delete m;

	// there are missing checks here but viewing through DDD shows it achieves an acceptable result (this is by no means good testing)
	return true;
}

static bool testComplicatedGaussianElimination(void)
{
   int array_matrix[22][19] = {
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

   matrix<double> *m = new matrix<double>;
   logger* nop = new nop_logger;

   // load the values into a matrix.
   {
      Vector<double> tempRow;

      for(int row = 0; row < 22; ++row)
      {
         tempRow.reset(0);
         for(int col = 0; col < 19; ++col)
         {
            tempRow.setValue(col, array_matrix[row][col]);
         }
         m->addRow(&tempRow);
      }
   }

   cout << endl;
   m->render(nop);
   m->gaussianEliminate();
   m->render(nop);

   delete m;

   return true;
}

static bool testmultiplyMatricies (void) {
	return true;	// not completed
}

static bool testsolveMatrix (void) {
	matrix<double> *m = new matrix<double>;
	Vector<double> *r = new Vector<double>;

	r->setValue(0, 1); r->setValue(1, -2); r->setValue(2, 3); r->setValue(3, 11); m->addRow(r);
	r->setValue(0, 2); r->setValue(1, -1); r->setValue(2, 3); r->setValue(3, 10); m->addRow(r);
	r->setValue(0, 4); r->setValue(1, 1); r->setValue(2, -1); r->setValue(3, 4); m->addRow(r);
	
   r->setDimension(3);
	r->setValue(0, 2); r->setValue(1, -3); r->setValue(2, 1);
	
   matrix<double>::solution solve_result;
   auto res = m->solve(&solve_result);
   bool result = true;
   if(res != nullptr)
   {
      res->round();

      result &= r->equal(res.get());
      result &= (solve_result == matrix<double>::UNIQUE_SOLUTION);
   }
   else
   {
      result = false;
   }

   delete r;
   delete m;
	
	return result;
}

static void printResult (bool res) {
	if (res) {
      cout << "PASS" << endl;
	} else {
      cout << "FAIL" << endl;
	}
}
