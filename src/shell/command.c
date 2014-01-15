#include "command.h"

void print_string_list(const char **ss) {
    char *st;
    int i = 0;
    while ((st = *ss++)) {
        printf("%d %s \n", i++, st);
    }
}
/* Takes a list of tokens and creates a list of commands.
 * Commands are separated by PIPEs, and have knowledge 
 * of their own I/O redirection. It is at this time that
 * filenames for I/O redirection are opened: they need 
 * to be opened before processes start to get forked so
 * that all children agree on file descriptors, and this
 * gives a more uniform look to tho command struct rather
 * than "maybe a string, maybe a file descriptor. Who knows?".
 */
command *separate_commands(const token tkns[]) {
    /* Determine number of commands */
    int length;
    for (length = 0; length < MAXTOKENS && tkns[length].type != EMPTY; ++length)
        ;
    command *ret;
    ret = (command *)malloc((length + 1) * sizeof(command));
    if (!ret) {
        fprintf(stderr, "Error allocating memory. Aborting.\n");
        exit(1);
    }
    int retdx = 0; 
    char *filename;
    int pair[2];
    int ardx = 0;
    for (int tdx = 0; tdx < MAXTOKENS && tkns[tdx].type != EMPTY; ++tdx) {
        switch(tkns[tdx].type) {
        /* For the redirects, find the appropriate filedescriptor with
         * open and add it to the command struct */
        case CHINP:
            printf("Dont come here\n");
            assert(tkns[tdx + 1].type == STRING);
            filename = tkns[tdx + 1].data.str;
            ret[retdx].filedes_in = open(filename, O_RDONLY);;
            tdx += 2;
            break;
        case CHOUT:
            printf("Dont come here\n");
            assert(tkns[tdx + 1].type == STRING);
            filename = tkns[tdx + 1].data.str;
            /* TODO: change this if we'd rather truncate than only
             * only write to new files. */
            ret[retdx].filedes_out = open(filename,
                                    O_WRONLY | O_TRUNC,
                                    S_IWUSR) ;
            tdx += 2;
            break;
        /* If a PIPE is encountered, the current command is finished. Add
         * a NULL to satisfy execlp */
        case PIPE:
            printf("Dont come here\n");
            printf("Found a pipe\n");
            assert(ardx > 0); /* Ensure at least one command */
            ret[retdx].argv_cmds[ardx] = NULL;
            ardx = 0;
            ++tdx;
            ++retdx;
            break;
        case STRING:
            if (ardx == 0) {
                printf("Mallocing!\n");
                char **p = (char **)malloc(sizeof(char *) * MAX_COMMAND_SIZE);
                if (!p) {
                    fprintf(stderr, "Error allocating memory. Aborting\n");
                    exit(1);
                }
                ret[retdx].argv_cmds = p;
            }
            ret[retdx].argv_cmds[ardx++] = strndup(tkns[tdx].data.str, MAXLINE);
            break;
        case CHOUTAPP:
            printf("Dont come here\n");
            assert(tkns[tdx + 1].type == STRING);
            filename = tkns[tdx + 1].data.str;
            ret[retdx].filedes_out = open(filename, O_WRONLY | O_APPEND);
            tdx += 2;
            break;
        /* TODO: this is pretty shady and needs to be debugged. */
        case BACKGROUND:
            printf("Dont come here\n");
            fprintf(stderr, "&: Not implemented\n");
            return NULL;
        case GENOUTRED:
            printf("Dont come here\n");
            assert(tkns[tdx + 1].type == STRING);
            filename = tkns[tdx + 1].data.str;
            /* TODO: this is not how pipe works! */
            pair[0] = open(filename, O_WRONLY | O_TRUNC);
            pair[1] = tkns[tdx].data.onefiledes;
            if (!pipe(pair)) {
                fprintf(stderr, "Could not create pipe\n");
                return NULL;
            }
            tdx += 2;
            break;
        case GENINOUTRED:
            printf("Dont come here\n");
            /* TODO: this is not how pipe works! */
            pair[0] = tkns[tdx].data.filedespair[0];
            pair[1] = tkns[tdx].data.filedespair[1];
            if (!pipe(pair)) {
                fprintf(stderr, "Could not create pipe\n");
                return NULL;
            }
            ++tdx;
            break;
        case RERUN:
            printf("Dont come here\n");
            fprintf(stderr, "!n: Not implemented\n");
            return NULL;
        default:
            assert(false);
        }
    }
    if (ret[retdx].argv_cmds && ret[retdx].argv_cmds[ardx] != NULL) {
        printf(" you dont know the model");
        ret[retdx].argv_cmds[ardx] = NULL;
    }
    ret[retdx + 1] = CMDBLANK;
    return ret;
}

int eq_command(const command a, const command b) {
    int i;
    printf("EQ\n");
    for (i = 0;  a.argv_cmds[i]; i++) {
        printf("dont say %s\n", a.argv_cmds[i]);
    }
    for (i = 0; b.argv_cmds[i]; i++) {
        printf("say %s\n", b.argv_cmds[i]);
    }
    printf("i is now really %d\n", i);
    for (i = 0; a.argv_cmds[i] && b.argv_cmds[i]; i++) {
        if (strcmp(a.argv_cmds[i], b.argv_cmds[i]))
            return false;
    }
    if (b.argv_cmds[i])  
        return false;
    else
        return true;
}

void free_command_list(command *freeable) {
    char **args;
    char **iter;
    char *one_arg;
    while ((args = (freeable++)->argv_cmds)) {
        iter = args;
        while ((one_arg = *iter++)) {
            free(one_arg);
        }
        free(args);
    }
    free(freeable);
}


void print_command_list(const command *freeable) {
    char **args;
    char *one_arg;
    printf("Args:\n");
    while ((args = (freeable++)->argv_cmds)) {
        printf("    cmd: ");
        while ((one_arg = *args++)) {
            printf("%s ", one_arg);
        }
        printf("\n");
    }
}

const command CMDBLANK = {NULL, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};

