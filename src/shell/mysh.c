#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/utsname.h>

#include "tokenize.h"
#include "command.h"
#include "execute.h"

/* Takes a pointer to a buffer that will hold the prompt, and
 * fills it with the prompt.
 */
int make_prompt(char *prompt) {
    char *username = getlogin();
    struct utsname hostname;

    if (uname(&hostname) < 0) {
        fprintf(stderr, "error: uname() failed\n");
        exit(1);
    }

    char *cwd = getcwd(NULL, MAXLINE);

    /* Make a prompt that looks like this:
     *
     * [name@hostname /path/to/cwd] $
     */
    snprintf(prompt, MAXLINE, "[%s@%s %s]$ ", username,
        hostname.nodename, cwd);
    free(cwd);
    return 0;
}

int main() {
    int exit = false;
    char prompt[MAXLINE];
    char *line;
    command *cmds;
    token tknd_input[MAXTOKENS];
    using_history();
    while(!exit) {
        make_prompt(prompt);
        line = readline(prompt);
        if (!line)
            break;
        add_history(line);
        if (tokenize_input(line, tknd_input) == -1) {
            fprintf(stderr, "Could not parse input, please try again.\n");
        } else {
            cmds = separate_commands(tknd_input);
            if (cmds) {
                execute_commands(cmds);
            }
        }
        free(line);
        free_token_list(tknd_input);
        if (cmds != NULL) {
            free_command_list(cmds);
            free(cmds);
        }
        /* Note: mkae sure to be careful not to delete any strings that might
         * be needed in a background (&) process. */
    }
    return 0;
}
