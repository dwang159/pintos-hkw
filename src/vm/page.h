#ifndef VM_PAGE_H
#define VM_PAGE_H

/* Supplemental page table.
 *
 * Contains additional information about each page contained in the
 * page table (defined in userprog/pagedir.h).
 */

#include <hash.h>

// Page types. Can be a page of zeros, a page to be read from the
// filesystem, or a page to be read from swap.
enum spt_page_type {
    SPT_ZERO,
    SPT_FILESYS,
    SPT_SWAP
};

/* Data structure for a supplemental page table. */
struct spt_table {
    struct hash data;
};

/* Data structure for a supplemental page table entry. */
struct spt_entry {
    // Element used to store entry in page table.
    struct hash_elem elem;

    // Key used to identify page. Should be the uaddr of the page with
    // the lower 12 bits zeroed out.
    unsigned key;

    // Page type.
    enum spt_page_type type;

    // Additional data about the page, depending on the page type.
    union {
    } data;
};

/* Create a new page table. */
struct spt_table *spt_create_table(void);

/* Create a new page table entry. */
struct spt_entry *spt_create_entry(unsigned key);

/* Look up a page table entry. */
struct spt_entry *spt_lookup(struct spt_table *spt, unsigned key);

/* Remove an entry from the page table. */
void spt_remove(struct spt_table *spt, unsigned key);

/* Returns a key to look up an entry in the hash table given a uaddr. */
unsigned spt_get_key(void *uaddr);

/* Returns true if e1 < e2. */
bool spt_hash_less_func(
    const struct hash_elem *e1,
    const struct hash_elem *e2,
    void *aux);

/* Hash function for the supplemental page table. */
unsigned spt_hash(const struct hash_elem *e, void *aux);

#endif /* VM_PAGE_H */
