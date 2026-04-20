#ifndef MNM_CLI_H
#define MNM_CLI_H

#include "config.h"

// Parse command-line arguments into a Config.
// Prints usage and exits with EXIT_FAILURE on bad input.
// Prints usage and exits with EXIT_SUCCESS on --help.
Config parseArgs(int argc, char** argv);

#endif
