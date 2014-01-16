#ifndef COMMAND_H
#define COMMAND_H

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>
#include "tokenize.h"

#define MAX_COMMAND_SIZE 128
#define MAX_FILES 1024

typedef struct {
    char **argv_cmds;
    int filedes_in;
    int filedes_out;
    int filedes_err;
} command;

const command CMDBLANK;
command *separate_commands(const token[]);
int eq_command(const command, const command);
void print_command_list(const command *freeable);
#endif /* COMMAND_H */
