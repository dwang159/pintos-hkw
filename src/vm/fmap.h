#ifndef VM_FMAP_H
#define VM_FMAP_H

/* File mapping table.
 *
 * Contains information about files mapped in by processes using the mmap
 * syscall.
 */

#include <hash.h>


/* File mapping data structure */
struct fmap_table {
    struct hash data;
};

struct fmap_entry {
    void *addr;
    unsigned size;
    unsigned key;
    struct hash_elem elem;
};

/* Create a new mapping table. */
struct fmap_table *fmap_create_table(void);

/* Create a new page table entry. */
struct fmap_entry *fmap_create_entry(unsigned key);

/* Insert an entry into the mapping table. */
void fmap_insert(struct fmap_table *fmap, struct fmap_entry *fmape);

/* Look up a mapping table entry. */
struct fmap_entry *fmap_lookup(struct fmap_table *fmap, unsigned key);

/* Remove an entry from the mapping table. */
void fmap_remove(struct fmap_table *fmap, unsigned key);

/* Returns true if e1 < e2. */
bool fmap_hash_less_func(
    const struct hash_elem *e1,
    const struct hash_elem *e2,
    void *aux);

/* Hash function for the fmapping table. */
unsigned fmap_hash(const struct hash_elem *e, void *aux);

#endif /*VM_FMAP_H */
