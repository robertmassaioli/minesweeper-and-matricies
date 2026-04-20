#ifndef __ROBERTM_MINESWEEPER_SOLVER
#define __ROBERTM_MINESWEEPER_SOLVER

#include <list>
#include <memory>

class solver
{
   public:
      std::unique_ptr<std::list<Move>> getMoves(Board* board, logger* log);
};

#endif
