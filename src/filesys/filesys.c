#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h" 
#include "threads/thread.h"
#include "filesys/cache.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/*! Partition that contains the file system. */
struct block *fs_device;

static void do_format(void);

/*! Initializes the file system module.
    If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
    fs_device = block_get_role(BLOCK_FILESYS);
    if (fs_device == NULL)
        PANIC("No file system device found, can't initialize file system.");
    cache_init();
    inode_init();
    free_map_init();

    if (format) 
        do_format();

    free_map_open();
    thread_current()->dir = inode_open(ROOT_DIR_SECTOR);
}

/*! Shuts down the file system module, writing any unwritten data to disk.
    */
void filesys_done(void) {
    cache_destroy();
    free_map_close();
}

/*! Creates a file named NAME with the given INITIAL_SIZE.  Returns true if
    successful, false otherwise.  Fails if a file named NAME already exists,
    or if internal memory allocation fails. */
bool filesys_create(const char *name, off_t initial_size) {
    block_sector_t inode_sector = 0;
    char *fname = malloc(strlen(name));
    if (fname == NULL)
        return false;
    struct dir *dir = dir_open_name(name, fname);

    bool success = (dir != NULL &&
                    free_map_allocate(1, &inode_sector) &&
                    inode_create(inode_sector, initial_size, false, 0) &&
                    dir_add(dir, fname, inode_sector));
    if (!success && inode_sector != 0) 
        free_map_release(inode_sector, 1);
    dir_close(dir);
    free(fname);

    return success;
}

/*! Opens the file with the given NAME.  Returns the new file if successful
    or a null pointer otherwise.  Fails if no file named NAME exists,
    or if an internal memory allocation fails. */
struct file * filesys_open(const char *name) {
    char *fname = malloc(strlen(name));
    if (fname == NULL)
        return false;
    struct dir *dir = dir_open_name(name, fname);
    struct inode *inode = NULL;
    if (dir != NULL)
        dir_lookup(dir, fname, &inode);
    dir_close(dir);
    free(fname);


    if(inode_is_dir(inode)) {
        return false;
    } else {
        return file_open(inode);
    }
}

struct dir *filesys_open_dir(const char *name) {
    // Special case, because of the empty fname.
    if (!strcmp("/", name)) {
        return dir_open_root();
    }
    char *fname = malloc(strlen(name));
    if (fname == NULL)
        return false;
    struct dir *dir = dir_open_name(name, fname);
    struct inode *inode = NULL;
    if (dir != NULL)
        dir_lookup(dir, fname, &inode);
    dir_close(dir);
    free(fname);

    if (inode_is_dir(inode)) {
        return dir_open(inode);
    } else {
        return NULL;
    }
}

/*! Deletes the file named NAME.  Returns true if successful, false on
 *  failure.  Fails if no file named NAME exists, or if an internal
 *  memory allocation fails. */
bool filesys_remove(const char *name) {
    char *fname = malloc(strlen(name));
    ASSERT(fname);
    struct dir *dir = dir_open_name(name, fname);
    bool success = dir != NULL && dir_remove(dir, fname);
    dir_close(dir);

    return success;
}

/*! Formats the file system. */
static void do_format(void) {
    printf("Formatting file system...");
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16, ROOT_DIR_SECTOR))
        PANIC("root directory creation failed");
    free_map_close();
    printf("done.\n");
}

