#include <stdint.h>
#include <stdbool.h>
#include <debug.h>
#include "threads/pte.h"
#include "threads/malloc.h"
#include "vm/map.h"

/* Create a new mapping table. Returns a null pointer if memory
 * allocation fails.
 */
struct map_table *map_create_table() {
    struct map_table *map;

    map = (struct map_table *) malloc(sizeof(struct map_table));
    if (!map || !hash_init(&map->data, &map_hash, &map_hash_less_func, NULL))
        return NULL;
    return map;
}

/* Create a new mapping table entry. Returns a null pointer if
 * memory allocation fails.
 */
struct map_entry *map_create_entry(unsigned key) {
    struct map_entry *mape;

    mape = (struct map_entry *) malloc(sizeof(struct map_entry));
    if (!mape)
        return NULL;

    mape->key = key;
    return mape;
}

/* Insert an entry into the mapping table. */
void map_insert(struct map_table *map, struct map_entry *mape) {
    ASSERT(map && mape);
    hash_insert(&map->data, &mape->elem);
}

/* Look up a mapping table entry. Returns NULL if no entry exists. */
struct map_entry *map_lookup(struct map_table *map, unsigned key) {
    struct hash_elem *e;
    struct map_entry *cmp;

    ASSERT(map);

    // Create a temporary struct with the same key so we can find
    // an "equal" element in the hash table.
    cmp = map_create_entry(key);
    ASSERT(cmp);
    e = hash_find(&map->data, &cmp->elem);
    free(cmp);

    if (!e)
        return NULL;

    return hash_entry(e, struct map_entry, elem);
}

/* Removes an entry from the mapping table. */
void map_remove(struct map_table *map, unsigned key) {
    struct map_entry *cmp;

    ASSERT(map);

    cmp = map_create_entry(key);
    ASSERT(cmp);
    hash_delete(&map->data, &cmp->elem);
    free(cmp);
}

/* Hashes a mapping */
unsigned map_hash(const struct hash_elem *e, void *aux UNUSED) {
    struct map_entry *mape;
    ASSERT(e);
    mape = hash_entry(e, struct map_entry, elem);
    return hash_int((int) mape->key);
}

/* Compares two hash elems. Returns true if e1 < e2. */
bool map_hash_less_func(const struct hash_elem *e1,
                        const struct hash_elem *e2, void *aux UNUSED) {
    struct map_entry *mape1, *mape2;

    ASSERT(e1 && e2);

    mape1 = hash_entry(e1, struct map_entry, elem);
    mape2 = hash_entry(e2, struct map_entry, elem);

    return mape1->key < mape2->key;
}

//TODO map table destructor (also, incidentally, spt desctructor)
