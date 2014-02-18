#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"

/* Macros to help with arg checking. */
#define check_args_1(ptr, type) \
    (mem_valid(ptr, sizeof(type)))
#define check_args_2(ptr, type1, type2) \
    (mem_valid(ptr, sizeof(type1)) && \
     mem_valid(ptr + sizeof(type1), sizeof(type2)))

static void syscall_handler(struct intr_frame *);

/* System calls. */
void sys_halt();
void sys_exit(int status);
pid_t sys_exec(const char *cmd_line);
int sys_wait(pid_t pid);
bool sys_create(const char *file, unsigned int initial_size);
bool sys_remove(const char *file);
int sys_open(const char *file);
int sys_filesize(int fd);
int sys_read(int fd, void *buffer, unsigned int size);
int sys_write(int fd, const void *buffer, unsigned size);
void sys_seek(int fd, unsigned int position);
unsigned int sys_tell(int fd);
void sys_close(int fd);

/* Checks if memory address is valid. */
bool mem_valid(const void *addr, int size);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    void *esp = f->esp;
    void *args;

    /* Check pointer validity */
    if (!mem_valid(esp, sizeof(int))) {
        thread_exit();
        return;
    }

    int syscall_nr = *((int *) esp);
    printf("syscall: %d\n", syscall_nr);
    args = esp + sizeof(int);

    // Check that all args are within user memory and call the
    // appropriate handler.
    switch (syscall_nr)
    {
    case SYS_HALT:
        break;
    case SYS_EXIT:
        break;
    case SYS_EXEC:
        break;
    case SYS_WAIT:
        break;
    case SYS_CREATE:
        break;
    case SYS_REMOVE:
        break;
    case SYS_OPEN:
        break;
    case SYS_FILESIZE:
        break;
    case SYS_READ:
        break;
    case SYS_WRITE:
        break;
    case SYS_SEEK:
        break;
    case SYS_TELL:
        break;
    case SYS_CLOSE:
        break;
    default:
        break;
    }
    return;
}

/* Checks that addr points into user space on a currently mapped page.
 * We check the addr and the byte just before addr + size. So if we
 * wanted to check an integer at addr, we check addr and addr + 3 to
 * make sure both are valid.
 */
bool mem_valid(const void *addr, int size)
{
    return (is_user_vaddr(addr) &&
            pagedir_get_page(thread_current()->pagedir, addr) != NULL &&
            is_user_vaddr(addr + size - 1) &&
            pagedir_get_page(thread_current()->pagedir,
                addr + size - 1) != NULL);
}
