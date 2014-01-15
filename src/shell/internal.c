#include "internal.h"

void exec_cd(command * cmd)
{
    // Create a buffer for the current directory
    char curdir[PATH_MAX];

    // Get the current directory and concatenate the user input
    getcwd(curdir, PATH_MAX);
    strcat(curdir, "/");
    strcat(curdir, cmd->argv_cmds[1]);

    chdir(curdir);    
}

void exec_exit(command * cmd)
{
    printf("Bye!\n");
    exit(0);
}

void exec_history()
{
    HIST_ENTRY ** hist = history_list();
    int i = 0;
    while (hist[i] != NULL)
    {
        printf("%d: %s\n", i, hist[i]->line);
        i++;
    }
}

int main(void)
{
    char * input = "h";
    using_history();
    while(strcmp(input, "hello"))
    {
    input = readline("hello: ");
    add_history(input);
    }
    exec_history();
    return 0;
}
