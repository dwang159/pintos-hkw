#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"

/*! Maximum length of a file name component.
    This is the traditional UNIX maximum length.
    After directories are implemented, this maximum length may be
    retained, but much longer full path names must be allowed. */
#define NAME_MAX 14

struct inode;

/* Opening and closing directories. */
bool dir_create(block_sector_t sector, size_t entry_cnt, block_sector_t par);
struct dir *dir_open(struct inode *);
struct dir *dir_open_root(void);
struct dir *dir_reopen(struct dir *);
void dir_close(struct dir *);
struct inode *dir_get_inode(struct dir *);
bool dir_mkdir(const char *name, size_t entry_cnt);
bool dir_chdir(const char *name);

bool in_deleted_dir(void);
/* Reading and writing. */
bool dir_lookup(const struct dir *, const char *name, struct inode **);
bool dir_add(struct dir *, const char *name, block_sector_t);
bool dir_remove(struct dir *, const char *name);
bool dir_readdir(struct dir *, char name[NAME_MAX + 1]);
bool dir_entry_name(const struct dir *, size_t  i, char *name);

struct dir *dir_open_parent(const char *name, block_sector_t cwd);
bool dir_is_path(const char *name);
struct dir *dir_open_name(const char *name, char *fname);

#endif /* filesys/directory.h */

