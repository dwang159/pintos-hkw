#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "devices/block.h"
#include "threads/malloc.h"
#include <string.h>

static char bounce[BLOCK_SECTOR_SIZE];

/* Reads the filesys sector sect into addr. Starts at offset into the
 * sector and reads size bytes. This is a read of at most *one* sector.
 * Will consult the cache and only go to the disk if necessary. */
void cache_read(block_sector_t sect, void *addr, off_t offset, off_t size) {
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    ASSERT(size + offset <= BLOCK_SECTOR_SIZE);
    block_read(fs_device, sect, bounce);
    memcpy(addr, bounce + offset, size);
}

void cache_write(block_sector_t sect, const void *addr, off_t offset,
        off_t size) {
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    ASSERT(size + offset <= BLOCK_SECTOR_SIZE);
    void *bounce = malloc(BLOCK_SECTOR_SIZE);
    if (offset > 0 || offset + size < BLOCK_SECTOR_SIZE) {
        block_read(fs_device, sect, bounce);
    } else {
        memset(bounce, 0, BLOCK_SECTOR_SIZE);
    }
    memcpy(bounce + offset, addr, size);
    block_write(fs_device, sect, bounce);
}
