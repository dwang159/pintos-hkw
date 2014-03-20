#include "threads/thread.h"
#include "threads/malloc.h"
#include "lib/kernel/vector.h"
#include <stdio.h>
#include "userprog/filedes.h"
#include "userprog/syscall.h"

struct fileordir {
    bool is_dir;
    union {
        struct file *f;
        struct dir *d;
    };
};

static int insert(void *, bool);

int fd_insert_file(struct file *f) {
    return insert(f, false);
}

int fd_insert_dir(struct dir *d) {
    return insert(d, true);
}

int insert(void *payload, bool is_dir) {
    struct thread *curr = thread_current();

    /* Insert file into first non-null entry of file descriptor
     * table. Return the index where the file was inserted
     */
    unsigned i;
    struct fileordir *fod = malloc(sizeof(struct fileordir));
    ASSERT(fod);
    ASSERT(payload);
    fod->is_dir = is_dir;
    if (is_dir) {
        fod->d = payload;
    } else {
        fod->f = payload;
    }
    for (i = STDOUT_FILENO + 1; i < curr->files.size; i++) {
        if (curr->files.data[i] == NULL) {
            curr->files.data[i] = fod;
            return i;
        }
    }
    /* If all current values are non-null, append */
    vector_append(&curr->files, fod);
    return (curr->files.size - 1);
}


struct file *fd_lookup_file(int fd) {
    struct fileordir *fod = thread_current()->files.data[fd];
    ASSERT(fod);
    if (fod->is_dir) {
        return NULL;
    } else {
        return fod->f;
    }
}

struct dir *fd_lookup_dir(int fd) {
    struct fileordir *fod = thread_current()->files.data[fd];
 //   if (!fod) {
 //       printf("not found: %d\n", fd);
 //       ASSERT(fod);
 //   }
    if (fod && fod->is_dir) {
        return fod->d;
    } else {
        return NULL;
    }
}

void fd_clear(int fd) {
    struct vector v = thread_current()->files;
    free(v.data[fd]);
    v.data[fd] = NULL;
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
        if (curr->files.data[i] != NULL) {
            sys_close(i);
            fd_clear(i);
        }
    }
    vector_destruct(&curr->files);
}

/* Checks if the file descriptor is valid. */
bool fd_valid(int fd) {
    struct thread *curr = thread_current();
    // File pointer should not be null unless it is STDIN or STDOUT.
    if (fd != STDIN_FILENO && fd != STDOUT_FILENO)
        if ((unsigned) fd >= curr->files.size ||
            curr->files.data[fd] == NULL) {
            return false;
        }
    return true;
}
