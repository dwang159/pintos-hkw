#ifndef VM_PAGE_H
#define VM_PAGE_H

/* Supplemental page table.
 *
 * Contains additional information about each page contained in the
 * page table (defined in userprog/pagedir.h).
 */

#include <hash.h>
#include "filesys/file.h"
#include "vm/swap.h"

/* Possible values for where to read and write data. */
enum spt_page_type {
    SPT_INVALID,
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

    // Page status (where to read and write data).
    enum spt_page_type read_status;
    enum spt_page_type write_status;

    // Additional data about the page, depending on the page type.
    union {
        // Struct to keep information on the location of the
        // page within the file.
        struct {
            struct file *file;
            off_t offset;
            int read_bytes;
            int zero_bytes;
        } fdata;
        // Used if associated with swap.
        size_t slot;
    } data;

    bool writable;
};

/* Create a new page table. */
struct spt_table *spt_create_table(void);

/* Create a new page table entry (does not insert). */
struct spt_entry *spt_create_entry(unsigned key);

/* Insert an entry into the page table. */
void spt_insert(struct spt_table *spt, struct spt_entry *spte);

/* Update an entry's read/write status. */
void spt_update_status(struct spt_entry *spte,
        enum spt_page_type read_status,
        enum spt_page_type write_status,
        bool writable);

/* Update an entry's file data. */
void spt_update_filesys(struct spt_entry *spte,
                        struct file *file, off_t offset,
                        int read_bytes, int zero_bytes);

/* Update an entry's swap data. */
void spt_update_swap(struct spt_entry *spte, size_t slot);

/* Look up a page table entry. */
struct spt_entry *spt_lookup(struct spt_table *spt, unsigned key);

/* Remove an entry from the page table. */
void spt_remove(struct spt_table *spt, unsigned key);

/* Returns a key to look up an entry in the hash table given a uaddr. */
unsigned spt_get_key(const void *uaddr);

#endif /* VM_PAGE_H */
