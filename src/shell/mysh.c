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
    /* TODO: remove, but it drives me crazy to have such a long prompt. */
    int attempted_len = strlcat(prompt, ">\n\t", MAXLINE);
    return attempted_len != (int) strlen(prompt);
}

int main() {
    int exit = false;
    char prompt[MAXLINE];
    char *line;
    token tknd_input[MAXTOKENS];
    while(!exit) {
        make_prompt(prompt);
        line = readline(prompt);
        if (!line)
            break;
        add_history(line);
        if (tokenize_input(line, tknd_input) == -1) {
            fprintf(stderr, "Could not parse input, please try again.\n");
        } else {
            for (int i = 0; tknd_input[i].type != EMPTY; i++)
                print_token(tknd_input[i]);
        }
    }
    return 0;
}
