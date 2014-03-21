#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "lib/user/syscall.h"
#include "threads/synch.h"

/*! A directory. */
struct dir {
    struct lock dlock;                  // Lock for creation/deletion of files
    struct inode *inode;                /*!< Backing store. */
    off_t pos;                          /*!< Current position. */
};

/*! A single directory entry. */
struct dir_entry {
    block_sector_t inode_sector;        /*!< Sector number of header. */
    char name[NAME_MAX + 1];            /*!< Null terminated file name. */
    bool in_use;                        /*!< In use or free? */
};

/*! Creates a directory with space for ENTRY_CNT entries in the
    given SECTOR.  Returns true if successful, false on failure. */
bool dir_create(block_sector_t sector, size_t entry_cnt, block_sector_t par) {
    if(inode_create(sector, entry_cnt * sizeof(struct dir_entry), 
            true, par)) {
        struct dir *d = dir_open(inode_open(sector));
        if (dir_add(d, ".", sector) && dir_add(d, "..", par)) {
            return true;
        }
    }
    return false;
}

/* Makes a directory at the specified path with the specified name, where
 * this information is encoded in NAME. Allow this new directory to have
 * entry_cnt entries */
bool dir_mkdir(const char *name, size_t entry_cnt) {
    block_sector_t inode_sector = 0;
    char *dname = malloc(strlen(name));
    if (dname == NULL)
        return false;
    struct dir *d = dir_open_name(name, dname);
    if (d == NULL) {
        free(dname);
        return false;
    }

    // Create the new directory
    bool success = (d != NULL &&
                    free_map_allocate(1, &inode_sector) &&
                    dir_create(inode_sector, entry_cnt, 
                        inode_get_inumber(d->inode)) &&
                    dir_add(d, dname, inode_sector));
    if (!success && inode_sector != 0)
        free_map_release(inode_sector, 1);
    dir_close(d);
    free(dname);
    return success;
}

/*! Opens and returns the directory for the given INODE, of which
    it takes ownership.  Returns a null pointer on failure. */
struct dir * dir_open(struct inode *inode) {
    struct dir *dir = calloc(1, sizeof(*dir));
    if (inode != NULL && dir != NULL) {
        dir->inode = inode;
        dir->pos = 0;
        lock_init(&dir->dlock);
        return dir;
    }
    else {
        inode_close(inode);
        free(dir);
        return NULL; 
    }
}

/*! Opens the root directory and returns a directory for it.
    Return true if successful, false on failure. */
struct dir * dir_open_root(void) {
    return dir_open(inode_open(ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir * dir_reopen(struct dir *dir) {
    return dir_open(inode_reopen(dir->inode));
}

/*! Destroys DIR and frees associated resources. */
void dir_close(struct dir *dir) {
    if (dir != NULL) {
        inode_close(dir->inode);
        free(dir);
    }
}

/*! Returns the inode encapsulated by DIR. */
struct inode * dir_get_inode(struct dir *dir) {
    return dir->inode;
}

/*! Searches DIR for a file with the given NAME.
    If successful, returns true, sets *EP to the directory entry
    if EP is non-null, and sets *OFSP to the byte offset of the
    directory entry if OFSP is non-null.
    otherwise, returns false and ignores EP and OFSP. */
static bool lookup(const struct dir *dir, const char *name,
                   struct dir_entry *ep, off_t *ofsp) {
    struct dir_entry e;
    size_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    for(ofs = 0; inode_read_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);
             ofs += sizeof(e)) {
        if (e.in_use && !strcmp(name, e.name)) {
            if (ep != NULL) {
                *ep = e;
            }
            if (ofsp != NULL) {
                *ofsp = ofs;
            }
            return true;
        }
    }
    return false;
}

bool dir_entry_name(const struct dir *dir, size_t i, char *name) {
    size_t ofs = i * sizeof(struct dir_entry);
    struct dir_entry e;
    if (inode_read_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e)) {
        if (e.in_use) {
            strlcpy(name, e.name, READDIR_MAX_LEN);
            return true;
        }
    }
    return false;
}

/*! Searches DIR for a file with the given NAME and returns true if one
    exists, false otherwise.  On success, sets *INODE to an inode for the
    file, otherwise to a null pointer.  The caller must close *INODE. */
bool dir_lookup(const struct dir *dir, const char *name,
        struct inode **inode_p) {
    struct dir_entry e;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    if (lookup(dir, name, &e, NULL)) {
        *inode_p = inode_open(e.inode_sector);
        if (inode_is_removed(*inode_p)) {
            *inode_p = NULL;
        }
    }
    else
        *inode_p = NULL;

