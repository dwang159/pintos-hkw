/*
 * Functions for managing the supplemental page table.
 */

#include "page.h"
#include <stdint.h>
#include <stdbool.h>
#include <debug.h>
#include "threads/pte.h"
#include "threads/malloc.h"

unsigned spt_hash(void *uaddr);
unsigned spt_get_key(void *uaddr);
bool spt_entry_cmp(struct spt_entry *e1, struct spt_entry *e2);

/* Create a new page table. Returns a null pointer if memory
 * allocation fails.
 */
struct spt_table *spt_create_table() {
    struct spt_table *spt;

    spt = (struct spt_table *) malloc(sizeof(struct spt_table));
    if (!spt || !hash_init(&spt->data, &spt_hash, &spt_entry_cmp, NULL))
        return NULL;
    return spt;
}

/* Create a new page table entry. Returns a null pointer if
 * memory allocation fails.
 */
struct spt_entry *spt_create_entry() {
    struct spt_entry *spte;

    spte = (struct spt_entry *) malloc(sizeof(struct spt_entry));
    if (!spte)
        return NULL;
    return spte;
}

/* Look up a page table entry. */
struct spt_entry *spt_lookup(struct spt_table *spt, void *uaddr) {
    struct hash_elem *e;

    // TODO
}


/* Hashes a pointer. */
unsigned spt_hash(hash_elem *e) {
    struct spt_entry *spte = hash_entry(e, struct spt_entry, elem);
    return hash_int((int) spt_get_key(spte->page_uaddr));
}

/* Returns an a key for the hash table given a user address.
 * The key is simply the bits of the address used to determine
 * the page, with all other bits zeroed out.
 */
unsigned spt_get_key(void *uaddr) {
    return uaddr & (PDMASK | PTMASK);
}
