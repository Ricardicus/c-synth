#ifndef OPTIONS_H
#define OPTIONS_H

#include "synth_config.h"
#include <stddef.h>

/* 0: success, 1: help requested, -1: error (written into error buffer). */
int parseOptions(int argc, char *argv[], SynthConfig *config,
                 char *error, size_t errorSize);
void printUsage(const char *program);

#endif
