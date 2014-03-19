#include "threads/thread.h"
#include "lib/kernel/vector.h"
#include <stdio.h>
#include "userprog/filedes.h"
#include "userprog/syscall.h"

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

void fd_clear(int fd) {
    thread_current()->files.data[fd] = NULL;
}

void fd_init(void) {
    struct thread *curr = thread_current();
    vector_init(&curr->files);
    vector_zeros(&curr->files, STDOUT_FILENO + 1);
}
void fd_destruct(void) {
    unsigned int i;
    struct thread *curr = thread_current();
    for (i = STDOUT_FILENO + 1; i < curr->files.size; i++)
    {
        if (curr->files.data[i] != NULL)
            sys_close(i);
    }
    vector_destruct(&curr->files);
}
