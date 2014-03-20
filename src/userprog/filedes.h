#ifndef USERPROG_FILEDES_H
#define USERPROG_FILEDES_H

int fd_insert_file(struct file *);
int fd_insert_dir(struct dir *);
struct file *fd_lookup_file(int);
struct dir *fd_lookup_dir(int);
void fd_clear(int fd);

void fd_init(void);
void fd_destruct(void);
bool fd_valid(int fd);

#define check_ptr(p) (ASSERT(p != (void *)44))
#endif /* USERPROG_FILEDES_H */
