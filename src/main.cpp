#include <iostream>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <fstream>

#include "game.h"
#include "solver.h"
#include "logging.h"
#include "cli.h"

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
static GameState solveRandomGame(Dimensions& dim, int mineCount,
                                  GuessingStrategy strategy, logger* log);

int main(int argc, char** argv)
{
   Config cfg = parseArgs(argc, argv);

   // Create the logger
   std::unique_ptr<logger> log;
   std::unique_ptr<std::fstream> logStream;
   if (!cfg.logFile.empty())
   {
      logStream = std::make_unique<std::fstream>(cfg.logFile, std::fstream::out);
      auto ol = std::make_unique<ostream_logger>(*logStream);
      ol->setLevel(cfg.logLevel);
      log = std::move(ol);
   }
   else
   {
      log = std::make_unique<nop_logger>();
   }

   Dimensions dim(cfg.width, cfg.height);
   const int mines     = cfg.mines;
   const int testRuns  = cfg.runs;

   // Init the random numbers
   unsigned int initialRandom = cfg.fixedSeed ? cfg.seed : unsigned(time(NULL));
   srand(initialRandom);
   log->at(LogLevel::INFO) << "Initial Random: " << initialRandom << logger::endl;

   results res;
   for(int i = 0; i < testRuns; ++i)
   {
      // Seed the random number generator
      unsigned int randomSeed = rand();
      srand(randomSeed);
      (*log) << "Random Seed: " << randomSeed << logger::endl << logger::endl;

      // Solve a game and count the result
      GameState result = solveRandomGame(dim, mines, cfg.strategy, log.get());
      res.count(result, randomSeed);

      if(i % 500 == 0)
      {
         cout << "Processed " << i << std::endl;
      }
   }
   cout << "Processed " << testRuns << std::endl;
   cout << std::endl;

   {
      auto tempLogger = std::make_unique<ostream_logger>(std::cout);
      printResults(tempLogger.get(), res);
   }
   printResults(log.get(), res);

   return EXIT_SUCCESS;
}

static const char* strategyName(GuessingStrategy s)
{
   switch (s)
   {
      case GuessingStrategy::MONTE_CARLO: return "MONTE_CARLO";
      case GuessingStrategy::NONE:        return "NONE";
      default:                            return "UNKNOWN";
   }
}

static const char* gameStateName(GameState s)
{
   switch (s)
   {
      case WON:      return "WON";
      case LOST:     return "LOST";
      case PROGRESS: return "STALLED";
      default:       return "UNKNOWN";
   }
}

static GameState solveRandomGame(Dimensions& dim, int mineCount,
                                  GuessingStrategy strategy, logger* log)
{
   log->at(LogLevel::INFO) << std::string(60, '=') << logger::endl;
   log->at(LogLevel::INFO) << " GAME START" << logger::endl;
   log->at(LogLevel::INFO) << " Board: " << dim.getWidth() << " x " << dim.getHeight()
                           << ",  mines: " << mineCount
                           << ",  strategy: " << strategyName(strategy) << logger::endl;
   log->at(LogLevel::INFO) << std::string(60, '=') << logger::endl;

   Game game(dim, mineCount, log);
   solver turnSolver(strategy);

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

   log->at(LogLevel::DEBUG) << logger::endl;
   game.print();

   GameState finalState = game.getState();
   log->at(LogLevel::INFO) << std::string(60, '=') << logger::endl;
   log->at(LogLevel::INFO) << " GAME END: " << gameStateName(finalState) << logger::endl;
   log->at(LogLevel::INFO) << std::string(60, '=') << logger::endl;

   return finalState;
}

static void printResults(logger* log, results& res)
{
   double winPercentage      = (res.winCount      * 100.0) / res.total;
   double lossPercentage     = (res.loseCount     * 100.0) / res.total;
   double progressPercentage = (res.progressCount * 100.0) / res.total;

   if(!res.losses.empty())
   {
      (*log) << "Seeds for losses:" << logger::endl;
      for(unsigned int seed : res.losses)
         (*log) << "  " << seed << logger::endl;
      (*log) << logger::endl;
   }

   (*log) << "WINs:      " << res.winCount      << " (" << winPercentage      << "%)" << logger::endl;
   (*log) << "LOSSes:    " << res.loseCount      << " (" << lossPercentage     << "%)" << logger::endl;
   (*log) << "STALLed:   " << res.progressCount  << " (" << progressPercentage << "%)" << logger::endl;
   (*log) << "Total:     " << res.total << logger::endl;
}
