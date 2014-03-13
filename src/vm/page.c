/*
 * Functions for managing the supplemental page table.
 */

#include <stdint.h>
#include <stdbool.h>
#include <debug.h>
#include "threads/pte.h"
#include "threads/malloc.h"
#include "vm/page.h"

/* Hash table functions for the supplemental page table. */
bool spt_hash_less_func(
    const struct hash_elem *e1,
    const struct hash_elem *e2,
    void *aux);
unsigned spt_hash_func(const struct hash_elem *e, void *aux);

/* ===== Function Definitions ===== */

/* Create a new page table. Returns a null pointer if memory
 * allocation fails.
 */
struct spt_table *spt_create_table() {
    struct spt_table *spt;

    spt = (struct spt_table *) malloc(sizeof(struct spt_table));
    if (!spt || !hash_init(&spt->data, &spt_hash_func,
                &spt_hash_less_func, NULL))
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
    // Initialize as invalid.
    spte->read_status = SPT_INVALID;
    spte->write_status = SPT_INVALID;
    return spte;
}

/* Insert an entry into the page table. */
void spt_insert(struct spt_table *spt, struct spt_entry *spte) {
    ASSERT(spt && spte);
    hash_insert(&spt->data, &spte->elem);
}

/* Update an entry's read/write status. */
void spt_update_status(struct spt_entry *spte,
        enum spt_page_type read_status,
        enum spt_page_type write_status,
        bool writable) {
    ASSERT(spte);
    spte->read_status = read_status;
    spte->write_status = write_status;
    spte->writable = writable;
}

/* Update an entry's file data. */
void spt_update_filesys(struct spt_entry *spte,
                        struct file *file, off_t offset,
                        int read_bytes, int zero_bytes) {
    ASSERT(spte && file);
    spte->data.fdata.file = file;
    spte->data.fdata.offset = offset;
    spte->data.fdata.read_bytes = read_bytes;
    spte->data.fdata.zero_bytes = zero_bytes;
}

/* Update an entry's swap data. */
void spt_update_swap(struct spt_entry *spte, size_t slot) {
    ASSERT(spte);
    spte->data.slot = slot;
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
unsigned spt_hash_func(const struct hash_elem *e, void *aux UNUSED) {
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
