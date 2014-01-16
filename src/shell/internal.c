#include "internal.h"

void exec_cd(command *cmd) {
    /* Create a buffer for the current directory */
    char cwd[MAXLINE];
    char dest[MAXLINE];
    char *new_dir = cmd->argv_cmds[1];

    if (new_dir == NULL) {
        /* Change directory to user's home directory. */
        strncpy(dest, "/home", MAXLINE);
        dest[MAXLINE - 1] = '\0';
    } else if (new_dir[0] == '/') {
        /* User has provided an absolute path. */
        strncpy(dest, new_dir, MAXLINE);
        dest[MAXLINE - 1] = '\0';
    } else {
        getcwd(cwd, MAXLINE);

        /* The destination must end with a /. We check if the last
         * character of the directory the user provides is a '/'.
         */
        snprintf(dest, MAXLINE, "%s/%s", cwd, new_dir);
    }
    printf("%s\n", dest);
    if (chdir(dest) < 0) {
        fprintf(stderr, "cd: %s: No such file or directory", new_dir);
    }
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
