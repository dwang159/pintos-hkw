#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAXLINE 1024
#define MAXTOKENS 1024
#define MAXARGS 128

/* The following macros specify the kind of token found in
 * the input string for use in the token data structure. */
#define EMPTY       0 
#define CHINP       1 /* < */
#define CHOUT       2 /* > */
#define PIPE        3 /* | */
#define STRING      4 /* a string surrounded by '"' or without spaces. */
#define CHOUTAPP    5 /* >> */
#define BACKGROUND  6 /* & */
#define GENOUTRED   7 /* n> */
#define GENINOUTRED 8 /* a>&b */
#define RERUN       9 /* !n */

/* Algebraic data type to represent a token. If it comes out of a 
 * symbolic string, the data is empty and only the type is indicated.
 * If data is needed, like the number of the command to rerun, a pointer
 * to the string for the token, or the filedescriptor(s) referred to, that
 * is stored in the data union: .last for a rerun (n in !n), .onefiledes for
 * a general redirect (n in n>), .filedespair for advanced redirection 
 * ({a, b} in a>&b, and of course .str for when the token is just a string 
 * (a command, flag, argument, etc.). */
typedef struct {
    int type;
    union {
        int last;
        int onefiledes;
        int filedespair[2];
        char *str;
    } data;
} token;

typedef struct {
    char *cmdname;
    char **args;
    char *inp_source;
    char *outp_source;
} command;

const command CMD_DEFAULT = {NULL, NULL, NULL, NULL};

void print_token(token t) {
    printf("%d ", t.type);
    switch (t.type) {
    case STRING:
        printf("%s\n", t.data.str);
        break;
    case GENOUTRED:
        printf("%d\n", t.data.onefiledes);
        break;
    case GENINOUTRED:
        printf("%d %d\n", t.data.filedespair[0], t.data.filedespair[1]);
        break;
    case RERUN:
        printf("%d\n", t.data.last);
        break;
    default:
        printf("\n");
    }
}

int eq_token(const token a, const token b) {
    if (a.type != b.type) 
        return false;

    switch(a.type) {
    case STRING:
        return strcmp(a.data.str, b.data.str);
    case GENOUTRED:
        return a.data.onefiledes == b.data.onefiledes;
    case GENINOUTRED:
        return a.data.filedespair[0] == b.data.filedespair[0] &&
               a.data.filedespair[1] == b.data.filedespair[1];
    case RERUN:
        return a.data.last == b.data.last;
    default:
        return true;
    }
}
         
/* Checks if the substring at the start of inp matches
 * \d+>, for a redirection like n>. If it fails -1 is
 * return and t points to an empty token, else the 
 * number of characters consumed is returned and t
 * is updated with the appropriate file descriptor.
 */
int match_gen_redirect(const char *inp, token *t) {
    int length = 0;
    int is_found = false;
    int n = 0;
    char c;
    while (isdigit(c = *inp++)) {
        is_found = true;
        n = 10 * n + c - '0';
        ++length;
    }
    if (is_found && *inp == '>') {
        t->type = GENOUTRED;
        t->data.onefiledes = n;
        return length + 1;
    } else {
        t->type = EMPTY;
        return -1;
    }
}

/* Checks if the beginning of the string pointed to 
 * by inp matches the form "\d+>&\d+". Returns -1 if
 * the match fails, and puts EMPTY in the token. If
 * the match is successful, the return value is
 * the number of bytes consumed and t is updated
 * to reflect the redirection a>&b.
 */
int match_gen_bi_redirect(const char *inp, token *t) {
    int length = 0;
    int is_found = false;
    int a, b;
    a = b = 0;
    char c;
    while (isdigit(c = *inp++)) {
        ++length;
        a = 10 * a + c - '0';
        is_found = true;
    }
    if (!is_found) {
        t->type = EMPTY;
        return -1;
    }
    if (*inp++ == '>') 
        if (*inp++ == '&') {
            is_found = false;
            while (isdigit(c = *inp++)) {
                ++length;
                b = 10 * b + c - '0';
                is_found = true;
            }
            if (is_found) {
                t->type = GENINOUTRED;
                t->data.filedespair[0] = a;
                t->data.filedespair[1] = b;
                return length;
            }
        }
    t->type = EMPTY;
    return -1;
}
/* Divides the input up, based on special characters and spaces.
 * Tokens: "|", "<", ">", ">>", ">&", "&", "n>", "a>&b", "!n", "\"*\"", and 
 * words composed of '-', '_', and alphanumeric characters.
 */
