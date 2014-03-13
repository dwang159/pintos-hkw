#ifndef VM_MAP_H
#define VM_MAP_H

/* File mapping table.
 *
 * Contains information about files mapped in by processes using the mmap
 * syscall.
 */

#include <hash.h>


/* File mapping data structure */
struct map_table {
    struct hash data;
};

struct map_entry {
    void *addr;
    unsigned size;
    unsigned key;
    struct hash_elem elem;
};

/* Create a new mapping table. */
struct map_table *map_create_table(void);

/* Create a new page table entry. */
struct map_entry *map_create_entry(unsigned key);

/* Insert an entry into the mapping table. */
void map_insert(struct map_table *map, struct map_entry *mape);

/* Look up a mapping table entry. */
struct map_entry *map_lookup(struct map_table *map, unsigned key);

/* Remove an entry from the mapping table. */
void map_remove(struct map_table *map, unsigned key);

/* Returns true if e1 < e2. */
bool map_hash_less_func(
    const struct hash_elem *e1,
    const struct hash_elem *e2,
    void *aux);

/* Hash function for the mapping table. */
unsigned map_hash(const struct hash_elem *e, void *aux);

#endif /*VM_MAP_H */
