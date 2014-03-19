#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include <list.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;
/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Number of i_blocks. */
#define N_BLOCKS 15


/*! On-disk inode.
    Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
    block_sector_t start; // TODO: eventually get rid of it
    // File size in bytes.
    off_t length;
    // Magic number used to identify inodes.
    unsigned magic;
    /* Block addressing, contains pointers to other blocks.
     * 0..N_BLOCKS - 4 are direct addresses
     * N_BLOCKS - 3 has 1-indirect addresses
     * N_BLOCKS - 2 has 2-indirect addresses
     * N_BLOCKS - 1 has 3-indirect addresses (unused).
     */
    block_sector_t i_block[N_BLOCKS];
    // Whether this inode is a directory
    bool is_dir;
    // If this inode is a directory, represents the parent directory. 
    // Invalid for files due to hard linking
    block_sector_t parent; 
    // Fills up the rest of the space in this block.
    char unused[BLOCK_SECTOR_SIZE - 5 * 4 - N_BLOCKS * 4];
};

/*! In-memory inode. */
struct inode {
    struct list_elem elem;       /*!< Element in inode list. */
    block_sector_t sector;       /*!< Sector number of disk location. */
    int open_cnt;                /*!< Number of openers. */
    bool removed;                /*!< True if deleted, false otherwise. */
    int deny_write_cnt;          /*!< 0: writes ok, >0: deny writes. */
    struct inode_disk data;      /*!< Inode content. */
};

void inode_init(void);
bool inode_create(block_sector_t, off_t, bool, block_sector_t);
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);

#endif /* filesys/inode.h */
