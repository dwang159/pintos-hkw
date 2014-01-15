#include "tokenize.h"

/* Primitive printing (for debugging) */
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

/* For debugging */
void print_token_list(const token ts[]) {
    int i;
    for (i = 0; i < MAXTOKENS && ts[i].type != EMPTY; i++) {
        print_token(ts[i]);
    }
}

/* Checks for equality between tokens: must be same type and
 * have the same data if applicable. */
int eq_token(const token a, const token b) {
    if (a.type != b.type)
        return false;
    switch(a.type) {
    case STRING:
        return !strcmp(a.data.str, b.data.str);
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

/* Treats a token with empty type for a "null" terminated list */
int eq_token_list(const token a[], const token b[]) {
    for (int i = 0; i < MAXTOKENS; i++) {
        if (!eq_token(a[i], b[i]))
            return false;
        if (a[i].type == EMPTY)
            return true;
    }
    return true;
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
    while (isdigit(c = *inp)) {
        ++inp;
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
    while (isdigit(c = *inp)) {
        ++inp;
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
            length += 2; /* for the redirection symbols */
            is_found = false;
            while (isdigit(c = *inp)) {
                ++inp;
                ++length;
                b = 10 * b + c - '0';
                is_found = true;
            }
            if (is_found) {
                t->type = GENINOUTRED;
                /* Put destination first and then source, see
                 * the `pipe` API so filedespair can just be passed. */
                t->data.filedespair[0] = b;
                t->data.filedespair[1] = a;
                return length;
            }
        }
    t->type = EMPTY;
    return -1;
}

/* Defines behavior for appropriate strings to be used in commands,
 * arguments to those commands, and filenames.
 */
int valid_strchr(char c) {
    return isalnum(c) || c == '.' || c == '/' || c == '-' || c == '_';
}

/* Divides the input up, based on special characters and spaces.
 * Tokens: "|", "<", ">", ">>", ">&", "&", "n>", "a>&b", "!n", "\"*\"", and
 * words composed of characters that satisfy valid_strchr
 */
int tokenize_input(const char *inp, token outp[]) {
    int tidx = 0;
    int inpdx = 0;
    token g = {EMPTY, {0}};
    token *t = &g;
    int toklen;
    char c;
    while ((c = inp[inpdx]) != '\0') {
        switch (c) {
        /* Single char tokens: make a one byte slice pointing to the current
         * location
         */
        case ' ':
            ++inpdx;
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
                return -1;
            } else {
                outp[tidx++].type = CHOUT;
            }
            break;
        /* Quoted string: continue taking until closed. */
        case '"':
            toklen = 0;
            while ((c = inp[inpdx + 1 + toklen]) != '"' && c != '\0')
                ++toklen;
            if (c == '\0') {
                fprintf(stderr, "Unclosed double quote\n");
                return -1;
            }
            char *st = (char *) malloc((toklen + 1)* sizeof(char));
            if (!st) {
                fprintf(stderr, "Error allocating memory. Aborting.\n");
                exit(0);
            }

            /* Copy the contents of the string inside the double quotes
             * into st. With strncpy, we have to manually add the terminating
             * null char.
             */
            strncpy(st, inp + inpdx + 1, toklen);
            st[toklen] = '\0';

            assert(strlen(st) == (unsigned long) toklen);
            assert(st[0] != '"');
            assert(st[0] == inp[inpdx + 1]);
            assert(st[strlen(st) - 1] != '"');

            outp[tidx].type = STRING,
            outp[tidx++].data.str = st;
            /* +2 for the '"' characters around the token. */
            inpdx += toklen + 2;
            break;
        case '!':
            ++inpdx;
            int n = 0;
            /* I'm not using atoi because
             * a) there should not be any spaces after the '!' or a +/- sign
             * b) because the length is needed to properly move through
             * the input.
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
            if ((toklen = match_gen_bi_redirect(inp + inpdx, t)) != -1) {
                outp[tidx++] = *t;
                inpdx += toklen;
            } else if ((toklen = match_gen_redirect(inp + inpdx,  t)) != -1) {
                outp[tidx++] = *t;
                inpdx += toklen;
            } else {
                toklen = 1;
                while (valid_strchr(inp[inpdx + toklen]))
                    ++toklen;
                char *st = (char *) malloc((toklen + 1) * sizeof(char));
                if (!st) {
                    fprintf(stderr, "Error allocating memory. Aborting.\n");
                    exit(0);
                }

                strncpy(st, inp + inpdx, toklen);
                st[toklen] = '\0';

                outp[tidx].type = STRING;
                outp[tidx++].data.str = st;
                inpdx += toklen;
            }
        }
    }
    outp[tidx].type = EMPTY;
    return tidx;
}

/* Free any of the strings that have been malloc'd */
void free_token_list(token freeable[]) {
    for (int i = 0; i < MAXTOKENS && freeable[i].type != EMPTY; i++) {
        if (freeable[i].type == STRING) {
            free(freeable[i].data.str);
        }
    }
}
