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

/* Look up a page table entry. */
struct spt_entry *spt_lookup(struct spt_table *spt, unsigned key) {
    struct hash_elem *e;
    struct spt_entry *cmp;

    // Create a temporary struct with the same key so we can find
    // an "equal" element in the hash table.
    cmp = spt_create_entry(key);
    ASSERT(cmp);
    e = hash_find(&spt->data, &cmp->elem);
    free(cmp);

    return hash_entry(e, struct spt_entry, elem);
}

/* Removes an entry from the page table. */
void spt_remove(struct spt_table *spt, unsigned key) {
    struct spt_entry *cmp;

    cmp = spt_create_entry(key);
    ASSERT(cmp);
    hash_delete(&spt->data, &cmp->elem);
    free(cmp);
}

/* Hashes a pointer. */
unsigned spt_hash(const struct hash_elem *e, void *aux UNUSED) {
    struct spt_entry *spte = hash_entry(e, struct spt_entry, elem);
    return hash_int((int) spte->key);
}

/* Returns an a key for the hash table given a user address.
 * The key is simply the bits of the address used to determine
 * the page, with all other bits zeroed out.
 */
unsigned spt_get_key(void *uaddr) {
    return ((unsigned) uaddr) & (PDMASK | PTMASK);
}

/* Compares two hash elems. Returns true if e1 < e2. */
bool spt_hash_less_func(
        const struct hash_elem *e1,
        const struct hash_elem *e2,
        void *aux UNUSED) {
    struct spt_entry *spte1, *spte2;

    spte1 = hash_entry(e1, struct spt_entry, elem);
    spte2 = hash_entry(e2, struct spt_entry, elem);

    return spte1->key < spte2->key;
}
