#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "devices/block.h"

void cache_read(block_sector_t sect, void *addr) {
    block_read(fs_device, sect, addr);
}

void cache_write(block_sector_t sect, const void *addr) {
    block_write(fs_device, sect, addr);
}
