#include "command.h"

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
    int length = 0;
    int tdx = 0;
    /* Keeps track of if we found the 'start' of a command. */
    int cmd_start = false;
    for (tdx = 0; tdx < MAXTOKENS && tkns[tdx].type != EMPTY; ++tdx) {
        if (tkns[tdx].type == STRING && !cmd_start) {
            /* Found a new start to a command. */
            cmd_start = true;
            length++;
        }
        else if (tkns[tdx].type == PIPE) {
            cmd_start = false;
        }
    }
    if (length == 0) {
        /* No commands. */
        return NULL;
    }
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
    int n;
    int errors = false;
    for (tdx = 0; tdx < MAXTOKENS && tkns[tdx].type != EMPTY; ++tdx) {
        if (errors)
            break;
        switch(tkns[tdx].type) {
        /* For the redirects, find the appropriate filedescriptor with
         * open and add it to the command struct
         */
        case CHINP:
            /* After a < token, the next token must be a string. */
            if (tkns[tdx + 1].type != STRING) {
                fprintf(stderr, "error: expected filename after < token\n");
                errors = true;
                break;
            }
            filename = tkns[tdx + 1].data.str;
            ret[retdx].filedes_in = open(filename, O_RDONLY);
            tdx ++;
            break;
        case CHOUT:
            /* After a > token, the next token must be a string. */
            if (tkns[tdx + 1].type != STRING) {
                fprintf(stderr, "error: expected filename after > token\n");
                errors = true;
                break;
            }
            filename = tkns[tdx + 1].data.str;
            fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 
                    S_IRUSR | S_IWUSR);
            ret[retdx].filedes_out = fd;
            tdx ++;
            break;
        /* If a PIPE is encountered, the current command is finished. Add
         * a NULL to satisfy execlp */
        case PIPE:
            /* There must be at least one command before a pipe. */
            if (ardx == 0) {
                fprintf(stderr, "error: expected command before pipe\n");
                errors = true;
                break;
            }
            /* There must be a command immediately following the pipe. */
            if (tkns[tdx + 1].type != STRING) {
                fprintf(stderr, "error: expected command after pipe\n");
                errors = true;
                break;
            }
            /* Set last argv to NULL for the previous command. */
            ret[retdx].argv_cmds[ardx] = NULL;
            /* Initialize the next command. */
            ret[++retdx] = CMDBLANK;
            /* Indexing into a new argv for the next command. */
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
            /* The >> must be followed by a string. */
            if (tkns[tdx + 1].type != STRING) {
                fprintf(stderr, "error: expected filename after >> token\n");
                errors = true;
                break;
            }
            filename = tkns[tdx + 1].data.str;
            fd = open(filename, O_WRONLY | O_APPEND | O_CREAT,
                    S_IRUSR | S_IWUSR);
            ret[retdx].filedes_out = fd;
            tdx ++;
            break;
        case BACKGROUND:
            /* TODO */
            printf("Dont come here\n");
            fprintf(stderr, "&: Not implemented\n");
            return NULL;
        case GENOUTRED:
            assert(tkns[tdx + 1].type == STRING);
            filename = tkns[tdx + 1].data.str;
            fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT,
                    S_IRUSR | S_IWUSR);
            if (dup2(fd, tkns[tdx].data.onefiledes) < 1) {
                fprintf(stderr, "dup2 failed.\n");
                return NULL;
            }
            tdx ++;
            break;
        case GENINOUTRED:
            if(dup2(tkns[tdx].data.filedespair[0],
                    tkns[tdx].data.filedespair[1]) < 0) {
                fprintf(stderr, "dup2 failed.\n");
                return NULL;
            }
            break;
        case RERUN:
            n = tkns[tdx].data.last;
            char *cmd = history_get(n)->line;
            token tkns[MAXTOKENS];
            tokenize_input(cmd, tkns);
            command *cms = separate_commands(tkns);
            n = 0;
            while (cms[n].argv_cmds != NULL) {
                ret[retdx++] = cms[n++];
            }
            if (n > 0) {
                ret[retdx] = CMDBLANK;
                ardx = 0;
            };
            if(cms) {
                free(cms);
            }
            break;
        default:
            /* This should never happen. */
            fprintf(stderr, "what just happened?\n");
        }
    }
    if (errors)
       return NULL;

    if (ret[retdx].argv_cmds && ret[retdx].argv_cmds[ardx] != NULL) {
        ret[retdx].argv_cmds[ardx] = NULL;
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

/* Reclaim the memory used by the arrays of commands for */
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
    //free(freeable);
}

/* Useful for debugging */
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
