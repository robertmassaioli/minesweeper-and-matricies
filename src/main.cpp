#include <iostream>
#include <cstdlib>
#include <memory>
#include <vector>

#include "game.h"
#include "solver.h"
#include "logging.h"
#include <fstream>

using namespace std;

class results
{
   public:
      results()
      {
         winCount = loseCount = progressCount = total = 0;
      }

      void count(GameState state, unsigned int randomSeed)
      {
         total++;
         switch(state)
         {
            case PROGRESS:
               progressCount++;
               break;

            case WON:
               winCount++;
               break;

            case LOST:
               loseCount++;
               losses.push_back(randomSeed);
               break;
         }
      }

      int winCount, loseCount, progressCount, total;
      vector<unsigned int> losses;
};

static void printResults(logger* log, results& res);
static GameState solveRandomGame(Dimensions& dim, int mineCount, logger* log);

int main(int argc, char** argv)
{
   // Create the logger
   std::unique_ptr<logger> log;
   if(argc == 2)
   {
      fstream out_file(argv[1], fstream::out);
      log = std::make_unique<ostream_logger>(out_file);
   }
   else
   {
      log = std::make_unique<nop_logger>();
   }

   /*
    * The windows version of minesweeper has the following difficulties.
         Beginner: 8 × 8 or 9 × 9 field with 10 mines
         Intermediate: 16 × 16 field with 40 mines
         Expert: 30 × 16 field with 99 mines
   */

   // Create the game
   Dimensions dim(16, 16);
   const int mines = 40;
   const int testRuns = 100000;

   // Init the random numbers
   unsigned int initialRandom = unsigned( time(NULL) );
   srand(initialRandom);
   (*log) << "Initial Random: " << initialRandom << logger::endl;

   results res;
   for(int i = 0; i < testRuns; ++i)
   {
      // Seed the random number generator
      unsigned int randomSeed = rand();
      srand(randomSeed);
      (*log) << "Random Seed: " << randomSeed << logger::endl << logger::endl;

      // Solve a game and count the result
      GameState result = solveRandomGame(dim, mines, log.get());
      res.count(result, randomSeed);

      if(i % 500 == 0)
      {
         cout << "Processed " << i << std::endl;
      }
   }
   cout << "Processed " << testRuns << std::endl;

   {
      auto tempLogger = std::make_unique<ostream_logger>(std::cout);
      printResults(tempLogger.get(), res);
   }
   printResults(log.get(), res);

   return EXIT_SUCCESS;
}

static GameState solveRandomGame(Dimensions& dim, int mineCount, logger* log)
{
   Game game(dim, mineCount, log);
   solver turnSolver;

   // Make the initial move
   // TODO make this a random move
   {
      Move move(Position(rand() % dim.getWidth(), rand() % dim.getHeight()), NORMAL); 
      game.acceptMove(move);
   }

   // Print out the game after the first move
   game.print();

   // Now get the AI to work out the rest
   std::unique_ptr<std::vector<Move>> movesToPerform;
   do
   {
      movesToPerform = turnSolver.getMoves(game.getBoard(), log);

      if(movesToPerform)
      {
         for(const Move& m : *movesToPerform)
         {
            Move currentMove = m;
            game.acceptMove(currentMove);
         }
      }
      game.print();
   } while (game.getState() == PROGRESS && movesToPerform && !movesToPerform->empty());

   (*log) << logger::endl;
   game.print();

   return game.getState();
}

static void printResults(logger* log, results& res)
{
   double winPercentage = ((double) (res.winCount * 100)) / ((double) res.total);
   double progressPercentage = ((double) (res.progressCount * 100)) / ((double) res.total);
   (*log) << "WINs: " << res.winCount << " (" << winPercentage << "%)" << logger::endl;
   (*log) << "PROGRESSes " << res.progressCount << " (" << progressPercentage << "%)" << logger::endl;
   (*log) << "ERRORS (losses) " << res.loseCount << logger::endl;

   if(!res.losses.empty())
   {
      // Print out losses
      (*log) << "Seeds for losses: " << logger::endl;
      for(unsigned int seed : res.losses)
      {
         (*log) << " " << seed << logger::endl;
      }
   }
}
