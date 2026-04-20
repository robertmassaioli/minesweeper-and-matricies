# Minesweeper and Matricies

## Introduction

Welcome to my code that demonstrates that you can solve all of the known parts of
Minesweeper by using Matricies and some special properties that you can derive from them.
Please note, in advance, that there will be certain situations where it is impossible to
tell for sure which squares are Mines and which are not. This solver will not make a move
if there are no sure moves that it can make; I do not attempt to do any probabalistic
analysis (yet).

If you want the explicit details on how it works then you should [read my blog post on the
subject][1].

## License

I have written all of this code myself and therefore it is mine to license as I wish. The
only part of the code that can be considered copied is the gaussian elimination algorithm
which I read on Wikipedia and then wrote myself.

Here is the short version: you can use the following code however you like, I will not
support it or be responsible for it but please give me attribution if you do use it. I
love attribution and all I really want is for people to go around saying:

> Hey, did you know that you can solve Minesweeper using Matricies? Yeah, this person called 
> Robert Massaioli came up with that and wrote this cool example program on the internet.

That is all I want. But for practical purposes you should consider this code to be
licensed under the MIT license.

## Compilation

The only dependency is [CMake][2] (version 3.10 or newer) and a C++17-capable compiler.

    mkdir localbuild && cd localbuild
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make

## Running

Run the solver with default settings (Intermediate difficulty, 100,000 games):

    ./src/mnm

### Options

    Usage: mnm [options]

    Options:
      --width N         Board width  (default: 16)
      --height N        Board height (default: 16)
      --mines N         Number of mines (default: 40)
      --runs N          Number of simulated games (default: 100000)
      --preset NAME     Shorthand difficulty: beginner | intermediate | expert
                        Sets width, height, and mines. Individual flags override.
      --seed N          Fix the initial RNG seed for reproducible runs (default: time-based)
      --log FILE        Write per-game trace to FILE
      --help            Print this message and exit

    Presets:
      beginner      9 x 9,  10 mines
      intermediate  16 x 16, 40 mines  (default)
      expert        30 x 16, 99 mines

### Examples

Quick smoke-test on Beginner difficulty:

    ./src/mnm --preset beginner --runs 1000

Run Expert with a fixed seed so results are reproducible:

    ./src/mnm --preset expert --seed 12345

Custom board with a per-game trace written to a file:

    ./src/mnm --width 9 --height 9 --mines 10 --runs 500 --log trace.txt

## Running the Tests

From the build directory:

    ctest

## Running the Benchmarks

From the build directory:

    ./bench/benchmarks

This runs three benchmarks for 2 seconds each and reports mean microseconds per
iteration and iterations per second: the full game pipeline, a single solver turn,
and Gaussian elimination in isolation.

## Installation

What, really? You actually want to install this code on your machine, well I'm chuffed.
You can use the standard cmake tools to install it but, honestly, it will probably just
clutter up your bin directory.

 [1]: http://robertmassaioli.wordpress.com/2013/01/12/solving-minesweeper-with-matricies/
 [2]: http://www.cmake.org/
