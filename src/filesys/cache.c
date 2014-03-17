#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "devices/block.h"
#include "threads/malloc.h"
#include <string.h>

#define BUF_NUM_SLOTS 64

/* Flags to describe state of a slot in the filesystem buffer. */
#define FS_BUF_UNUSED 0
#define FS_BUF_INUSE 1
#define FS_BUF_ACCESSED 2
#define FS_BUF_DIRTY 4
struct cache_slot {
    block_sector_t sect_id;
    unsigned flags;
    char content[BLOCK_SECTOR_SIZE];
};

static struct cache_slot fs_buffer[BUF_NUM_SLOTS];
static char bounce[BLOCK_SECTOR_SIZE];

/* Determines if a sector is in the cache. */
static bool buff_present(block_sector_t);
static bool is_slot_empty(int slot_id UNUSED) UNUSED;
static int force_empty_slot(void);
static int passive_empty_slot(void);
static void writeback(int);

/* Reads the filesys sector sect into addr. Starts at offset into the
 * sector and reads size bytes. This is a read of at most *one* sector.
 * Will consult the cache and only go to the disk if necessary. */
void cache_read_spec(block_sector_t sect, void *addr, off_t offset,
        off_t size) {
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    ASSERT(size + offset <= BLOCK_SECTOR_SIZE);
    if (buff_present(sect)) {
        PANIC("How can you read from the cache if you "
              "haven't written to it? huh?\n");
    } else {
        block_read(fs_device, sect, bounce);
        memcpy(addr, bounce + offset, size);
    }
}

void cache_read(block_sector_t sect, void *addr) {
    cache_read_spec(sect, addr, 0, BLOCK_SECTOR_SIZE);
}

void cache_write(block_sector_t sect, const void *addr) {
    cache_write_spec(sect, addr, 0, BLOCK_SECTOR_SIZE);
}

/* Writes the sector sect of the filesystem block device into sect,
 * but of course checks if it is in the cache first. */
void cache_write_spec(block_sector_t sect, const void *addr, off_t offset,
        off_t size) {
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    ASSERT(size + offset <= BLOCK_SECTOR_SIZE);
    if (buff_present(sect)) {
        PANIC("You don't actually have a cache to work with...\n");
    } else {
        int idx = force_empty_slot();
        fs_buffer[idx].sect_id = sect;
        fs_buffer[idx].flags = FS_BUF_INUSE;
        char *buff_actual = fs_buffer[idx].content;
        if (offset > 0 || offset + size < BLOCK_SECTOR_SIZE) {
            block_read(fs_device, sect, buff_actual);
        } else {
            memset(buff_actual, 0, BLOCK_SECTOR_SIZE);
        }
        memcpy(buff_actual+ offset, addr, size);
        block_write(fs_device, sect, buff_actual);
    }
}

bool buff_present(block_sector_t sect UNUSED) {
    return false;
}

bool is_slot_empty(int slot_id UNUSED) {
    return false;
}

int force_empty_slot(void) {
    int ret = passive_empty_slot();
    if (ret != -1)
        return ret;
    else {
        writeback(0);
        return 0;
    }
}

int passive_empty_slot(void) {
    return -1;
}

void writeback(int cache_slot) {
    if (fs_buffer[cache_slot].flags & FS_BUF_DIRTY) {
        block_write(fs_device, fs_buffer[cache_slot].sect_id,
                fs_buffer[cache_slot].content);
    }
    fs_buffer[cache_slot].flags = FS_BUF_UNUSED;
}
