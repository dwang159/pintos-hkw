#ifndef INTERNAL_H
#define INTERNAL_H

#include "command.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>

void exec_cd(command * cmd);
void exec_history();
void exec_exit(command * cmd);

#endif
