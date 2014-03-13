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
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

/* Global frame table. */
struct frame_table ft;

/* Lock for the frame table. */
struct lock ft_lock;

/* Global page eviction policy. */
struct frame_entry *(*evict)(void);

/* Page eviction policies. */
struct frame_entry *random_policy(void);
struct frame_entry *lru_policy(void);

/* Hash table functions for the frame table. */
unsigned frame_hash_func(const struct hash_elem *e, void *aux);
bool frame_hash_less_func(
        const struct hash_elem *e1,
        const struct hash_elem *e2,
        void *aux);

/* ===== Function Definitions ===== */

/* Initialize the global frame table. */
void frame_init(void) {
    // Initialize the hash table.
    if (!hash_init(&ft.data, &frame_hash_func, &frame_hash_less_func, NULL))
        PANIC("Could not initialize frame table\n");

    // Initialize the lock.
    lock_init(&ft_lock);

    // Set global page eviction policy.
    evict = &random_policy;
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
    unsigned key = frame_get_key(uaddr);
    void *upage;

    ASSERT(is_user_vaddr(uaddr));
    upage = (void *) key;
    // Try to get a page.
    lock_acquire(&ft_lock);
    void *kpage = palloc_get_page(PAL_USER);
    if (kpage) {
        fe = frame_create_entry(key);
        ASSERT(fe);
        frame_insert(fe);
        install_page(upage, kpage, writable);
    } else {
        PANIC("NO EVICTING\n");
    }
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

/* Evicts a random frame. */
struct frame_entry *random_policy() {
    // TODO
    return NULL;
}

/* Evicts the last recently used frame. */
struct frame_entry *lru_policy() {
    // TODO
    return NULL;
}
