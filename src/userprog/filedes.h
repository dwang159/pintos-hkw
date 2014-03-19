#ifndef USERPROG_FILEDES_H
#define USERPROG_FILEDES_H

int fd_insert_file(struct file *);
struct file *fd_lookup_file(int);

void fd_clear(int fd);

void fd_init(void);
void fd_destruct(void);
#endif /* USERPROG_FILEDES_H */
