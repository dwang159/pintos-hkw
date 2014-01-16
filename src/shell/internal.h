#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/utsname.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "command.h"

void exec_cd(command *cmd);
void exec_history();
void exec_exit();

#endif
