#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

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
