#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"

/* Read from the buffer the block_sector into the pointer */
void cache_read(block_sector_t sect, void *target);
void cache_write(block_sector_t sect, const void *source);

#endif /* FILESYS_CACHE_H */
