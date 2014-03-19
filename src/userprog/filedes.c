#include "threads/thread.h"
#include "lib/kernel/vector.h"
#include <stdio.h>
#include "userprog/filedes.h"

int fd_insert_file(struct file *f) {
    struct thread *curr = thread_current();

    /* Insert file into first non-null entry of file descriptor
     * table. Return the index where the file was inserted
     */
    unsigned i;
    for (i = STDOUT_FILENO + 1; i < curr->files.size; i++) {
        if (curr->files.data[i] == NULL) {
            curr->files.data[i] = f;
            return i;
        }
    }
    /* If all current values are non-null, append */
    vector_append(&curr->files, f);
    return (curr->files.size - 1);
}

struct file *fd_lookup_file(int fd) {
    return thread_current()->files.data[fd];
}
