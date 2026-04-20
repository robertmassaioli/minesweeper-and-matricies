#ifndef MNM_CONFIG_H
#define MNM_CONFIG_H

#include <string>

struct Config
{
    int         width     = 16;
    int         height    = 16;
    int         mines     = 40;
    int         runs      = 100000;
    unsigned    seed      = 0;       // 0 means "use time(NULL)"
    bool        fixedSeed = false;
    std::string logFile;             // empty means no log file
};

#endif
