/*
 * Functions for managing the supplemental page table.
 */

#include "page.h"
#include <stdint.h>
#include <stdbool.h>
#include <debug.h>
#include "threads/pte.h"
#include "threads/malloc.h"

/* Create a new page table. Returns a null pointer if memory
 * allocation fails.
 */
struct spt_table *spt_create_table() {
    struct spt_table *spt;

    spt = (struct spt_table *) malloc(sizeof(struct spt_table));
    if (!spt || !hash_init(&spt->data, &spt_hash, &spt_hash_less_func, NULL))
        return NULL;
    return spt;
}

/* Create a new page table entry. Returns a null pointer if
 * memory allocation fails.
 */
struct spt_entry *spt_create_entry(unsigned key) {
    struct spt_entry *spte;

    spte = (struct spt_entry *) malloc(sizeof(struct spt_entry));
    if (!spte)
        return NULL;

    spte->key = key;
    return spte;
}

/* Insert an entry into the page table. */
void spt_insert(struct spt_table *spt, struct spt_entry *spte) {
    ASSERT(spt && spte);
    hash_insert(&spt->data, &spte->elem);
}

/* Update an entry to SPT_ZERO. */
void spt_update_zero(struct spt_entry *spte) {
    ASSERT(spte);
    spte->type = SPT_ZERO;
}

/* Update an entry to SPT_FILESYS. */
void spt_update_filesys(struct spt_entry *spte,
                        struct file *file, off_t offset,
                        int read_bytes, int zero_bytes, bool writable) {
    ASSERT(spte && file);
    spte->type = SPT_FILESYS;
    spte->data.fdata.file = file;
    spte->data.fdata.offset = offset;
    spte->data.fdata.read_bytes = read_bytes;
    spte->data.fdata.zero_bytes = zero_bytes;
    spte->data.fdata.writable = writable;
}

/* Update an entry to SPT_SWAP. */
void spt_update_swap(struct spt_entry *spte, size_t slot) {
    ASSERT(spte);
    spte->type = SPT_SWAP;
    spte->data.slot = slot;
}

/* Update an entry to SPT_INVALID (invalidates the page). */
void spt_invalidate(struct spt_entry *spte) {
    ASSERT(spte);
    spte->type = SPT_INVALID;
}

/* Look up a page table entry. Returns NULL if no entry exists. */
struct spt_entry *spt_lookup(struct spt_table *spt, unsigned key) {
    struct hash_elem *e;
    struct spt_entry *cmp;

    ASSERT(spt);

    // Create a temporary struct with the same key so we can find
    // an "equal" element in the hash table.
    cmp = spt_create_entry(key);
    ASSERT(cmp);
    e = hash_find(&spt->data, &cmp->elem);
    free(cmp);

    if (!e)
        return NULL;

    return hash_entry(e, struct spt_entry, elem);
}

/* Removes an entry from the page table. */
void spt_remove(struct spt_table *spt, unsigned key) {
    struct spt_entry *cmp;

    ASSERT(spt);

    cmp = spt_create_entry(key);
    ASSERT(cmp);
    hash_delete(&spt->data, &cmp->elem);
    free(cmp);
}

/* Hashes a pointer. */
unsigned spt_hash(const struct hash_elem *e, void *aux UNUSED) {
    struct spt_entry *spte;
    ASSERT(e);
    spte = hash_entry(e, struct spt_entry, elem);
    return hash_int((int) spte->key);
}

/* Returns an a key for the hash table given a user address.
 * The key is simply the bits of the address used to determine
 * the page, with all other bits zeroed out.
 */
unsigned spt_get_key(const void *uaddr) {
    return ((unsigned) uaddr) & (PDMASK | PTMASK);
}

/* Compares two hash elems. Returns true if e1 < e2. */
bool spt_hash_less_func(const struct hash_elem *e1,
                        const struct hash_elem *e2, void *aux UNUSED) {
    struct spt_entry *spte1, *spte2;

    ASSERT(e1 && e2);

    spte1 = hash_entry(e1, struct spt_entry, elem);
    spte2 = hash_entry(e2, struct spt_entry, elem);

    return spte1->key < spte2->key;
}
