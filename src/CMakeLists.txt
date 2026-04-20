# Get required libraries
find_library(M_LIB m)

# Standalone Libraries (Axioms so to speak)
add_library( game SHARED game.cpp )

add_library( game-solver SHARED solver.cpp )
target_link_libraries( game-solver
   game
   )

# The actual game-solver itself
add_executable( mnm main.cpp )
target_link_libraries( mnm
   game
   game-solver
   )