int tokenize_input(const char *inp, token outp[]) {
    int tidx = 0;
    int inpdx = 0;
    token *t;
    t = (token *)malloc(sizeof(token));
    if (!t) {
        fprintf(stderr, "Allocation error. Aborting.\n");
        exit(0);
    }
    unsigned long toklen;
    char c;
    while ((c = inp[inpdx]) != '\0') {
        switch (c) {
        /* Single char tokens: make a one byte slice pointing to the current
         * location */
        case ' ':
            break;
        case '<':
            ++inpdx;
            outp[tidx++].type = CHINP;
            break;
        case '|':
            ++inpdx;
            outp[tidx++].type = PIPE;
            break;
        case '&':
            ++inpdx;
            outp[tidx++].type = BACKGROUND;
            break;
        case '>':
            ++inpdx;
            if (inp[inpdx] == '>') {
                outp[tidx++].type = CHOUTAPP;
                ++inpdx;
            } else if (inp[inpdx] == '&') {
                fprintf(stderr, "Missing file descriptor before >&\n");
            } else {
                outp[tidx++].type = CHOUT;
            }
            break;
        /* Quoted string: continue taking until closed. */
        case '"':
            toklen = 1; /* beginning '"' */
            while ((c = inp[inpdx + toklen]) != '"' && c != '\0')
                ++toklen;
            ++toklen; /* end '"' */
            if (c == '\0') {
                fprintf(stderr, "Unclosed double quote\n");
                return -1;
            }
            char *st = (char *) malloc(toklen * sizeof(char));
            if (!st) {
                fprintf(stderr, "Error allocating memory. Aborting.\n");
                exit(0);
            }
            strlcpy(st, inp + inpdx, toklen + 1); /* +1 for null character. */
            assert(strlen(st) == toklen);
            assert(st[0] == '"');
            assert(st[strlen(st) - 1] == '"');
            outp[tidx].type = STRING, 
            outp[tidx++].data.str = st;
            inpdx += toklen;
            break;
        case '!':
            ++inpdx;
            int n = 0;
            /*I'm not using atoi because a) there should not be any spaces after the
             * '!' or a +/- sign, and b) because the length is needed to properly move
             * through the input.
             */
            while (isdigit(c = inp[inpdx])) {
                n = n * 10 + c - '0';
                ++inpdx;
            }
            if (n == 0) {
                fprintf(stderr, "error: No number provided immediately after " 
                    "'!' or attempted to repeat a command not yet issued.\n");
                return -1;
            }
            outp[tidx].type = RERUN, 
            outp[tidx++].data.last = n;
            break;
        default:
            printf("froze");
            if ((toklen = match_gen_bi_redirect(inp + inpdx, t)) != -1) {
                outp[tidx++] = *t;
                inpdx += toklen; 
            } else if ((toklen = match_gen_redirect(inp + inpdx,  t)) != -1) {
                outp[tidx++] = *t;
                inpdx += toklen;
            } else {
                toklen = 1;
                printf("froze");
                while (isalnum(c = inp[inpdx]) || c == '-' || c == '_' || c == '/')
                    ++toklen;
                char *st = (char *) malloc(toklen * sizeof(char));
                if (!st) {
                    fprintf(stderr, "Error allocating memory. Aborting.\n");
                    exit(0);
                }
                strlcpy(st, inp + inpdx, toklen + 1); /* +1 for null character. */
                outp[tidx].type = STRING;
                outp[tidx++].data.str = st;
                inpdx += toklen;
                printf("inpdx: %d\n", inpdx);
            }
        }
    }
    return tidx;
}
                             

    

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
        printf("froze");
        line = readline(prompt);
        printf("%s\n", line);
        printf("froze");
        if (!line)
            break;
        //add_history(line);
        printf("froze");
        if (tokenize_input(line, tknd_input) == -1) {
            printf("Could not parse input, please try again.\n");
        } else {
            for (int i = 0; tknd_input[i].type != EMPTY; i++)
                print_token(tknd_input[i]);
        }
        printf("froze");
    }
    return 0;
}
