#include "internal.h"

void exec_cd(command *cmd) {
    /* Create a buffer for the current directory */
    char cwd[MAXLINE];
    char dest[MAXLINE];
    char *new_dir = cmd->argv_cmds[1];
    char *username = getlogin();
    char home_dir[MAXLINE];
    struct utsname hostname;

    if (uname(&hostname) < 0) {
        fprintf(stderr, "error: uname() failed\n");
        exit(1);
    }

    /* Get the home directory of this user. The location of the
     * home directory is either /home/username for Linux and most other
     * unix systems or /Users/username for OS X.
     */
    if (strcmp(hostname.sysname, "Darwin") == 0) {
        snprintf(home_dir, MAXLINE, "/Users/%s", username);
    } else {
        snprintf(home_dir, MAXLINE, "/home/%s", username);
    }

    /* Determine the absolute path to the dest directory. */
    if (new_dir == NULL) {
        /* Change directory to user's home directory. */
        snprintf(dest, MAXLINE, "%s", home_dir);
    } else if (new_dir[0] == '/') {
        /* User has provided an absolute path. */
        snprintf(dest, MAXLINE, "%s", new_dir);
    } else if (new_dir[0] == '~' &&
            (new_dir[1] == '\0' || new_dir[1] == '/')) {
        /* User provided input like "cd ~" or "cd ~/dir", so we replace
         * the ~ with the home directory.
         */
        if (new_dir[1] == '\0') {
            snprintf(dest, MAXLINE, "%s", home_dir);
        } else {
            new_dir += 2;
            snprintf(dest, MAXLINE, "%s/%s", home_dir, new_dir);
        }
    } else {
        getcwd(cwd, MAXLINE);

        /* The destination must end with a /. We check if the last
         * character of the directory the user provides is a '/'.
         */
        snprintf(dest, MAXLINE, "%s/%s", cwd, new_dir);
    }

    /* Change the directory. */
    if (chdir(dest) < 0) {
        fprintf(stderr, "cd: %s: No such file or directory\n", new_dir);
    }
}

void exec_exit() {
    printf("Bye!\n");
    exit(0);
}

void exec_history() {
#ifndef __APPLE__
    HIST_ENTRY **hist = history_list();
    int i = 0;
    while (hist[i] != NULL)
    {
        printf("%d: %s\n", i, hist[i]->line);
        i++;
    }
#endif /* __APPLE__ */
}
