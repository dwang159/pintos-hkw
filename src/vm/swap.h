#include "threads/thread.h"

typedef size_t slotid_t;

void swap_table_init(void);
void swap_table_destroy(void);
void swap_free_and_read(void *buf, slotid_t to_read);
slotid_t swap_swalloc_and_write(void *buf);
