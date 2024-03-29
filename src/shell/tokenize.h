#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

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

/* Printing for debugging */
void print_token(const token t);
void print_token_list(const token ts[]);

/* Equality for testing */
int eq_token(const token a, const token b);
int eq_token_list(const token a[], const token b[]);

/* Functions for parsing. The match functions are used to
 * classify general strings, and valid_strchr is used to
 * determine what is a valid character for a string that
 * isn't surrounded by '"'s. Exported for testing.
 */
int match_gen_redirect(const char *inp, token *t);
int match_gen_bi_redirect(const char *inp, token *t);
int valid_strchr(char c);

/* The only functions that should be actually called by
 * other non-testing modules. inp is the input string,
 * outp is provided as a buffer to put the output in,
 * and the return value is the number of tokens found.
 */
int tokenize_input(const char *inp, token outp[]);

/* Cleans up any memory used by the token list */
void free_token_list(token[]);
#endif
