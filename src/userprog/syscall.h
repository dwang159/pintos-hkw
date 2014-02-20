#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include<stdint.h>
#include<stdbool.h>
#include "threads/thread.h"
void syscall_init(void);

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
int sys_write(int fd, const void *buffer, unsigned size);
void sys_seek(int fd, unsigned int position);
unsigned int sys_tell(int fd);
void sys_close(int fd);

/* Checks if memory address is valid. */
bool mem_valid(const void *addr);

/* Checks file descriptor. */
bool fd_valid(int fd);

#endif /* userprog/syscall.h */

