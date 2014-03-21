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
#include "threads/thread.h"

/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

static void extend_to(struct inode *inode, off_t offset);

/* Number of i_blocks. */
#define N_BLOCKS 15

/* Number of blocks in a group. We allocate blocks one group at a time
 * to improve locality of data, while still enabling extensibility.
 * This should be chosen such that BLOCKS_PER_GROUP * BLOCK_SECTOR_SIZE
 * is a multiple of a page size.
 */
#define BLOCKS_PER_GROUP 8

/*! On-disk inode.
    Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
    // File size in bytes.
    off_t length;
    // Magic number used to identify inodes.
    unsigned magic;
    // Number of blocks currently used by this file.
    unsigned blocks_used;
    // Next free block in the group of blocks reserved for this file.
    // NULL if there are no more free blocks.
    block_sector_t next_block;
    // Number of blocks left in the next group reserved for this file.
    unsigned group_blocks_free;
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
    char unused[BLOCK_SECTOR_SIZE - (7 + N_BLOCKS) * 4];
};

/* Appends a sector to an inode. */
static bool append_sector(struct inode_disk *disk_inode,
        block_sector_t *result);

/* Removes the last sector from an inode. */
static bool pop_sector(struct inode *inode);

static void acquire(struct inode *inode);
static void release(struct inode *inode);

/*! Returns the number of sectors to allocate for an inode SIZE
    bytes long. */
