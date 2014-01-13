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
command *separate_commands(token tkns[]) {
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
    int j = 0; 
    char *filename;
    for (int i = 0; i < MAXTOKENS && tkns[i].type != EMPTY; ++i) {
        int k = 0;
        switch(tkns[i].type) {
        case CHINP:
            assert(tkns[i+1].type == STRING);
            filename = tkns[i + 1].data.str;
            ret[j].filedes_in = open(filename, O_RDONLY);;
            i += 2;
            break;
        case CHOUT:
            assert(tkns[i+1].type == STRING);
            filename = tkns[i + 1].data.str;
            /* TODO: change this if we'd rather truncate than only
             * only write to new files. */
            ret[j].filedes_out = open(filename,
                                    O_WRONLY | O_CREAT | O_EXCL,
                                    S_IWUSR) ;
            i += 2;
            break;
        case PIPE:
            assert(!ret[j].argv_cmds[0]); /* Ensure at least one command */
            ++i;
            ++j;
            break;
        case STRING:
            ret[j].argv_cmds[k++] = tkns[i++].data.str;
            break;
        case CHOUTAPP:
            assert(tkns[i + 1].type == STRING);
            filename = tkns[i + 1].data.str;
            ret[j].filedes_out = open(filename, O_WRONLY | O_APPEND);
            i += 2;
            break;
        /* TODO: this is pretty shady and needs to be debugged. */
        case BACKGROUND:
            fprintf(stderr, "&: Not implemented\n");
            return NULL;
        case GENOUTRED:
            assert(tkns[i + 1].type == STRING);
            filename = tkns[i + 1].data.str;
            int output = open(filename, O_WRONLY | O_CREAT | O_EXCL);
            int pair[2] = {output, tkns[i].data.onefiledes};
            if (!pipe(pair)) {
                fprintf(stderr, "Could not create pipe\n");
                return NULL;
            }
            i += 2;
            break;
        case GENINOUTRED:
            if (!pipe(tkns[i].data.filedespair)) {
                fprintf(stderr, "Could not create pipe\n");
                return NULL;
            }
            ++i;
            break;
        case RERUN:
            fprintf(stderr, "!n: Not implemented\n");
            return NULL;
        default:
            assert(false);
        }
    }
    ret[j] = CMDBLANK;
    return ret;
}

                     


                         

      

