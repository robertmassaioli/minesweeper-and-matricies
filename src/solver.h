#ifndef __ROBERTM_MINESWEEPER_SOLVER
#define __ROBERTM_MINESWEEPER_SOLVER

#include <vector>
#include <memory>

class solver
{
   public:
      std::unique_ptr<std::vector<Move>> getMoves(Board* board, logger* log);
};

#endif
