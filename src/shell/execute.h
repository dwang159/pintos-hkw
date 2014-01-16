#ifndef EXECUTE_H
#define EXECUTE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "command.h"
#include "internal.h"

int dup2_stdfiles(int in, int out, int err);
void execute_commands(command *cmds);

#endif /* EXECUTE_H */
