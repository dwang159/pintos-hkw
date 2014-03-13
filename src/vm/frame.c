/*
 * Functions for managing the frame table.
 */

#include <stdint.h>
#include <stdbool.h>
#include <debug.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"

/* Global frame table. */
struct frame_table ft;

/* Lock for the frame table. */
struct lock ft_lock;

/* Global page eviction policy. */
struct frame_entry *(*evict)(void);

/* Page eviction policies. */
struct frame_entry *evict_first(void);

/* Hash table functions for the frame table. */
unsigned frame_hash_func(const struct hash_elem *e, void *aux);
bool frame_hash_less_func(
        const struct hash_elem *e1,
        const struct hash_elem *e2,
        void *aux);

/* Write back a frame to either swap or file. */
void write_back(struct frame_entry *fe);

/* ===== Function Definitions ===== */

/* Initialize the global frame table. */
void frame_init(void) {
    // Initialize the hash table.
    if (!hash_init(&ft.data, &frame_hash_func, &frame_hash_less_func, NULL))
        PANIC("Could not initialize frame table\n");

    // Initialize the lock.
    lock_init(&ft_lock);

    // Set global page eviction policy.
    evict = &evict_first;
}

/* Crete a frame table entry (does not insert). */
struct frame_entry *frame_create_entry(unsigned key) {
    struct frame_entry *fe;

    fe = (struct frame_entry *) malloc(sizeof(struct frame_entry));
    if (!fe)
        return NULL;

    fe->key = key;
    return fe;
}

/* Insert an entry into the frame table. */
void frame_insert(struct frame_entry *fe) {
    ASSERT(fe);
    hash_insert(&ft.data, &fe->elem);
}

/* Look up a frame table entry. */
struct frame_entry *frame_lookup(unsigned key) {
    struct frame_entry *cmp;
    struct hash_elem *e;

    // Create a temporary struct with the same key to compare.
    cmp = frame_create_entry(key);
    ASSERT(cmp);
    e = hash_find(&ft.data, &cmp->elem);
    free(cmp);

    if (!e)
        return NULL;
    return hash_entry(e, struct frame_entry, elem);
}

/* Remove an entry from the frame table. */
void frame_remove(unsigned key) {
    struct frame_entry *cmp;

    cmp = frame_create_entry(key);
    ASSERT(cmp);
    hash_delete(&ft.data, &cmp->elem);
    free(cmp);
}

/* Returns a free frame, evicting one if necessary. */
void *frame_get(void *uaddr, bool writable) {
    struct frame_entry *fe;
    // Key is the kernel address, ukey is the user address.
    unsigned key, ukey;
    void *upage;

    ASSERT(is_user_vaddr(uaddr));
    ukey = spt_get_key(uaddr);
    upage = (void *) ukey;

    // Try to get a page.
    lock_acquire(&ft_lock);
    void *kpage = palloc_get_page(PAL_USER);
    if (kpage) {
        key = frame_get_key(kpage);
    } else {
        // Choose a page to evict.
        fe = evict();
        key = fe->key;
        kpage = (void *) key;
        // If necessary, write back the contents of this frame.
        write_back(fe);
        // Unmap this page.
        pagedir_clear_page(fe->owner->pagedir, (void *) fe->ukey);
        // Old page is no longer needed.
        free(fe);
    }
    // Create a new frame and insert it into the frame table.
    fe = frame_create_entry(key);
    ASSERT(fe);
    // Set the owner thread/spt.
    fe->owner = thread_current();
    fe->ukey = ukey;
    frame_insert(fe);
    // Associate the user page with the returned kpage.
    install_page(upage, kpage, writable);

    lock_release(&ft_lock);
    return kpage;
}

/* Returns a key used to identify a frame entry. The key is simply
 * the top bits of the address used to determine the frame, with all
 * other bits zeroed out.
 */
unsigned frame_get_key(void *kaddr) {
    return ((unsigned) kaddr) & (PDMASK | PTMASK);
}

/* Hashes a hash element. */
unsigned frame_hash_func(const struct hash_elem *e, void *aux UNUSED) {
    struct frame_entry *fe;
    ASSERT(e);
    fe = hash_entry(e, struct frame_entry, elem);
    return hash_int((int) fe->key);
}

/* Compares two hash elems. Returns true if e1 < e2. */
bool frame_hash_less_func(
        const struct hash_elem *e1,
        const struct hash_elem *e2,
        void *aux UNUSED) {
    struct frame_entry *fe1, *fe2;
    ASSERT(e1 && e2);
    fe1 = hash_entry(e1, struct frame_entry, elem);
    fe2 = hash_entry(e2, struct frame_entry, elem);
    return fe1->key < fe2->key;
}
/* Writes a frame table entry back to whence it came. */
void frame_writeback(struct frame_entry *fe) {
    fe++;
    /* TODO */
    return;
}

/* Evicts the first frame it sees with the accessed bit unset. If
 * none are found, evicts the last one it saw.
 */
struct frame_entry *evict_first() {
    struct hash_iterator hi;
    struct frame_entry *fe;
    hash_first(&hi, &ft.data);
    while (hash_next(&hi)) {
        fe = hash_entry(hash_cur(&hi), struct frame_entry, elem);
        // Check if accessed.
        if (!pagedir_is_accessed(fe->owner->pagedir, (void *) fe->ukey))
            return fe;
        else
            pagedir_set_accessed(fe->owner->pagedir, (void *) fe->ukey, false);
    }
    // If none were found, return the last one we saw.
    return fe;
}

/* Write back a frame to either swap or file. */
void write_back(struct frame_entry *fe) {
    struct spt_entry *spte;
    void *kpage, *upage;

    ASSERT(fe);
    kpage = (void *) fe->key;
    upage = (void *) fe->ukey;
    spte = spt_lookup(fe->owner->spt, fe->ukey);
    size_t slotid;
    switch (spte->write_status) {
    case SPT_SWAP:
        slotid = swap_swalloc_and_write(kpage);
        // Set the read-from location.
        spt_update_status(spte, SPT_SWAP, SPT_SWAP, spte->writable);
        spt_update_swap(spte, slotid);
        break;
    case SPT_FILESYS:
        // If dirty bit is set, write back. This also takes care of
        // read-only files - the dirty bit will never be set.
        if (pagedir_is_dirty(fe->owner->pagedir, upage)) {
            file_write_at(spte->data.fdata.file,
                    kpage, PGSIZE, spte->data.fdata.offset);
        }
        // Clear the dirty bit.
        pagedir_set_dirty(fe->owner->pagedir, upage, false);
        spt_update_status(spte, SPT_FILESYS, SPT_FILESYS, spte->writable);
        break;
    default:
        PANIC("Write back failed.\n");
    }
}
