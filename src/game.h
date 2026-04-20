#ifndef _MINESWEEPER_GAME
#define _MINESWEEPER_GAME

#include "logging.h"
#include <cstdlib>
#include <memory>

// In every board game square there should be a mine and they can be EMPTY, 1-8 or a MINE.
#define NA -1
#define EMPTY 0
#define MINE 9

enum GameState 
{
   PROGRESS, WON, LOST
};

enum ClickType 
{
   NORMAL, FLAG, QUESTION, EXPAND
};

enum SquareState
{
   NOT_CLICKED,
   FLAG_CLICKED,
   QUESTION_CLICKED,
   CLICKED
};

// The prototypes of the classes so that people know they exist and are waiting to happen.
class Position;
class Dimensions;
class Move;
class Square;
class Board;
class Game;

/**
 * \brief This class represents dimensions.
 *
 * Please note that it is immutable.
 */
class Dimensions 
{
   public:
      Dimensions(int width, int height) 
      {
         this->width = width;
         this->height = height;
      }

      int getWidth() const { return width; }
      int getHeight() const { return height; }

   private:
      int width, height;
};

class Position
{
   public:
      Position()
      {
         this->x = this->y = 0;
      }

      Position(int x, int y)
      {
         this->x = x;
         this->y = y;
      }

      int getX() const { return x; }
      int getY() const { return y; }

      bool isAdjacent(Position other) const
      {
         return abs(x - other.x) <= 1 && abs(y - other.y) <= 1;
      }

   private:
      int x, y;
};

class Square
{
   public:
      int value;
      SquareState state;
};

class Board
{
   public:
      Board(Dimensions dim, int mineCount, logger* log);
      ~Board();

      void print();

      GameState clickSquare(Move& move);

      Dimensions getDimensions() const;
      Square* getGrid();

      bool isGenerated();
      
      Position posLoc(int position) const;

      int locPos(Move& move);
      int locPos(Position& pos);
      int locPos(int col, int row);
      bool isValidPos(Position& pos);
      bool isValidPos(int col, int row);

   private:
      void generateGrid(Move& move);
      int openEmptySquares(Position& position);
      GameState expandSquares(Position& position);

      std::unique_ptr<Square[]> grid;

      Dimensions dim;
      int mines, squaresLeft;
      bool generated;

      logger* log;
};

// TODO it is possible that only Game and Move need to be in the final interface. Think more on
// this.

/**
 * \brief This class represents a move that the user can make in a game of minesweeper. 
 *
 * When the user wants to make a move then they can just use this to make it happen. This way we can
 * pass moves into the game whether they come from an AI or a computer. Please note that this class
 * should be immutable.
 */
class Move
{
   public: 
      Move(Position pos, ClickType clickType)
         : position(pos), clickType(clickType)
      {}

      Position getPosition() { return position; }
      ClickType getClickType() { return clickType; }

   private:
      Position position;
      ClickType clickType;
};

class Game
{
   public:
      Game(Dimensions dim, int mineCount, logger* log);
      ~Game();

      void acceptMove(Move& m);
      void print();
      Board* getBoard();
      GameState getState();

   private:
      void generateBoard(int rows, int cols);

      Board board; 
      GameState state;

      logger* log;
};

#endif
