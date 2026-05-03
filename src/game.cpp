#include <iostream>
#include <cstdlib>
#include <cassert>
#include <algorithm>
#include <random>
#include <vector>

#include "game.h"
#include "boardspec.h"

using namespace std;

/**
 * TODO change this to be a function that is provided from Position.
 * This map contains all of the potential relative positions.
 */
static int map[8][2] = {
   {-1,-1},
   {0,-1},
   {1,-1},
   {1, 0},
   {1, 1},
   {0, 1},
   {-1,1},
   {-1,0}
};

// Implementing the Game
Game::Game(Dimensions dim, int mineCount, logger* log)
   : board(dim, mineCount, log), log(log)
{
   state = PROGRESS;
}

Game::Game(const BoardSpec& spec, logger* log)
   : board(spec, log), log(log)
{
   state = PROGRESS;
}

Game::~Game()
{
}

void Game::acceptMove(Move& m)
{
   if(state == PROGRESS)
   {
      state = board.clickSquare(m);
   }
}

void Game::print()
{
   // TODO maybe show more game information
   board.print();
}

Board* Game::getBoard()
{
   return &board;
}

//
// Implementing the Board
//
Board::Board(Dimensions dim, int mineCount, logger* log)
   : dim(dim), log(log)
{
   generated = false;
   mines = mineCount;
}

Board::Board(const BoardSpec& spec, logger* log)
   : dim(spec.width, spec.height), log(log)
{
   int w = spec.width, h = spec.height;
   int total = w * h;
   grid = std::make_unique<Square[]>(total);

   mines = 0;
   int clickedCount = 0;

   // First pass: mark mines and states
   for (int i = 0; i < total; ++i)
   {
      grid[i].value = spec.mines[i] ? MINE : EMPTY;
      grid[i].state = spec.states[i];

      if (spec.mines[i])
         ++mines;
      if (spec.states[i] == CLICKED)
         ++clickedCount;
   }

   // Second pass: compute neighbor-mine counts for all non-mine cells,
   // matching what generateGrid does for a randomly-placed board.
   for (int row = 0; row < h; ++row)
   {
      for (int col = 0; col < w; ++col)
      {
         int idx = row * w + col;
         if (grid[idx].value == MINE)
            continue;

         int count = 0;
         for (int d = 0; d < 8; ++d)
         {
            int nc = col + map[d][0];
            int nr = row + map[d][1];
            if (isValidPos(nc, nr))
               count += (grid[nr * w + nc].value == MINE) ? 1 : 0;
         }
         grid[idx].value = count;
      }
   }

   // squaresLeft mirrors how generateGrid initialises it: total cells minus
   // already-clicked cells. The win condition (squaresLeft == mines) then fires
   // once every non-mine cell has been clicked.
   squaresLeft = total - clickedCount;
   generated = true;
}

Board::~Board()
{
}

void Board::print()
{
   logger& l = log->at(LogLevel::DEBUG);
   for(int row = 0; row < dim.getHeight(); ++row)
   {
      for(int col = 0; col < dim.getWidth(); ++col)
      {
         int position = locPos(col, row);
         int gridValue;
         switch(grid[position].state)
         {
            case CLICKED:
               gridValue = grid[position].value;
               switch(gridValue)
               {
                  case MINE:
                     l << "M";
                     break;

                  case EMPTY:
                     l << " ";
                     break;

                  default:
                     l << gridValue;
                     break;
               }
               break;

            case FLAG_CLICKED:
               l << "F";
               break;

            case QUESTION_CLICKED:
               l << "?";
               break;

            case NOT_CLICKED:
               l << "#";
               break;

            default:
               l << "E";
               break;
         }
      }

      l << logger::endl;
   }
}

GameState Board::clickSquare(Move& move)
{
   if(!generated)
   {
      generateGrid(move);
   }

   GameState resultingState = PROGRESS;

   Position clickPosition = move.getPosition();
   if(isValidPos(clickPosition)) 
   {
      int position = locPos(move);

      if(grid[position].state == NOT_CLICKED)
      {
         switch(move.getClickType())
         {
            case NORMAL:
               squaresLeft -= openEmptySquares(clickPosition);

               if(grid[position].state == NOT_CLICKED)
               {
                  grid[position].state = CLICKED;
                  squaresLeft--;
               }

               // Check to see if you won or lost on this click
               if(grid[position].value == MINE)
               {
                  // if you clicked a mine then you lost
                  resultingState = LOST;
               }
               else if(squaresLeft == mines) 
               {
                  // if you did not click a mine and only mines are left on the board then
                  // you won!
                  resultingState = WON;
               }
               break;

            case FLAG:
               if(grid[position].state != CLICKED)
               {
                  grid[position].state = FLAG_CLICKED;
               }
               break;

            case QUESTION:
               if(grid[position].state != CLICKED)
               {
                  grid[position].state = QUESTION_CLICKED;
               }
               break;

            case EXPAND:
               if(grid[position].state != CLICKED)
               {
                  resultingState = expandSquares(clickPosition);
               }
               break;
         }
      }
   } else {
      // TODO we should report some sort of error message or mistake here
      (*log) << "ERROR: The position provided in the move did not exist on the board." << logger::endl;
   }

   return resultingState;
}

