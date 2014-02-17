#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

/* System calls. */
void sys_halt(void);
void sys_exit(int status);
pid_t sys_exec(const char *cmd_line);
int sys_wait(pid_t pid);
bool sys_create(const char *file, unsigned int initial_size);
bool sys_remove(const char *file);
int sys_open(const char *file);
int sys_filesize(int fd);
int sys_read(int fd, void *buffer, unsigned int size);
int sys_write(int fd, const void *buffer, unsigned int size);
void sys_seek(int fd, unsigned int position);
unsigned int sys_tell(int fd);
void sys_close(int fd);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    void *esp = f->esp;
    int syscall_nr = *((int *) esp);

    switch (syscall_nr)
    {
    case SYS_HALT:
        printf("SYS_HALT\n");
        break;
    case SYS_EXIT:
        printf("SYS_EXIT\n");
        break;
    case SYS_EXEC:
        printf("SYS_EXEC\n");
        break;
    case SYS_WAIT:
        printf("SYS_WAIT\n");
        break;
    case SYS_CREATE:
        printf("SYS_CREATE\n");
        break;
    case SYS_REMOVE:
        printf("SYS_REMOVE\n");
        break;
    case SYS_OPEN:
        printf("SYS_OPEN\n");
        break;
    case SYS_FILESIZE:
        printf("SYS_FILESIZE\n");
        break;
    case SYS_READ:
        printf("SYS_READ\n");
        break;
    case SYS_WRITE:
        printf("SYS_WRITE\n");
        break;
    case SYS_SEEK:
        printf("SYS_SEEK\n");
        break;
    case SYS_TELL:
        printf("SYS_TELL\n");
        break;
    case SYS_CLOSE:
        printf("SYS_CLOSE\n");
        break;
    default:
        printf("INVALID\n");
        break;
    }
    thread_exit();
}
