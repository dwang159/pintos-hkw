#include "execute.h"

/*
 * Takes an array of command structs and executes them. Expects the last
 * element to be CMDBLANK (defined in command.h).
 */
void execute_commands(command *cmds) {
    int filedes[2];
    int i;

    /* Iterate through all of the commands and set up the pipes. We iterate
     * through the first n-1 commands and set up a pipe between the ith and
     * (i+1)th command. If the user had set up something strange like this:
     *
     * cat myfile > outfile | grep hello < myotherfile
     *
     * This will overwrite the stdin/stdout redirection.
     */
    for (i = 0; cmds[i + 1].argv_cmds != NULL; i++) {
        if (pipe(filedes) == 0) {
            /* Set the 'out' filedes for this command to the write
             * end of the pipe and set the 'in' filedes for the next
             * command to the read end of the pipe.
             */
            cmds[i].filedes_out = filedes[1];
            cmds[i + 1].filedes_in = filedes[0];
        } else {
            /* pipe() failed. */
            fprintf(stderr, "error: pipe() failed\n");
            exit(1);
        }
    }

    /* Iterate through all of the commands and execute them appropriately. */
    for (i = 0; cmds[i].argv_cmds != NULL; i++) {
        pid_t pid = fork();

        /* The child process executes the command. */
        if (pid == 0) {
            /* Change stdin, stdout, and stderr to the provided values. If
             * the filedes_in/out/err values are the default values (
             * STDIN/OUT/ERR_FILENO), then nothing happens.
             */
            if (dup2(cmds[i].filedes_in, STDIN_FILENO) == -1 ||
                dup2(cmds[i].filedes_out, STDOUT_FILENO) == -1 ||
                dup2(cmds[i].filedes_err, STDERR_FILENO) == -1) {
                /* dup2 failed */
                fprintf(stderr, "error: dup2() failed\n");
                exit(1);
            }

            /* Execute the command. */
            if (execvp(cmds[i].argv_cmds[0], cmds[i].argv_cmds)) {
                /* If execvp returns then an error occurred. */
                fprintf(stderr, "error: execvp() failed\n");
                exit(1);
            }
        } else {
            /* Parent process */
            int status;

            /* Wait for the child to terminate. */
            waitpid(pid, &status, 0);
        }
    }
}
