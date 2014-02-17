#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.c"

static void syscall_handler(struct intr_frame *);

/* System calls. */
void sys_halt(const void **args);
void sys_exit(const void **args);
tid_t sys_exec(const void **args);
int sys_wait(const void **args);
bool sys_create(const void **args);
bool sys_remove(const void **args);
int sys_open(const void **args);
int sys_filesize(const void **args);
int sys_read(const void **args);
int sys_write(const void **args);
void sys_seek(const void **args);
unsigned int sys_tell(const void **args);
void sys_close(const void **args);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    void *esp = f->esp;

    /* Check pointer validity */
    if (!mem_valid(esp)) {
        thread_exit();
        return;
    }

    int syscall_nr = *((int *) esp);
    esp += sizeof(int);

    switch (syscall_nr)
    {
    case SYS_HALT:
        printf("SYS_HALT\n");
        sys_halt(esp);
        break;
    case SYS_EXIT:
        printf("SYS_EXIT\n");
        sys_exit(esp);
        break;
    case SYS_EXEC:
        printf("SYS_EXEC\n");
        sys_exec(esp);
        break;
    case SYS_WAIT:
        printf("SYS_WAIT\n");
        sys_wait(esp);
        break;
    case SYS_CREATE:
        printf("SYS_CREATE\n");
        sys_create(esp);
        break;
    case SYS_REMOVE:
        printf("SYS_REMOVE\n");
        sys_remove(esp);
        break;
    case SYS_OPEN:
        printf("SYS_OPEN\n");
        sys_open(esp);
        break;
    case SYS_FILESIZE:
        printf("SYS_FILESIZE\n");
        sys_filesize(esp);
        break;
    case SYS_READ:
        printf("SYS_READ\n");
        sys_read(esp);
        break;
    case SYS_WRITE:
        printf("SYS_WRITE\n");
        sys_write(esp);
        break;
    case SYS_SEEK:
        printf("SYS_SEEK\n");
        sys_seek(esp);
        break;
    case SYS_TELL:
        printf("SYS_TELL\n");
        sys_tell(esp);
        break;
    case SYS_CLOSE:
        printf("SYS_CLOSE\n");
        sys_close(esp);
        break;
    default:
        printf("INVALID\n");
        thread_exit();
        break;
    }
    return;
}

/* Checks that addr points into user space on a currently mapped page */
bool mem_valid(const void *addr)
{
    return (is_user_vaddr(addr) &&
                    lookup_page(thread_current()->pagedire, addr, false) != NULL);
}

/* Terminates Pintos */
void sys_halt(const void **args) {
    shutdown_power_off();
}

/* Terminates current user program and returns status to kernel */
void sys_exit(const void **args) {
    if (!mem_valid(*args)) {
        thread_exit();
        return;
    }
}

/* Runs the executable with the given name, returns the new process's pid */
pid_t sys_exec(const void **args) {
    if (!mem_valid(*args)) {
        thread_exit();
        return;
    }
}

/* Waits for a child process and retrieves its exit status */
int sys_wait(const void **args) {
    if (!mem_valid(*args)) {
        thread_exit();
        return;
    }
}

/* Creates a new file of given. Returns true if successful, false otherwise */
bool sys_create(const void **args) {
    if (!mem_valid((*args) + sizeof(file *))) {
        thread_exit();
        return;
    }

/* Deletes the specified file */
bool sys_remove(const void **args) {
    if (!mem_valid(*args)) {
        thread_exit();
        return;
    }
}

/* Opens the specified file, returns a file descriptor*/
int sys_open(const void **args) {
    if (!mem_valid(*args)) {
        thread_exit();
        return;
    }
}

/* Returns size in bytes of specified file */
int sys_filesize(const void **args) {
    if (!mem_valid(*args)) {
        thread_exit();
        return;
    }
}

/* Read bytes from buffer to open file. Returns number of bytes read */
int sys_read(const void **args) {
    if (!mem_valid((*args) + sizeof(void *) + sizeof(unsigned))) {
        thread_exit();
        return;
    }
}

/* Writes bytes from buffer to file. Returns number of bytes written */
int sys_write(const void **args) {
    if (!mem_valid((*args) + sizeof(void *) + sizeof(unsigned))) {
        thread_exit();
        return;
    }
}

/* Changes the next byte to be read or written in open file */
void sys_seek(const void **args) {
    if (!mem_valid((*args) + sizeof(unsigned))) {
        thread_exit();
        return;
    }
}

/* Returns the postition of the next byte to be read or written in open file */
unsigned int sys_tell(const void **args) {
if (!mem_valid(*args)) {
        thread_exit();
        return;
    }
}

/* Closes given file descriptor */
void sys_close(const void **args) {
    if (!mem_valid(*args)) {
        thread_exit();
        return;
    }
}
