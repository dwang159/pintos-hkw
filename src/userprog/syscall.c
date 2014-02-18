#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"

/* Macros to help with arg checking. Checks the pointer to the args
 * and the byte just before the end of the last arg.
 */
#define check_args_1(ptr, type) \
    (mem_valid((void *) ptr) && mem_valid((void *) ptr + sizeof(type) - 1))
#define check_args_2(ptr, type1, type2) \
    (mem_valid((void *) ptr) && \
     mem_valid((void *) ptr + sizeof(type1) + sizeof(type2) - 1))
#define check_args_3(ptr, type1, type2, type3) \
    (mem_valid((void *) ptr) && \
     mem_valid((void *) ptr + sizeof(type1) + sizeof(type2) + sizeof(type3) \
         - 1))

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
bool mem_valid(const void *addr);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    void *esp = f->esp;
    void *args;
    bool args_valid = true;

    /* Check pointer validity */
    if (!check_args_1(esp, int)) {
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
        sys_halt();
        break;
    case SYS_EXIT:
        if (check_args_1(args, int))
            sys_exit(*((int *) args));
        else
            args_valid = false;
        break;
    case SYS_EXEC:
        if (check_args_1(args, char *))
            sys_exec(*((char **) args));
        else
            args_valid = false;
        break;
    case SYS_WAIT:
        if (check_args_1(args, pid_t))
            sys_wait(*((pid_t *) args));
        else
            args_valid = false;
        break;
    case SYS_CREATE:
        if (check_args_2(args, char *, unsigned))
            sys_create(*((char **) args),
                    *((unsigned *)(args + sizeof(char *))));
        else
            args_valid = false;
        break;
    case SYS_REMOVE:
        if (check_args_1(args, char *))
            sys_remove(*((char **) args));
        else
            args_valid = false;
        break;
    case SYS_OPEN:
        if (check_args_1(args, char *))
            sys_remove(*((char **) args));
        else
            args_valid = false;
        break;
    case SYS_FILESIZE:
        if (check_args_1(args, int))
            sys_remove(*((int *) args));
        else
            args_valid = false;
        break;
    case SYS_READ:
        if (check_args_2)
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
bool mem_valid(const void *addr)
{
    return (addr != NULL && is_user_vaddr(addr) &&
            pagedir_get_page(thread_current()->pagedir, addr) != NULL);
}
