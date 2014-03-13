#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <hash.h>
#include "vm/page.h"

/* Contains all elements of the frame table. */
struct frame_table {
    struct hash data;
};

/* An element of the frame table. */
struct frame_entry {
    // Element used to store entry in frame table.
    struct hash_elem elem;

    // Key used to identify page. This is a pointer to a kernel page,
    // and the key is retrieved using frame_get_key().
    unsigned key;

    // Thread and key for the supplemental page table associated with
    // this frame.
    struct thread *owner;
    unsigned ukey;
};

/* Initialize the global frame table. */
void frame_init(void);

/* Crete a frame table entry (does not insert). */
struct frame_entry *frame_create_entry(unsigned key);

/* Insert an entry into the frame table. */
void frame_insert(struct frame_entry *fe);

/* Look up a frame table entry. */
struct frame_entry *frame_lookup(unsigned key);

/* Remove an entry from the frame table. */
void frame_remove(unsigned key);

/* Returns a free frame, evicting one if necessary. */
void *frame_get(void *uaddr, bool writeable);

/* Returns a key used to identify a frame entry. */
unsigned frame_get_key(void *kaddr);

#endif /* VM_FRAME_H */
