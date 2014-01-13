#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <readline/history.h>
#include "tokenize.h"

typedef struct {
    char **argv_cmds;
    int filedes_in;
    int filedes_out;
    int filedes_err;
} command;

const command CMDBLANK = {NULL, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
command *separate_commands(const token[]);
