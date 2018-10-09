#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include "list.h"

// size of read buffer
#define BUFSIZE 512

list *load_rules(const char *path);
#endif
