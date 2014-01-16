#include "execute.h"

/*
 * Calls dup2 to overwrite stdin, stdout, stderr. Returns true on success
 * and false if errors are encountered. If the provied in/out/err values
 * are the same as STDIN/OUT/ERR_FILENO, then nothing happens.
 */
int dup2_stdfiles(int in, int out, int err) {
    if (dup2(in, STDIN_FILENO) < 0 ||
            dup2(out, STDOUT_FILENO) < 0 ||
            dup2(err, STDERR_FILENO) < 0)
        return false;
    return true;
}

/*
 * Closes the provided files if they are not STDIN, STDOUT, or STDERR. This
 * is useful for closing pipes.
 */
void close_not_std(int in, int out, int err) {
    if (in != STDIN_FILENO)
        close(in);
    if (out != STDOUT_FILENO)
        close(out);
    if (err != STDERR_FILENO)
        close(err);
}

/*
 * Takes an array of command structs and executes them. Expects the last
 * element to be CMDBLANK (defined in command.h).
 */
void execute_commands(command *cmds) {
    int filedes[2];
    int i, n;

    /* Count the number of commands (n). We do not count the terminating
     * NULL command.
     */
    for (n = 0; cmds[n].argv_cmds != NULL; n++);

    /* Iterate through all of the commands and set up the pipes. We iterate
     * through the first n-1 commands and set up a pipe between the ith and
     * (i+1)th command. If the user had set up something strange like this:
     *
     * cat myfile > outfile | grep hello < myotherfile
     *
     * This will overwrite the stdin/stdout redirection.
     */
    for (i = 0; i < n - 1; i++) {
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
    /* Use dup to save a 'copy' of stdin, stdout, and stderr. This
     * is important so that if we change the stdin/out/err for the
     * shell process, we can restore it later.
     */
    int tmp_stdin, tmp_stdout, tmp_stderr;
    tmp_stdin = dup(STDIN_FILENO);
    tmp_stdout = dup(STDOUT_FILENO);
    tmp_stderr = dup(STDERR_FILENO);

    if (tmp_stdin < 0 || tmp_stdout < 0 || tmp_stderr < 0) {
        fprintf(stderr, "error: dup() failed\n");
        exit(1);
    }

    /* Iterate through all of the commands and execute them appropriately. */
    for (i = 0; i < n; i++) {
        /* Check if the command is an internal command, and if yes,
         * execute it properly.
         */
        if (strcmp("exit", cmds[i].argv_cmds[0]) == 0) {
            exec_exit();
        } else if (strcmp("cd", cmds[i].argv_cmds[0]) == 0) {
            /* Redirect std* files. */
            if (!dup2_stdfiles(cmds[i].filedes_in, cmds[i].filedes_out,
                    cmds[i].filedes_err)) {
                fprintf(stderr, "error: dup2() failed\n");
                exit(1);
            }
            exec_cd(&cmds[i]);

            /* Close all pipes/redirects, if any. */
            close_not_std(cmds[i].filedes_in, cmds[i].filedes_out,
                    cmds[i].filedes_err);

            /* Restore std* files. */
            if (!dup2_stdfiles(tmp_stdin, tmp_stdout, tmp_stderr)) {
                fprintf(stderr, "error: dup2() failed\n");
                exit(1);
            }
        } else if (strcmp("history", cmds[i].argv_cmds[0]) == 0) {
            /* Redirect std* files. */
            if (!dup2_stdfiles(cmds[i].filedes_in, cmds[i].filedes_out,
                    cmds[i].filedes_err)) {
                fprintf(stderr, "error: dup2() failed\n");
                exit(1);
            }
            exec_history();

            /* Close all pipes/redirects, if any. */
            close_not_std(cmds[i].filedes_in, cmds[i].filedes_out,
                    cmds[i].filedes_err);

            /* Restore std* files. */
            if (!dup2_stdfiles(tmp_stdin, tmp_stdout, tmp_stderr)) {
                fprintf(stderr, "error: dup2() failed\n");
                exit(1);
            }
        } else {
            pid_t pid = fork();

            /* The child process executes the command. */
            if (pid == 0) {
                if (!dup2_stdfiles(cmds[i].filedes_in, cmds[i].filedes_out,
                        cmds[i].filedes_err)) {
                    fprintf(stderr, "error: dup2() failed\n");
                    exit(1);
                }

                /* Execute the command. */
                if (execvp(cmds[i].argv_cmds[0], cmds[i].argv_cmds)) {
                    /* If execvp returns then an error occurred. */
                    fprintf(stderr, "error: could not find command %s\n",
                        cmds[i].argv_cmds[0]);
                }

                /* Terminate this child, because it should not reach this
                 * point if execvp was successfully called.
                 */
                exit(1);
            } else {
                /* Parent process */
                int status;

                /* Wait for the child to terminate. */
                waitpid(pid, &status, 0);

                close_not_std(cmds[i].filedes_in, cmds[i].filedes_out,
                        cmds[i].filedes_err);
            }
        }
    }

    /* If stdin/stdout/stderr were changed for the shell process (for example,
     * by running history | grep hello ), then we restore them now.
     */
    if (dup2(tmp_stdin, STDIN_FILENO) < 0 ||
            dup2(tmp_stdout, STDOUT_FILENO) < 0 ||
            dup2(tmp_stderr, STDERR_FILENO) < 0) {
        fprintf(stderr, "error: dup2() failed\n");
        exit(1);
    }

    /* Now close the temporary stdin/out/err files. */
    close(tmp_stdin);
    close(tmp_stdout);
    close(tmp_stderr);
}
