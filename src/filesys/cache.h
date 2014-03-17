#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/off_t.h"

/* Read from the buffer the block_sector into the pointer */
void cache_read_spec(block_sector_t sect, void *target, off_t start,
        off_t size);
void cache_write_spec(block_sector_t sect, const void *source, off_t start,
        off_t size);

/* Sane defaults for start and size. */
void cache_read(block_sector_t sect, void *target);
void cache_write(block_sector_t sect, const void *source);

/* Start and clean up. */
void cache_init(void);
void cache_writeback_all(void);
void cache_destroy(void);
#endif /* FILESYS_CACHE_H */
