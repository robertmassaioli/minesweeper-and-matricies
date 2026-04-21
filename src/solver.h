#ifndef __ROBERTM_MINESWEEPER_SOLVER
#define __ROBERTM_MINESWEEPER_SOLVER

#include <vector>
#include <memory>
#include "config.h"

class solver
{
   public:
      explicit solver(GuessingStrategy strategy = GuessingStrategy::MONTE_CARLO);
      std::unique_ptr<std::vector<Move>> getMoves(Board* board, logger* log);

   private:
      GuessingStrategy strategy;
      int turnCount;
};

#endif
