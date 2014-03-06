#ifndef VM_PAGE_H
#define VM_PAGE_H

/* Supplemental page table.
 *
 * Contains additional information about each page contained in the
 * page table (defined in userprog/pagedir.h).
 */

#include <hash.h>

/* Data structure for a supplemental page table. */
struct spt_table {
  struct hash data;
};

/* Data structure for a supplemental page table entry. */
struct spt_entry {
  // Element used to store entry in page table.
  struct hash_elem elem;

  // Virtual address of start of page. Used to identify page.
  void *page_uaddr;
};

/* Create a new page table. */
struct spt_table *spt_create_table(void);

/* Create a new page table entry. */
struct spt_entry *spt_create_entry(void);

/* Look up a page table entry. */
struct spt_entry *spt_lookup(struct spt_table *spt, void *uaddr);


#endif /* VM_PAGE_H */
