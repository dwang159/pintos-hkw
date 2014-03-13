#include "swap.h"
#include <stdio.h>
#include <debug.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "lib/debug.h"
#include "lib/kernel/bitmap.h"


static struct lock swap_table_lock;
static void *swap_buffer;
static struct bitmap *swap_table;
static struct block *swap_dev;

#define SWAP_NUM_SLOTS (1 << 10)

static block_sector_t slot_to_sect(slotid_t slot) {
    return (block_sector_t) slot;
}

/* Allocate space for the swap table and initialize it. */
void swap_table_init() {
    swap_table = bitmap_create(SWAP_NUM_SLOTS);
    swap_dev = block_get_role(BLOCK_SWAP);
    bitmap_mark(swap_table, 0); /* The 0 slot should never be given out,
                                  * for, you know, safety reasons. Null
                                  * terminated lists, for one reason. */
    lock_init(&swap_table_lock);
}

void swap_table_destroy() {
    lock_acquire(&swap_table_lock);
    bitmap_destroy(swap_table);
    /* Don't bother releasing the lock. There is no more swap table! */ 
}

/* Releases the block corresponding to the slotid argument
 * and writes the contents to the page starting at vaddr. */
void swap_free_and_read(void *vaddr, slotid_t swap_slot) {
    lock_acquire(&swap_table_lock);
    /* Check that the block being freed is in use. */
    ASSERT(bitmap_test(swap_table, swap_slot));
    block_sector_t sect = slot_to_sect(swap_slot);
    /* Read enough sectors from the disk for one page. */
    int i;
    for (i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
        block_read(swap_dev, sect, vaddr);
        vaddr += BLOCK_SECTOR_SIZE;
        sect += 1;
    }
    bitmap_reset(swap_table, swap_slot);
    lock_release(&swap_table_lock);
}

/* Releases a list of swap slots without writing them back. */
void swap_free_several(slotid_t *slot_list) {
    lock_acquire(&swap_table_lock);
    int i;
    slotid_t swap_slot;
    for (i = 0; slot_list[i]; i++) {
        swap_slot = slot_list[i];
        ASSERT(bitmap_test(swap_table, slot_list[i]));
        bitmap_reset(swap_table, swap_slot);
    }
    lock_release(&swap_table_lock);
}

/* Finds an empty swap slot, and writes the contents of
 * the page starting at vaddr to it. Returns the id of the
 * slot written to, and panics the kernel if it cannot
 * allocate a slot. */
slotid_t swap_swalloc_and_write(void *vaddr) {
    slotid_t out;
    lock_acquire(&swap_table_lock);
    /* Look for the first occurance of an empty slot */
    out = bitmap_scan_and_flip(swap_table, 0, 1, false); 
    if (out == BITMAP_ERROR)
        PANIC("Out of swap slots, could not swalloc.");
    block_sector_t sect = slot_to_sect(out);
    /* Write the page out over several sectors of the partition. */
    int i;
    for (i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
        block_write(swap_dev, sect, vaddr);
        vaddr += BLOCK_SECTOR_SIZE;
        sect += 1;
    }
    lock_release(&swap_table_lock);
    return out;
}
