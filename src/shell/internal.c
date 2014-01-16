#include "internal.h"

void exec_cd(command *cmd) {
    // Create a buffer for the current directory
    char curdir[MAXLINE];

    // Get the current directory and concatenate the user input
    getcwd(curdir, MAXLINE);
    strncat(curdir, "/", MAXLINE - 1);
    strncat(curdir, cmd->argv_cmds[1], MAXLINE - 1);
    curdir[MAXLINE - 1] = '\0';

    chdir(curdir);
}

void exec_exit() {
    printf("Bye!\n");
    exit(0);
}

void exec_history() {
    HIST_ENTRY **hist = history_list();
    int i = 0;
    while (hist[i] != NULL)
    {
        printf("%d: %s\n", i, hist[i]->line);
        i++;
    }
}