static inline size_t bytes_to_sectors(off_t size) {
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/*! In-memory inode. */
struct inode {
    struct list_elem elem;       /*!< Element in inode list. */
    struct lock in_lock;
    bool is_dir;
    block_sector_t sector;       /*!< Sector number of disk location. */
    int open_cnt;                /*!< Number of openers. */
    bool removed;                /*!< True if deleted, false otherwise. */
    int deny_write_cnt;          /*!< 0: writes ok, >0: deny writes. */
    off_t length;      /*!< Inode content. */
};

/*! Returns the block device sector that contains byte offset POS
    within INODE.
    Returns -1 if INODE does not contain data for a byte at offset
    POS. */
static block_sector_t byte_to_sector(const struct inode *inode, off_t pos) {
    block_sector_t result;
    off_t start;

    ASSERT(inode != NULL);
    // Read in the data.
    struct inode_disk buffer;
    struct inode_disk *disk_inode = &buffer;
    ASSERT(disk_inode);
    cache_read(inode->sector, disk_inode);
    update_thread();
    if (pos >= disk_inode->length) {
    update_thread();
        return -1;
    }
    update_thread();
    // The virtual block we want (the block offset within the file if
    // the file was linear).
    unsigned vblock = pos / BLOCK_SECTOR_SIZE;
    update_thread();
    // Find the actual block sector that corresponds to pos.
    if (vblock <= N_BLOCKS - 4) {
        // If vblock is in the range 0..N_BLOCKS - 4, then we can directly
    update_thread();
        // get the block that we want.
        result = disk_inode->i_block[vblock];
    } else if (vblock <= BLOCK_SECTOR_SIZE / 4 + (N_BLOCKS - 4)) {
        // If vblock is in the range:
        // NBLOCKS - 3..BLOCK_SECTOR_SIZE / 4 + N_BLOCKS - 4
        // then it is accessed through 1-indirect addressing.
        // This requires one disk access to get the block sector.
        block_sector_t indirect_1 = disk_inode->i_block[N_BLOCKS - 3];
    update_thread();
        start = vblock - (N_BLOCKS - 3);
        cache_read_spec(indirect_1, &result, start * sizeof(block_sector_t),
                sizeof(block_sector_t));
    update_thread();
    update_thread();
    } else if (vblock <= (BLOCK_SECTOR_SIZE / 4) * (BLOCK_SECTOR_SIZE / 4) +
            BLOCK_SECTOR_SIZE / 4 + (N_BLOCKS - 4)) {
        // Otherwise, if vblock is in the range:
        // BLOCK_SECTOR_SIZE / 4 + N_BLOCKS - 4 through
        // (BLOCK_SECTOR_SIZE / 4)^2 + BLOCK_SECTOR_SIZE / 4 + N_BLOCKS - 4
    update_thread();
        // then it is accessed through 2-indirect addressing. This requires
        // two disk accesses to get the block sector.
    update_thread();
        block_sector_t indirect2 = disk_inode->i_block[N_BLOCKS - 2];
        block_sector_t indirect1;
        start = (vblock - BLOCK_SECTOR_SIZE / 4 - (N_BLOCKS - 3)) /
            (BLOCK_SECTOR_SIZE / 4);
        cache_read_spec(indirect2, &indirect1,
                start * sizeof(block_sector_t), sizeof(block_sector_t));
    update_thread();
        start = (vblock - BLOCK_SECTOR_SIZE / 4 - (N_BLOCKS - 3)) %
            (BLOCK_SECTOR_SIZE / 4);
        cache_read_spec(indirect1, &result,
                start * sizeof(block_sector_t), sizeof(block_sector_t));
    update_thread();
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

    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode == NULL) {
        return false;
    }
    size_t sectors = bytes_to_sectors(length);
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    disk_inode->blocks_used = 0;
    disk_inode->next_block = 0;
    disk_inode->group_blocks_free = 0;
    disk_inode->is_dir = is_dir;
    disk_inode->parent = parent;
    unsigned i;
    char *zeros = calloc(BLOCK_SECTOR_SIZE, sizeof(char));
    block_sector_t block;
    for (i = 0; i < sectors; i++) {
        if (append_sector(disk_inode, &block)) {
            // Fill the new block with zeros.
            cache_write(block, zeros);
        } else {
            return false;
        }
    }
    // Write inode to disk.
    cache_write(sector, disk_inode);
    free(disk_inode);
    free(zeros);
    return true;
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
    inode = (struct inode *) malloc(sizeof(struct inode));
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    lock_init(&inode->in_lock);
    struct inode_disk *buf = malloc(sizeof(struct inode_disk));
    cache_read(inode->sector, buf);
    update_thread();
    inode->is_dir = buf->is_dir;
    inode->length = buf->length;
    free(buf);
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
            struct inode_disk buffer;
            struct inode_disk *disk_inode = &buffer;
            cache_read(inode->sector, disk_inode);
            unsigned i;
            for (i = 0; i < disk_inode->blocks_used; i++)
                pop_sector(inode);
            free_map_release(inode->sector, 1);
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

bool inode_is_removed(const struct inode *inode){
    return inode && inode->removed;
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
        block_sector_t sector_idx = byte_to_sector(inode, offset);
    update_thread();
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    update_thread();
        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    update_thread();
        int min_left = inode_left < sector_left ? inode_left : sector_left;

    update_thread();
        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
    update_thread();
        if (chunk_size <= 0)
            break;
    update_thread();

    update_thread();
        cache_read_spec(sector_idx, buffer + bytes_read, sector_ofs,
                chunk_size);
    update_thread();
    update_thread();

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }
    update_thread();
    release(inode);
    update_thread();
    return bytes_read;
}

/*! Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
    Returns the number of bytes actually written, which may be
    less than SIZE if an error occurs. */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size,
        off_t offset) {
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    acquire(inode);
    if (inode->deny_write_cnt) {
        release(inode);
        return 0;
    }
    if (offset + size > inode->length) {
        extend_to(inode, offset + size);
    }

    while (size > 0) {
        /* Sector to write, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
    update_thread();
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

/* Extends the number of blocks used by the file to contain the offset
 * provided.
 */
void extend_to(struct inode *inode, off_t offset) {
    struct inode_disk buffer;
    struct inode_disk *disk_inode = &buffer;
    cache_read(inode->sector, disk_inode);
    int num_blocks = bytes_to_sectors(offset) - disk_inode->blocks_used;

    while (num_blocks > 0) {
        block_sector_t block;
        append_sector(disk_inode, &block);
        num_blocks--;
    }
    disk_inode->length = offset;
    inode->length = offset;
    cache_write(inode->sector, disk_inode);
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
    return inode->length;
}

/* Appends a sector to an inode. Returns true if successful, else
 * false. Write the newly allcoated block to result.
 */
static bool append_sector(struct inode_disk *disk_inode,
        block_sector_t *result) {
    ASSERT(disk_inode);
    // Check if there are any free blocks in the current group of blocks
    // allocated for this file.
    if (disk_inode->group_blocks_free == 0) {
        // Allocate a new group.
        if (free_map_allocate(BLOCKS_PER_GROUP, &disk_inode->next_block)) {
            disk_inode->group_blocks_free = BLOCKS_PER_GROUP;
        } else {
            // Allocation failed.
            return false;
        }
    }
    *result = disk_inode->next_block;
    disk_inode->next_block++;
    disk_inode->group_blocks_free--;
    if (disk_inode->group_blocks_free == 0) {
        disk_inode->next_block = 0;
    }
    // Insert this new block into the index table.
    disk_inode->blocks_used++;
    // Block offset within file.
    unsigned b = disk_inode->blocks_used - 1;
    if (b <= N_BLOCKS - 4) {
        // Direct addressing.
        disk_inode->i_block[b] = *result;
    } else if (b <= BLOCK_SECTOR_SIZE / 4 + (N_BLOCKS - 4)) {
        // 1-indirect addressing.
        block_sector_t indirect_block;
        // Index of our pointer within the indirect block.
        off_t index1 = b - (N_BLOCKS - 3);
        if (index1 == 0) {
            // We need to make a block for the indirect block.
            if (!free_map_allocate(1, &indirect_block)) {
                return false;
            }
            disk_inode->i_block[N_BLOCKS - 3] = indirect_block;
        } else {
            indirect_block = disk_inode->i_block[N_BLOCKS - 3];
        }
        // Write the location of the newly allocated block to the
        // indirect block.
        cache_write_spec(indirect_block, result,
                index1 * sizeof(block_sector_t), sizeof(block_sector_t));
    } else if (b <= (BLOCK_SECTOR_SIZE / 4) * (BLOCK_SECTOR_SIZE / 4) +
            BLOCK_SECTOR_SIZE / 4 + (N_BLOCKS - 4)) {
        // 2-indirect addressing.
        block_sector_t indirect2; // Contains pointers to indirect1 blocks.
        block_sector_t indirect1; // Contains pointers to data blocks.
        // Index of our desired pointers within the indirect2 and indirect1
        // blocks, respectively.
        off_t index2 = (b - BLOCK_SECTOR_SIZE / 4 - (N_BLOCKS - 3)) /
            (BLOCK_SECTOR_SIZE / 4);
        off_t index1 = (b - BLOCK_SECTOR_SIZE / 4 - (N_BLOCKS - 3)) %
            (BLOCK_SECTOR_SIZE / 4);
        if (index1 == 0 && index2 == 0) {
            // We need to create the indirect2 block.
            if (!free_map_allocate(1, &indirect2)) {
                return false;
            }
            disk_inode->i_block[N_BLOCKS - 2] = indirect2;
        } else {
            indirect2 = disk_inode->i_block[N_BLOCKS - 2];
        }
        if (index1 == 0) {
            // We need to create a new indirect1 block.
            if (!free_map_allocate(1, &indirect1)) {
                return false;
            }
            cache_write_spec(indirect2, &indirect1,
                    index2 * sizeof(block_sector_t), sizeof(block_sector_t));
        } else {
            // Read the location of the indirect1 block from the indirect2
            // block.
            cache_read_spec(indirect2, &indirect1,
                    index2 * sizeof(block_sector_t), sizeof(block_sector_t));
        }
        // Write the location of the newly allocated block to the
        // indirect block.
        cache_write_spec(indirect1, result,
                index1 * sizeof(block_sector_t), sizeof(block_sector_t));
    } else {
        PANIC("3-Indirect accessing not implemented!\n");
    }
    return true;
}

/* Frees the last sector of a file. We deallocate blocks as groups, so this
 * doesn't actually free any blocks until the entire group is free.
 * Returns true if successful.
 */
static bool pop_sector(struct inode *inode) {
    ASSERT(inode);
    // Get the address of the last block that was allocated.
    struct inode_disk buffer;
    struct inode_disk *disk_inode = &buffer;
    cache_read(inode->sector, disk_inode);
    if (disk_inode->blocks_used == 0) {
        PANIC("File is already empty!\n");
    }
    block_sector_t last_block = byte_to_sector(inode, inode->length - 1);
    // Mark this block as free.
    disk_inode->next_block = last_block;
    disk_inode->group_blocks_free++;
    disk_inode->blocks_used--;
    // If we reached a group size free, then actually free it.
    if (disk_inode->group_blocks_free == BLOCKS_PER_GROUP) {
        // Free the group.
        free_map_release(last_block, BLOCKS_PER_GROUP);
        disk_inode->next_block = 0;
        disk_inode->group_blocks_free = 0;
        // If there were indirect blocks associated with this block, then
        // we should free them as well if necessary.
        unsigned b = disk_inode->blocks_used + 1;
        if (b == N_BLOCKS - 3) {
            // Nothing left in the singly indirect block.
            free_map_release(disk_inode->i_block[N_BLOCKS - 3], 1);
        } else if (b >= BLOCK_SECTOR_SIZE / 4 + (N_BLOCKS - 3)) {
            off_t index2 = (b - BLOCK_SECTOR_SIZE / 4 - (N_BLOCKS - 3)) /
                (BLOCK_SECTOR_SIZE / 4);
            off_t index1 = (b - BLOCK_SECTOR_SIZE / 4 - (N_BLOCKS - 3)) %
                (BLOCK_SECTOR_SIZE / 4);
            block_sector_t indirect1;
            block_sector_t indirect2 = disk_inode->i_block[N_BLOCKS - 2];
            if (index1 == 0) {
                cache_read_spec(indirect2, &indirect1,
                        index2 * sizeof(block_sector_t),
                        sizeof(block_sector_t));
                free_map_release(indirect1, 1);
                if (index2 == 0) {
                    free_map_release(indirect2, 1);
                }
            }
        }
    }
    // Write back to disk.
    cache_write(inode->sector, disk_inode);
    return true;
}



void acquire(struct inode *inode) {
    lock_acquire(&inode->in_lock);
}

void release(struct inode *inode) {
    lock_release(&inode->in_lock);
}

bool inode_is_dir(const struct inode *inode) {
    return inode && inode->is_dir;
}
