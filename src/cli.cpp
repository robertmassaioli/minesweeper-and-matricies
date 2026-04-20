#include "cli.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <string>

static const char* USAGE = R"(Usage: mnm [options]

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
)";

static void usageError(const std::string& msg)
{
    std::cerr << "error: " << msg << "\n\n" << USAGE;
    std::exit(EXIT_FAILURE);
}

// Returns the next argv token as a string, or calls usageError if none remains.
static std::string nextToken(int i, int argc, char** argv, const std::string& flag)
{
    if (i >= argc)
        usageError(flag + " requires a value");
    return argv[i];
}

// Parses a token as a positive integer, or calls usageError.
static int parsePositiveInt(const std::string& token, const std::string& flag)
{
    try
    {
        int v = std::stoi(token);
        if (v < 1)
            usageError(flag + " must be at least 1");
        return v;
    }
    catch (const std::exception&)
    {
        usageError(flag + " requires an integer value, got: " + token);
        return 0; // unreachable
    }
}

// Parses a token as a non-negative integer, or calls usageError.
static unsigned parseUnsignedInt(const std::string& token, const std::string& flag)
{
    try
    {
        long v = std::stol(token);
        if (v < 0)
            usageError(flag + " must be non-negative");
        return static_cast<unsigned>(v);
    }
    catch (const std::exception&)
    {
        usageError(flag + " requires an integer value, got: " + token);
        return 0; // unreachable
    }
}

static void applyPreset(const std::string& name, Config& cfg)
{
    if (name == "beginner")
    {
        cfg.width = 9; cfg.height = 9; cfg.mines = 10;
    }
    else if (name == "intermediate")
    {
        cfg.width = 16; cfg.height = 16; cfg.mines = 40;
    }
    else if (name == "expert")
    {
        cfg.width = 30; cfg.height = 16; cfg.mines = 99;
    }
    else
    {
        usageError("unknown preset '" + name + "' (choose: beginner, intermediate, expert)");
    }
}

Config parseArgs(int argc, char** argv)
{
    Config cfg;

    for (int i = 1; i < argc; ++i)
    {
        std::string flag = argv[i];

        if (flag == "--help")
        {
            std::cout << USAGE;
            std::exit(EXIT_SUCCESS);
        }
        else if (flag == "--preset")
        {
            applyPreset(nextToken(++i, argc, argv, flag), cfg);
        }
        else if (flag == "--width")
        {
            cfg.width = parsePositiveInt(nextToken(++i, argc, argv, flag), flag);
        }
        else if (flag == "--height")
        {
            cfg.height = parsePositiveInt(nextToken(++i, argc, argv, flag), flag);
        }
        else if (flag == "--mines")
        {
            cfg.mines = parsePositiveInt(nextToken(++i, argc, argv, flag), flag);
        }
        else if (flag == "--runs")
        {
            cfg.runs = parsePositiveInt(nextToken(++i, argc, argv, flag), flag);
        }
        else if (flag == "--seed")
        {
            cfg.seed      = parseUnsignedInt(nextToken(++i, argc, argv, flag), flag);
            cfg.fixedSeed = true;
        }
        else if (flag == "--log")
        {
            cfg.logFile = nextToken(++i, argc, argv, flag);
        }
        else
        {
            usageError("unrecognised option: " + flag);
        }
    }

    // Cross-field validation
    if (cfg.mines >= cfg.width * cfg.height)
        usageError("--mines must be less than width * height (" +
                   std::to_string(cfg.width * cfg.height) + ")");

    return cfg;
}