int Board::openEmptySquares(Position& position)
{
   int pos = locPos(position);
   int clicked = 0;

   if(isValidPos(position))
   {
      if(grid[pos].state != CLICKED)
      {
         grid[pos].state = CLICKED;
         clicked++;

         if(grid[pos].value == EMPTY)
         {
            for(int mapPos = 0; mapPos < 8; ++mapPos)
            {
               Position nextPos(position.getX() + map[mapPos][0], position.getY() + map[mapPos][1]);
               clicked += openEmptySquares(nextPos);
            }
         }
      }
   }

   return clicked;
}

GameState Board::expandSquares(Position& position)
{
   int count = 0;

   // Count the number of adjacent flags
   for(int i = 0; i < 8; ++i)
   {
      Position tempPos(position.getX() + map[i][0], position.getY() + map[i][1]);
      if(isValidPos(tempPos))
      {
         count += (grid[locPos(tempPos)].state == FLAG_CLICKED);
      }
   }

   GameState lastGameState = PROGRESS;
   // if you have clicked enough adjacent flags
   if(count == grid[locPos(position)].value)
   {
      // click each adjacent square normally
      for(int i = 0; i < 8; ++i)
      {
         Position tempPos(position.getX() + map[i][0], position.getY() + map[i][1]);
         Move move(tempPos, NORMAL);
         lastGameState = clickSquare(move);
      }
   }

   return lastGameState;
}

Dimensions Board::getDimensions() const
{
   return dim;
}

Square* Board::getGrid()
{
   return grid.get();
}

int Board::getMineCount() const
{
   return mines;
}

Position Board::posLoc(int position) const
{
   int row = position / dim.getWidth();
   int col = position % dim.getWidth();
   return Position(col, row);
}

int Board::locPos(Move& move)
{
   Position pos = move.getPosition();
   return locPos(pos);
}

int Board::locPos(Position& pos)
{
   return locPos(pos.getX(), pos.getY());
}

int Board::locPos(int col, int row)
{
   return row * dim.getWidth() + col;
}

bool Board::isValidPos(Position& pos)
{
   return isValidPos(pos.getX(), pos.getY());
}

bool Board::isValidPos(int col, int row)
{
   if((row < 0) || (row >= dim.getHeight())) return false;
   if((col < 0) || (col >= dim.getWidth())) return false;

   return true;
}

class IncGenerator
{
   public:
      IncGenerator(Dimensions dim) 
         : dimensions(dim), current(Position(0,0))
      {
      }

      IncGenerator(Position initial, Dimensions dim)
         : dimensions(dim), current(initial)
      {
      }

      Position operator()()
      {
         Position last = current;
         if(current.getX() + 1 == dimensions.getWidth()) 
         {
            current = Position(0, current.getY() + 1);
         }
         else
         {
            current = Position(current.getX() + 1, current.getY());
         }
         return last;
      }

   private:
      Dimensions dimensions;
      Position current;
};

// We can assume that the move is valid by this stage.
void Board::generateGrid(Move& move)
{
   int width = dim.getWidth();
   int height = dim.getHeight();
   int totalSquares = width * height;
   squaresLeft = totalSquares;
   assert(mines >= 0);
   assert(mines < totalSquares); // Cannot place more mines than there are squares.

   // Shuffle the positions so that we can pick the ones that we want to be the mines.
   vector<Position> squarePositions(totalSquares);
   IncGenerator gen(dim);
   generate(squarePositions.begin(), squarePositions.end(), gen);
   // Seed from rand() so the shuffle is controlled by the same srand() seed as
   // the rest of the program, making --seed reproduce identical runs.
   std::mt19937 rng{static_cast<unsigned>(rand())};
   std::shuffle(squarePositions.begin(), squarePositions.end(), rng);

   // Generate the board
   grid = std::make_unique<Square[]>(totalSquares);
   for(int i = 0; i < totalSquares; ++i)
   {
      grid[i].state = NOT_CLICKED;
      grid[i].value = EMPTY;
   }

   // Pick the Mines
   {
      int mineCount = 0;
      for(
            vector<Position>::iterator positionIter = squarePositions.begin();
            mineCount < mines && positionIter != squarePositions.end();
            ++positionIter)
      {
         if(!positionIter->isAdjacent(move.getPosition())) {
            grid[locPos(*positionIter)].value = MINE;
            ++mineCount;
         }
      }
   }

   // Calculate the numbers of the squares
   for(int row = 0; row < dim.getHeight(); ++row)
   {
      for(int col = 0; col < dim.getWidth(); ++col)
      {
         int currentSquare = locPos(col, row);

         if(grid[currentSquare].value != MINE)
         {
            int count = EMPTY;

            for(int mapCounter = 0; mapCounter < 8; ++mapCounter)
            {
               Position testPos(col + map[mapCounter][0], row + map[mapCounter][1]);
               if(isValidPos(testPos))
               {
                  count += (grid[locPos(testPos)].value == MINE);
               }
            }

            grid[currentSquare].value = count;
         }
      }
   }

   // Now finally state that the board has been generated
   generated = true;
}

GameState Game::getState()
{
   return state;
}
