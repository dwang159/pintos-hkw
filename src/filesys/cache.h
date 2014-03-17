#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/off_t.h"

/* Read from the buffer the block_sector into the pointer */
void cache_read(block_sector_t sect, void *target, off_t start, off_t size);
void cache_write(block_sector_t sect, const void *source, off_t start,
        off_t size);

#endif /* FILESYS_CACHE_H */
