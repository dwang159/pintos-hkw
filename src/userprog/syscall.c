#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "process.h"
#include "threads/synch.h"
#include "userprog/filedes.h"
#include "lib/user/syscall.h"

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

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    void *esp = f->esp;
    void *args;
    bool args_valid = true;

    /* Check pointer validity */
    if (!check_args_1(esp, int)) {
        sys_exit(-1);
        return;
    }

    int syscall_nr = *((int *) esp);
    int off1, off2;
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
                     *((unsigned int *) (args + off1)));
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
    case SYS_CHDIR:
        if (check_args_1(args, char *))
            f->eax = (uint32_t ) sys_chdir(*((char **) args));
        else
            args_valid = false;
        break;
    case SYS_MKDIR:
        if (check_args_1(args, char *))
            f->eax = (uint32_t) sys_mkdir(*((char **) args));
        else
            args_valid = false;
        break;
    case SYS_READDIR:
        if (check_args_2(args, int, char *)) {
            off1 = sizeof(int);
            f->eax = (uint32_t )sys_readdir(*((int *) args),
                                            *((char **) (args + off1)));
        } else
            args_valid = false;
        break;
    case SYS_ISDIR:
        if (check_args_1(args, int))
            f->eax = (uint32_t) sys_isdir(*(int *) args);
        else
            args_valid = false;
        break;
    case SYS_INUMBER:
        if (check_args_1(args, int))
            f->eax = (uint32_t) sys_inumber(*(int *) args);
        else
            args_valid = false;
        break;
    default:
        args_valid = false;
        break;
    }

    /* If something went wrong, then we exit with status code -1. */
    if (!args_valid)
        sys_exit(-1);
    return;
}

/* Checks that addr points into user space on a currently mapped page. */
bool mem_valid(const void *addr) {
    return (addr != NULL && is_user_vaddr(addr) &&
            pagedir_get_page(thread_current()->pagedir, addr) != NULL);
}

/* Terminates Pintos. */
void sys_halt() {
    shutdown_power_off();
}

/* Terminates the current process, returning status. */
void sys_exit(int status) {
    // Set the exit status.
    struct thread *t = thread_current();

    // Print exit message.
    printf("%s: exit(%d)\n", t->name, status);

    // Set exit status.
    struct exit_state *es = thread_exit_status.data[t->tid];
    es->exit_status = status;

    // Do the rest of the exit process.
    thread_exit();
}

/* Runs the executable given by cmd_line. Returns the pid of
 * the process.
 */
pid_t sys_exec(const char *cmd_line) {
    if (!mem_valid(cmd_line))
        sys_exit(-1);
    enum intr_level old_level = intr_disable();
    pid_t ret = process_execute(cmd_line);
    intr_set_level(old_level);
    struct exit_state *es = thread_exit_status.data[ret];
    // Wait until the child process has launched, to measure
    // whether it was successful or not. 
    sema_down(&es->launching);
    return es->load_successful ? ret : TID_ERROR;
}

/* Waits for child process to terminate, then returns the
 * returned status.
 */
int sys_wait(pid_t pid) {
    // We map pids directly into tids, so we know the tid of the
    // thread we want.
    return process_wait((tid_t) pid);
}

/* Creates a new file with initial_size bytes. Returns true on
 * success.
 */
bool sys_create(const char *file, unsigned int initial_size) {
    if (!mem_valid(file)) {
        sys_exit(-1);
    }
    if (in_deleted_dir()) {
        return false;
    }
    bool ret = filesys_create(file, initial_size);
    return ret;
}

/* Deletes the file called file. Returns true on success. */
bool sys_remove(const char *file) {
    if (!mem_valid(file))
        sys_exit(-1);
    int fd = sys_open(file);
    if (sys_isdir(fd)) {
        char name[16];
        bool out = sys_readdir(fd, name);
        sys_close(fd);
        // Cannot delete a nonempty directory.
        if (out) {
            return false;
        }
    } else {
        sys_close(fd);
    }

    bool ret = filesys_remove(file);
    return ret;
}

/* Opens the file called file. Returns the file descriptor or -1
 * on failure.
 * */
