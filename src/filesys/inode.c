#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Number of i_blocks. */
#define N_BLOCKS 15

static void acquire(struct inode *inode);
static void release(struct inode *inode);

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

/*! Returns the number of sectors to allocate for an inode SIZE
    bytes long. */
static inline size_t bytes_to_sectors(off_t size) {
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/*! In-memory inode. */
struct inode {
    struct list_elem elem;       /*!< Element in inode list. */
    struct lock in_lock;
    block_sector_t sector;       /*!< Sector number of disk location. */
    int open_cnt;                /*!< Number of openers. */
    bool removed;                /*!< True if deleted, false otherwise. */
    int deny_write_cnt;          /*!< 0: writes ok, >0: deny writes. */
    struct inode_disk data;      /*!< Inode content. */
};

/*! Returns the block device sector that contains byte offset POS
    within INODE.
    Returns -1 if INODE does not contain data for a byte at offset
    POS. */
static block_sector_t byte_to_sector(const struct inode *inode, off_t pos) {
    block_sector_t result;
    off_t start;

    ASSERT(inode != NULL);
    if (pos >= inode->data.length)
        return -1;
    // TODO: old
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;

    // The virtual block we want (the block offset within the file if
    // the file was linear).
    unsigned vblock = pos / BLOCK_SECTOR_SIZE;
    // Find the actual block sector that corresponds to pos.
    if (vblock <= N_BLOCKS - 4) {
        // If vblock is in the range 0..N_BLOCKS - 4, then we can directly
        // get the block that we want.
        return inode->data.i_block[vblock];
    } else if (vblock <= BLOCK_SECTOR_SIZE / 4 + (N_BLOCKS - 4)) {
        // If vblock is in the range:
        // NBLOCKS - 3..BLOCK_SECTOR_SIZE / 4 + N_BLOCKS - 4
        // then it is accessed through 1-indirect addressing.
        // This requires one disk access to get the block sector.
        block_sector_t indirect_1 = inode->data.i_block[N_BLOCKS - 3];
        start = vblock - (N_BLOCKS - 4);
        cache_read_spec(indirect_1, &result, start, sizeof(block_sector_t));
    } else if (vblock <= (BLOCK_SECTOR_SIZE / 4) * (BLOCK_SECTOR_SIZE / 4) +
            BLOCK_SECTOR_SIZE / 4 + (N_BLOCKS - 4)) {
        // Otherwise, if vblock is in the range:
        // BLOCK_SECTOR_SIZE / 4 + N_BLOCKS - 4 through
        // (BLOCK_SECTOR_SIZE / 4)^2 + BLOCK_SECTOR_SIZE / 4 + N_BLOCKS - 4
        // then it is accessed through 2-indirect addressing. This requires
        // two disk accesses to get the block sector.
        block_sector_t indirect_2 = inode->data.i_block[N_BLOCKS - 2];
        block_sector_t indirect_1;
        start = (vblock - (N_BLOCKS - 4)) / (BLOCK_SECTOR_SIZE / 4);
        cache_read_spec(indirect_2, &indirect_1, start,
                sizeof(block_sector_t));

        start = (vblock - (N_BLOCKS - 4)) % (BLOCK_SECTOR_SIZE / 4);
        cache_read_spec(indirect_1, &result, start, sizeof(block_sector_t));
    } else {
        PANIC("Level 3 indirect addressing not implemented.\n");
    }
    return result;
}

/*! List of open inodes, so that opening a single inode twice
    returns the same `struct inode'. */
static struct list open_inodes;

/*! Initializes the inode module. */
void inode_init(void) {
    list_init(&open_inodes);
}

/*! Initializes an inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length, bool is_dir,
        block_sector_t parent) {
    struct inode_disk *disk_inode = NULL;
    bool success = false;

    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL) {
        size_t sectors = bytes_to_sectors(length);
        disk_inode->length = length;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->is_dir = is_dir;
        disk_inode->parent = parent;
        if (free_map_allocate(sectors, &disk_inode->start)) {
            cache_write(sector, disk_inode);
            if (sectors > 0) {
                static char zeros[BLOCK_SECTOR_SIZE];
                size_t i;

                for (i = 0; i < sectors; i++)
                    cache_write(disk_inode->start + i, zeros);
            }
            success = true;
        }
        free(disk_inode);
    }
    return success;
}

/*! Reads an inode from SECTOR
    and returns a `struct inode' that contains it.
    Returns a null pointer if memory allocation fails. */
struct inode * inode_open(block_sector_t sector) {
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            return inode;
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    lock_init(&inode->in_lock);
    cache_read(inode->sector, &inode->data);
    return inode;
}

/*! Reopens and returns INODE. */
struct inode * inode_reopen(struct inode *inode) {
    if (inode != NULL) {
        acquire(inode);
        inode->open_cnt++;
        release(inode);
    }
    return inode;
}

/*! Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode *inode) {
    return inode->sector;
}

/*! Closes INODE and writes it to disk.
    If this was the last reference to INODE, frees its memory.
    If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode) {
    /* Ignore null pointer. */
    if (inode == NULL)
        return;
    acquire(inode);
    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0) {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);

        /* Deallocate blocks if removed. */
        if (inode->removed) {
            free_map_release(inode->sector, 1);
            free_map_release(inode->data.start,
                             bytes_to_sectors(inode->data.length));
        }
        release(inode);
        free(inode);
    } else {
        release(inode);
    }
}

/*! Marks INODE to be deleted when it is closed by the last caller who
    has it open. */
void inode_remove(struct inode *inode) {
    ASSERT(inode != NULL);
    acquire(inode);
    inode->removed = true;
    release(inode);
}

/*! Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size,
        off_t offset) {
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    acquire(inode);
    while (size > 0) {
        /* Disk sector to read, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector (inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        cache_read_spec(sector_idx, buffer + bytes_read, sector_ofs,
                chunk_size);

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }
    release(inode);
    return bytes_read;
}

/*! Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
    Returns the number of bytes actually written, which may be
    less than SIZE if end of file is reached or an error occurs.
    (Normally a write at end of file would extend the inode, but
    growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size,
        off_t offset) {
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    acquire(inode);
    if (inode->deny_write_cnt) {
        release(inode);
        return 0;
    }

    while (size > 0) {
        /* Sector to write, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        /* Write full sector directly to disk through the cache. */
        cache_write_spec(sector_idx, buffer + bytes_written, sector_ofs,
            chunk_size);

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }
    release(inode);
    return bytes_written;
}

/*! Disables writes to INODE.
    May be called at most once per inode opener. */
void inode_deny_write (struct inode *inode) {
    acquire(inode);
    inode->deny_write_cnt++;
    release(inode);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/*! Re-enables writes to INODE.
    Must be called once by each inode opener who has called
    inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write (struct inode *inode) {
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    acquire(inode);
    inode->deny_write_cnt--;
    release(inode);
}

/*! Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode) {
    return inode->data.length;
}

void acquire(struct inode *inode) {
    lock_acquire(&inode->in_lock);
}

void release(struct inode *inode) {
    lock_release(&inode->in_lock);
}