    return *inode_p != NULL;
}

/*! Adds a file named NAME to DIR, which must not already contain a file by
    that name.  The file's inode is in sector INODE_SECTOR.
    Returns true if successful, false on failure.
    Fails if NAME is invalid (i.e. too long) or a disk or memory
    error occurs. */
bool dir_add(struct dir *dir, const char *name, block_sector_t inode_sector) {
    struct dir_entry e;
    off_t ofs;
    bool success = false;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Check NAME for validity. */
    if (*name == '\0' || strlen(name) > NAME_MAX)
        return false;

    lock_acquire(&(dir->dlock));
    /* Check that NAME is not in use. */
    if (lookup(dir, name, NULL, NULL)) {
        goto done;
    }

    /* Set OFS to offset of free slot.
     * If there are no free slots, then it will be set to the
     * current end-of-file.
     *
     * inode_read_at() will only return a short read at end of file.
     * Otherwise, we'd need to verify that we didn't get a short
     * read due to something intermittent such as low memory. */
    for(ofs = 0; inode_read_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);
            ofs += sizeof(e)) {
        if (!e.in_use)
            break;
    }

    /* Write slot. */
    e.in_use = true;
    strlcpy(e.name, name, sizeof e.name);
    e.inode_sector = inode_sector;
    success = inode_write_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);

done:
    lock_release((&dir->dlock));
    return success;
}

/*! Removes any entry for NAME in DIR.  Returns true if successful, false on
    failure, which occurs only if there is no file with the given NAME. */
bool dir_remove(struct dir *dir, const char *name) {
    struct dir_entry e;
    struct inode *inode = NULL;
    bool success = false;
    off_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    lock_acquire(&dir->dlock);
    /* Find directory entry. */
    if (!lookup(dir, name, &e, &ofs)) {
        goto done;
    }

    /* Open inode. */
    inode = inode_open(e.inode_sector);
    if (inode == NULL)
        goto done;

    /* Erase directory entry. */
    e.in_use = false;
    if (inode_write_at(dir->inode, &e, sizeof(e), ofs) != sizeof(e))
        goto done;

    /* Remove inode. */
    inode_remove(inode);
    success = true;

done:
    inode_close(inode);
    lock_release(&dir->dlock);
    return success;
}

/*! Reads the next directory entry in DIR and stores the name in NAME.
    Returns true if successful, false if the directory contains no more
    entries. */
bool dir_readdir(struct dir *dir, char name[NAME_MAX + 1]) {
    struct dir_entry e;

    while(inode_read_at(dir->inode, &e, sizeof(e), dir->pos) == sizeof(e)) {
        dir->pos += sizeof(e);
        if (e.in_use) {
            strlcpy(name, e.name, NAME_MAX + 1);
            return true;
        } 
    }
    return false;
}

/* Given a path, opens the parent directory of the specified file and returns
 * a pointer to it. If an invalid path is given, returns NULL */
struct dir * dir_open_parent(const char * name, block_sector_t cwd) {
    char *copied_name = malloc(strlen(name));
    if (copied_name == NULL)
        return NULL;

    char **buf = malloc(strlen(name));
    if (buf == NULL)
        return NULL;

    bool absolute = (name[0] == '/');
    struct dir *d, *old;

    strlcpy(copied_name, name, strlen(name) + 1);
    if (absolute) {
        d = dir_open_root();
    } else {
        d = dir_open(inode_open(cwd));
    }

    // Tokenize string, then open each directory starting from the root
    // or the current working directory.
    struct inode *i;
    char *tok = strtok_r(copied_name, "/", buf);
    while (tok != NULL) {
        old = d;
        dir_lookup(d, tok, &i);
        d = dir_open(i);
        tok = strtok_r(NULL, "/", buf);
        if (tok == NULL) {
            d = old;
            break;
        }
        if (d == NULL) {
            free(copied_name);
            free(buf);
            return NULL;
        }
        dir_close(old);
    }
    free(buf);
    free(copied_name);
    return d;
}

/* Determines whether a given name is a filename or a path */
bool dir_is_path(const char *name) {
    return (strchr(name, '/') != NULL);
}

bool dir_chdir(const char *name) {
    char *dname = malloc(strlen(name));
    if (dname == NULL)
        return false;
    struct dir *d = dir_open_name(name, dname);
    if (d == NULL) {
        free (dname);
        return false;
    }

    struct inode *i;
    if (!dir_lookup(d, dname, &i)) {
        return false;
    }
    d = dir_open(i);
    if (d == NULL) {
        free(dname);
        return false;
    }
    thread_current()->dir = d->inode;
    free(dname);
    return true;
}

bool in_deleted_dir(void) {
    struct inode *cwd_inode = thread_current()->dir;
    return inode_is_removed(cwd_inode);
}

/* Given the path name of a directory, opens that directory and separates 
 * the file name from the path, putting it into fname */
struct dir *dir_open_name(const char *name, char *fname) {
    struct dir *dir;
    char *copied_name = malloc(strlen(name));
    if (copied_name == NULL)
        return NULL;

    strlcpy(copied_name, name, strlen(name) + 1);
    // Open the specified directory. If not given a name requiring
    // directory traversal, opens the current working directory.
    block_sector_t cwd = inode_get_inumber(thread_current()->dir);
    if (dir_is_path(copied_name)) {
        dir = dir_open_parent(copied_name, cwd);
    } else {
        dir = dir_open(inode_open(cwd));
    }
    // Find the actual name of the directory
    char *aname = strrchr(copied_name, '/');
    if (aname != NULL) {
        aname ++;
    } else {
        aname = copied_name;
    }
    strlcpy(fname, aname, strlen(aname) + 1);
    free(copied_name);
    return dir;
}
