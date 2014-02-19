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
    int off1, off2;
    printf("syscall: %d\n", syscall_nr);
    args = esp + sizeof(int);

    // Check that all args are within user memory and call the
    // appropriate handler. When we call the handler, we dereference
    // args using the *((int *) args) notation instead of (int) args[0]
    // because even though it just happens that int, void *, etc are all
    // the same size, this works better for the general case.
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
            f->eax = (uint32_t) sys_exec(*((char **) args));
        else
            args_valid = false;
        break;
    case SYS_WAIT:
        if (check_args_1(args, pid_t))
            /* Put return value in %eax */
            f->eax = (uint32_t) sys_wait(*((pid_t *) args));
        else
            args_valid = false;
        break;
    case SYS_CREATE:
        if (check_args_2(args, char *, unsigned int)) {
            off1 = sizeof(char*);
            f->eax = (uint32_t) sys_create(*((char **) args),
                                *((unsigned int *)(args + off1)));
        } else
            args_valid = false;
        break;
    case SYS_REMOVE:
        if (check_args_1(args, char *))
            f->eax = (uint32_t) sys_remove(*((char **) args));
        else
            args_valid = false;
        break;
    case SYS_OPEN:
        if (check_args_1(args, char *))
            f->eax = (uint32_t) sys_open(*((char **) args));
        else
            args_valid = false;
        break;
    case SYS_FILESIZE:
        if (check_args_1(args, int))
            f->eax = (uint32_t) sys_filesize(*((int *) args));
        else
            args_valid = false;
        break;
    case SYS_READ:
        if (check_args_3(args, int, void *, unsigned int)) {
            off1 = sizeof(int);
            off2 = off1 + sizeof(void *);
            f->eax = (uint32_t) sys_read(*((int *) args), 
                              *((void **) (args + off1)),
                              *((unsigned int *) (args + off2)));
        } else
            args_valid = false;
        break;
    case SYS_WRITE:
        if (check_args_3(args, int, void *, unsigned int)) {
            off1 = sizeof(int);
            off2 = off1 + sizeof(void *);
            f->eax = (uint32_t) sys_write(*((int *) args), 
                               *((void **) (args + off1)),
                               *((unsigned int *) (args + off2)));
        } else
            args_valid = false;
        break;
    case SYS_SEEK:
        if (check_args_2(args, int, unsigned int)) {
            off1 = sizeof(int);
            sys_seek(*((int *) args),
                     *((unsigned int **) (args + off1)));
        } else
            args_valid = false;
        break;
    case SYS_TELL:
        if (check_args_1(args, int))
            f->eax = (uint32_t) sys_tell(*((int *) args));
        else
            args_valid = false;
        break;
    case SYS_CLOSE:
        if (check_args_1(args, int))
            sys_close(*((int *) args));
        else
            args_valid = false;
        break;
    default:
        args_valid = false;
        break;
    }

    if (!args_valid)
        sys_exit(-1);
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

/* Terminates Pintos. */
void sys_halt()
{
    shutdown_power_off();
}

/* Terminates the current process, returning status. */
void sys_exit(int status)
{
    return;
}

/* Runs the executable given by cmd_line. Returns the pid of
 * the process.
 */
pid_t sys_exec(const char *cmd_line)
{
    return 1;
}

/* Waits for child process to terminate, then returns the
 * returned status.
 */
int sys_wait(pid_t pid)
{
    // TODO
    return 1;
}

/* Creates a new file with initial_size bytes. Returns true on
 * success.
 */
bool sys_create(const char *file, unsigned int initial_size)
{
    // TODO
    return true;
}

/* Deletes the file called file. Returns true on success. */
bool sys_remove(const char *file)
{
    // TODO
    return true;
}

/* Opens the file called file. Returns the file descriptor or -1
 * on failure.
 * */
int sys_open(const char *file)
{
    // TODO
    return 1;
}

/* Returns the size of the file open, given the file descriptor. */
int sys_filesize(int fd)
{
    // TODO
    return 1;
}

/* Reads size bytes from the file fd into buffer. Returns the number
 * of bytes actually read.
 */
int sys_read(int fd, void *buffer, unsigned int size)
{
    // TODO
    return 1;
}

/* Writes size bytes from the buffer into the open file fd. Returns
 * the number of bytes actually written.
 */
int sys_write(int fd, const void *buffer, unsigned int size)
{
    // TODO
    return 1;
}

/* Changes the next byte to be read or written in file fd to position. */
void sys_seek(int fd, unsigned int position)
{
    // TODO
    return;
}

/* Returns the position of the next byte to be read or written
 * in file fd.
 */
unsigned int sys_tell(int fd)
{
    // TODO
    return 1;
}

/* Closes file descriptor fd. */
void sys_close(int fd)
{
    // TODO
    return;
}
