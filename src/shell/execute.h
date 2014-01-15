#ifndef EXECUTE_H
#define EXECUTE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/history.h>
#include "command.h"

void execute_commands(command *cmds);

#endif /* EXECUTE_H */