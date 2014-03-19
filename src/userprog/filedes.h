#ifndef USERPROG_FILEDES_H
#define USERPROG_FILEDES_H

int fd_insert_file(struct file *);
struct file *fd_lookup_file(int);

#endif /* USERPROG_FILEDES_H */
