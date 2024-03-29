#ifndef VM_FMAP_H
#define VM_FMAP_H

/* File mapping table.
 *
 * Contains information about files mapped in by processes using the mmap
 * syscall.
 */

#include <hash.h>

typedef int mapid_t;

/* File mapping data structure */
struct fmap_table {
    struct hash data;
};

struct fmap_entry {
    void *addr;
    int fd;
    struct file *hidden;
    unsigned num_pages;
    unsigned key;
    struct hash_elem elem;
};

/* Create a new mapping table. */
struct fmap_table *fmap_create_table(void);

void fmap_update(struct fmap_entry *fme, int fd, void *addr, 
        struct file *hidden, unsigned size);

/* Creates a unique key each time. */
mapid_t fmap_generate_id(void);

/* Create a new page table entry. */
struct fmap_entry *fmap_create_entry(mapid_t key);

/* Insert an entry into the mapping table. */
void fmap_insert(struct fmap_table *fmap, struct fmap_entry *fmape);

/* Look up a mapping table entry. */
struct fmap_entry *fmap_lookup(struct fmap_table *fmap, mapid_t key);

/* Remove an entry from the mapping table. */
void fmap_remove(struct fmap_table *fmap, mapid_t key);

/* Returns true if e1 < e2. */
bool fmap_hash_less_func(
    const struct hash_elem *e1,
    const struct hash_elem *e2,
    void *aux);

/* Hash function for the fmapping table. */
unsigned fmap_hash(const struct hash_elem *e, void *aux);

#endif /*VM_FMAP_H */
