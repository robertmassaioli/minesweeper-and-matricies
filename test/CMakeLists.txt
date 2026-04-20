include_directories( "${PROJECT_SOURCE_DIR}/src" )

add_executable( run-all-tests 
   run-tests.cpp
   testmatrix.cpp
   testvector.cpp
   )
target_link_libraries( run-all-tests
   )

add_test( AllTests run-all-tests )
