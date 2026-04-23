#ifndef MNM_CONFIG_H
#define MNM_CONFIG_H

#include "logging.h"
#include <string>

enum class GuessingStrategy
{
    NONE,          // deterministic only — never guess
    MONTE_CARLO,   // probabilistic fallback via constraint-matrix sampling
};

struct Config
{
    int             width     = 16;
    int             height    = 16;
    int             mines     = 40;
    int             runs      = 100000;
    unsigned        seed      = 0;       // 0 means "use time(NULL)"
    bool            fixedSeed = false;
    std::string     logFile;             // empty means no log file
    std::string     boardFile;           // empty means random generation
    GuessingStrategy strategy = GuessingStrategy::MONTE_CARLO;
    LogLevel         logLevel = LogLevel::DEBUG;
};

#endif
