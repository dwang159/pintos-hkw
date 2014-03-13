#include <stdint.h>
#include <stdbool.h>
#include <debug.h>
#include "threads/pte.h"
#include "threads/malloc.h"
#include "vm/fmap.h"

static mapid_t key_giver = 0;

/* Create a new mapping table. Returns a null pointer if memory
 * allocation fails.
 */
struct fmap_table *fmap_create_table() {
    struct fmap_table *fmap;

    fmap = (struct fmap_table *) malloc(sizeof(struct fmap_table));
    if (!fmap || 
            !hash_init(&fmap->data, &fmap_hash, &fmap_hash_less_func, NULL))
        return NULL;
    return fmap;
}

mapid_t fmap_generate_id(void) {
    // Increment the global key_giver so that it doesn't repeat itself.
    return key_giver++;
}

/* Create a new mapping table entry. Returns a null pointer if
 * memory allocation fails.  */
struct fmap_entry *fmap_create_entry(mapid_t key) {
    struct fmap_entry *fme;
    fme = (struct fmap_entry *) malloc(sizeof(struct fmap_entry));
    if (!fme)
        return NULL;

    fme->key = key;
    return fme;
}

/* Insert an entry into the mapping table. */
void fmap_insert(struct fmap_table *fmap, struct fmap_entry *fme) {
    ASSERT(fmap && fme);
    hash_insert(&fmap->data, &fme->elem);
}

/* Look up a mapping table entry. Returns NULL if no entry exists. */
struct fmap_entry *fmap_lookup(struct fmap_table *fmap, mapid_t key) {
    struct hash_elem *e;
    struct fmap_entry *fme;

    ASSERT(fmap);

    // Create a temporary struct with the same key so we can find
    // an "equal" element in the hash table.
    fme = fmap_create_entry(key);
    ASSERT(fme);
    e = hash_find(&fmap->data, &fme->elem);
    free(fme);

    if (!e)
        return NULL;

    return hash_entry(e, struct fmap_entry, elem);
}

/* Removes an entry from the mapping table. */
void fmap_remove(struct fmap_table *fmap, mapid_t key) {
    struct fmap_entry *fme;

    ASSERT(fmap);

    fme = fmap_create_entry(key);
    ASSERT(fme);
    hash_delete(&fmap->data, &fme->elem);
    free(fme);
}

/* Hashes a mapping */
unsigned fmap_hash(const struct hash_elem *e, void *aux UNUSED) {
    struct fmap_entry *fme;
    ASSERT(e);
    fme = hash_entry(e, struct fmap_entry, elem);
    return hash_int((int) fme->key);
}

/* Compares two hash elems. Returns true if e1 < e2. */
bool fmap_hash_less_func(const struct hash_elem *e1,
                        const struct hash_elem *e2, void *aux UNUSED) {
    struct fmap_entry *fme1, *fme2;

    ASSERT(e1 && e2);

    fme1 = hash_entry(e1, struct fmap_entry, elem);
    fme2 = hash_entry(e2, struct fmap_entry, elem);

    return fme1->key < fme2->key;
}

//TODO fmap table destructor (also, incidentally, spt desctructor)
