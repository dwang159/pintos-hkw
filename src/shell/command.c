#include "command.h"

void print_string_list(char **ss) {
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
    ret[0] = CMDBLANK;
    int retdx = 0; 
    char *filename;
    int ardx = 0;
    int fd;
    for (int tdx = 0; tdx < MAXTOKENS && tkns[tdx].type != EMPTY; ++tdx) {
        switch(tkns[tdx].type) {
        /* For the redirects, find the appropriate filedescriptor with
         * open and add it to the command struct */
        case CHINP:
            assert(tkns[tdx + 1].type == STRING);
            filename = tkns[tdx + 1].data.str;
            ret[retdx].filedes_in = open(filename, O_RDONLY);;
            tdx += 2;
            break;
        case CHOUT:
            assert(tkns[tdx + 1].type == STRING);
            filename = tkns[tdx + 1].data.str;
            /* TODO: change this if we'd rather truncate than only
             * only write to new files. */
            fd = open(filename, O_WRONLY | O_TRUNC);
            ret[retdx].filedes_out = fd;
            tdx += 2;
            break;
        /* If a PIPE is encountered, the current command is finished. Add
         * a NULL to satisfy execlp */
        case PIPE:
            assert(ardx > 0); /* Ensure at least one command */
            ret[retdx].argv_cmds[ardx] = NULL;
            /* Initialize the next command */
            ret[++retdx] = CMDBLANK;
            /* Indexing into a new argv_cmds */
            ardx = 0;
            break;
        case STRING:
            /* If there are no strings in the current command, create
             * space for them */
            if (ardx == 0) {
                char **p = (char **)malloc(sizeof(char *) * MAX_COMMAND_SIZE);
                if (!p) {
                    fprintf(stderr, "Error allocating memory. Aborting\n");
                    exit(1);
                }
                ret[retdx].argv_cmds = p;
            }
            /* Append the string to argv_cmds */
            ret[retdx].argv_cmds[ardx++] = strndup(tkns[tdx].data.str, MAXLINE);
            break;
        case CHOUTAPP:
            assert(tkns[tdx + 1].type == STRING);
            filename = tkns[tdx + 1].data.str;
            fd = open(filename, O_WRONLY | O_APPEND);
            ret[retdx].filedes_out = fd;
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
            fd = open(filename, O_WRONLY | O_TRUNC);
            if (dup2(fd, tkns[tdx].data.onefiledes) < 1) {
                fprintf(stderr, "dup2 failed.\n");
                return NULL;;
            }
            tdx += 2;
            break;
        case GENINOUTRED:
            printf("Dont come here\n");
            /* TODO: this is not how pipe works! */
            if(dup2(tkns[tdx].data.filedespair[0], 
                    tkns[tdx].data.filedespair[1]) < 0) {
                fprintf(stderr, "dup2 failed.\n");
                return NULL;
            }
            break;
        case RERUN:
            printf("Dont come here\n");
            int n = tkns[tdx].data.last; 
            printf("N is %d\n", n);
            char *cmd = history_get(n)->line;
            printf("history: %s\n", cmd);
            token tkns[MAXTOKENS];
            tokenize_input(cmd, tkns);
            command *cms = separate_commands(tkns); 
            printf("internal: \n");
            print_command_list(cms);
            printf("external\n");
            fprintf(stderr, "!n: Not implemented\n");
            return NULL;
        default:
            assert(false);
        }
    }
    if (ret[retdx].argv_cmds && ret[retdx].argv_cmds[ardx] != NULL) {
        printf(" you dont know the model");
        assert(false);
        ret[retdx].argv_cmds[ardx] = NULL;
    }
    if (!ret[retdx].argv_cmds) {
        printf("empty list\n");
    }
    ret[retdx + 1] = CMDBLANK;
    return ret;
}

/* Checks if two commands have the same argv lists */
int eq_command(const command a, const command b) {
    int i;
    for (i = 0; a.argv_cmds[i]; i++) {
        if (strcmp(a.argv_cmds[i], b.argv_cmds[i]))
            return false;
    }
    if (b.argv_cmds[i])  
        return false;
    else
        return true;
}

/* Reclaim the memory used by the arrays of commands for
 */
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
    if (freeable) {
        while (freeable && (args = freeable->argv_cmds)) {
            printf("    cmd: ");
            while ((one_arg = *args++)) {
                printf("%s, ", one_arg);
            }
            printf("i/o/e des: %d %d %d", freeable->filedes_in, 
                freeable->filedes_out, freeable-> filedes_err);
            printf("\n");
            freeable++;
        }
    } else
        printf("<no commands>\n");
}

const command CMDBLANK = {NULL, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
