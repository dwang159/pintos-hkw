#ifndef SWAP_H
#define SWAP_H
#include "threads/thread.h"

typedef size_t slotid_t;

void swap_table_init(void);
void swap_table_destroy(void);
void swap_free_and_read(void *buf, slotid_t to_read);
void swap_free_several(slotid_t *to_free);
slotid_t swap_swalloc_and_write(void *buf);

#endif /* SWAP_H */