int sys_open(const char *filename) {
    /* Check for invalid pointer */
    if (!mem_valid(filename))
        sys_exit(-1);
    if (in_deleted_dir()) {
        return -1;
    }
    /* Open file, return -1 on failure */

    struct file *file = NULL;
    struct dir *dir = NULL;
    file = filesys_open(filename);
    if (file == NULL) {
        dir = filesys_open_dir(filename);
    }
    if (file != NULL) {
        int out = fd_insert_file(file);
        return out;
    } else if (dir != NULL) {
        int out = fd_insert_dir(dir);
        return out;
    } else {
        return -1;
    }
}

/* Returns the size of the file open, given the file descriptor. */
int sys_filesize(int fd) {
    if (!fd_valid(fd) || sys_isdir(fd))
        sys_exit(-1);
    struct file *file = fd_lookup_file(fd);
    int ret = file_length(file);
    return ret;
}

/* Reads size bytes from the file fd into buffer. Returns the number
 * of bytes actually read.
 */
int sys_read(int fd, void *buffer, unsigned int size) {
    unsigned int i;

    if (!mem_valid(buffer) || !mem_valid(buffer + size - 1) ||
            fd == STDOUT_FILENO || !fd_valid(fd) || sys_isdir(fd))
        sys_exit(-1);

    if (fd == STDIN_FILENO) {
        for (i = 0; i < size; i++) {
            *((char *) buffer) = input_getc();
            buffer += sizeof(char);
        }
        return size;
    } else {
        struct file *file = fd_lookup_file(fd);
        int ret = (int) file_read(file, buffer, size);
        return ret;
    }
}

/* Writes size bytes from the buffer into the open file fd. Returns
 * the number of bytes actually written.
 */
int sys_write(int fd, const void *buffer, unsigned int size) {
    if (!mem_valid(buffer) || !mem_valid(buffer + size - 1) ||
            fd == STDIN_FILENO || !fd_valid(fd) || sys_isdir(fd))
        sys_exit(-1);

    if (fd == STDOUT_FILENO) {
        putbuf((char *) buffer, size);
        return size;
    } else {
        struct file *file = fd_lookup_file(fd);
        int ret = (int) file_write(file, buffer, size);
        return ret;
    }
}

/* Changes the next byte to be read or written in file fd to position. */
void sys_seek(int fd, unsigned int position) {
    if (!fd_valid(fd) || sys_isdir(fd))
        sys_exit(-1);
    struct file *file = fd_lookup_file(fd);
    file_seek(file, position);
}

/* Returns the position of the next byte to be read or written
 * in file fd.
 */
unsigned int sys_tell(int fd) {
    if (!fd_valid(fd) || sys_isdir(fd))
        sys_exit(-1);

    struct file *file = fd_lookup_file(fd);
    unsigned int ret = file_tell(file);
    return ret;
}

/* Closes file descriptor fd. */
void sys_close(int fd) {
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || !fd_valid(fd))
        sys_exit(-1);

    if (sys_isdir(fd)) { 
        struct dir *dir = fd_lookup_dir(fd);
        dir_close(dir);
    } else {
        struct file *file = fd_lookup_file(fd);
        file_close(file);
    }
    fd_clear(fd);
}

bool sys_chdir(const char *dir) {
    if (strlen(dir) == 0)
        return false;
    return dir_chdir(dir);
}

bool sys_mkdir(const char *dir) {
    if (strlen(dir) == 0)
        return false;
    return dir_mkdir(dir, 16);
}

bool sys_readdir(int fd, char *name) {
    if (!fd_valid(fd)) {
        sys_exit(-1);
    }
    if (!sys_isdir(fd)) {
        return false;
    }
    struct dir *dir = fd_lookup_dir(fd);
    int i = fd_count_dir(fd);
    return dir_entry_name(dir, i, name);
}

bool sys_isdir(int fd) {
    return fd_valid(fd) ? fd_lookup_dir(fd) : false;
}

int sys_inumber(int fd) {
    struct inode *inode;
    if (!fd_valid(fd)) {
        return -1;
    } else if (sys_isdir(fd)) {
        struct dir *dir = fd_lookup_dir(fd);
        inode = dir_get_inode(dir);
        return inode_get_inumber(inode);
    } else {
        struct file *file = fd_lookup_file(fd);
        inode = file_get_inode(file);
    }
    return inode_get_inumber(inode);
}
