#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "tokenize.h"
#include "command.h"
#include "execute.h"

/* Takes a pointer to a buffer that will hold the prompt, and
 * fills it with the prompt, equivalent to "\u:\w>" but
 * without substituting $HOME with a ~. Returns 0 if the
 * prompt fits in MAXLINE bytes and 1 if there is an
 * overflow.
 */
int make_prompt(char *prompt) {
    char *username = getlogin();
    strlcpy(prompt, username, MAXLINE);
    strlcat(prompt, ":", MAXLINE);
    char *cwd = getcwd(NULL, MAXLINE);
    strlcat(prompt, cwd, MAXLINE);
    int attempted_len = strlcat(prompt, "$ ", MAXLINE);
    return attempted_len != (int) strlen(prompt);
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
            execute_commands(cmds);
        }
        free_token_list(tknd_input);
        /* Note: mkae sure to be careful not to delete any strings that might
         * be needed in a background (&) process. */
    }
    return 0;
}
