#include "vm/frame.h"
#include <devices/timer.h>
#include <lib/random.h>
#include "threads/pte.h"
#include <stdlib.h>
#include <stdio.h>

typedef uint32_t flags_t;

hash_hash_func frame_hash;
hash_less_func frame_less;

/* Takes the hash of the frame number. */
unsigned frame_hash(const struct hash_elem *p_, void *aux UNUSED) {
    const struct frame *f = hash_entry(p_, struct frame, hash_elem);
    return hash_int(f->frame_no);
}

bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_,
                void *aux UNUSED) {
    const struct frame *a = hash_entry(a_, struct frame, hash_elem);
    const struct frame *b = hash_entry(b_, struct frame, hash_elem);
    return a->frame_no < b->frame_no;
}

/* Initializes the frame table for the process that has 
 * just started. */

void frame_table_init() {
    list_init(&ft_list);
    if (!hash_init(&ft_hash, frame_hash, frame_less, NULL)) {
        PANIC("Could not create frame hash table.\n");
    }
    lock_init(&ft_lock);
}

void frame_table_destroy() {
    lock_acquire(&ft_lock);
    /* TODO: Be a bit more careful about freeing resources? */
    hash_destroy(&ft_hash, NULL);
}

/* Returns an array to the pageinfos associated with the frame number. */
pageinfo *frame_table_lookup(uint32_t frame_no) {
    struct frame *fp;
    fp = find_frame(frame_no);
    if (fp != NULL) {
        return fp->ptes;
    } else {
        return NULL; 
    }
}

struct frame *find_frame(uint32_t frame_no) {
    struct frame f;
    struct frame *fp;
    struct hash_elem *he;
    lock_acquire(&ft_lock);
    f.frame_no = frame_no;
    he = hash_find(&ft_hash, &f.hash_elem);
    if (he != NULL) {
        fp = hash_entry(he, struct frame, hash_elem);
        lock_release(&ft_lock);
        return fp;
    } else {
        return NULL;
    }
}

/* Adds or removes a page to the array associated with that frame in
 * the frame table. */
void frame_table_add_vpage(uint32_t frame_no, uint32_t page_no) {
    printf("Attempted to add page %u from frame %u\n", page_no,
                                                          frame_no);
    printf("Error: not implemented\n");
}

void frame_table_rem_vpage(uint32_t frame_no, uint32_t page_no) {
    printf("Attempted to remove page %u from frame %u\n", page_no,
                                                          frame_no);
    printf("Error: not implemented\n");
}

void frame_table_set_flags(uint32_t frame_no, flags_t flags) {
    printf("frame_table_set_flags called with %u\n", frame_no);
    printf("Attempted flags were %d\n", flags);
    printf("Error: not implemented\n");
}

flags_t frame_table_get_flags(uint32_t frame_no) {
    printf("frame_table_get_flags called with %u\n", frame_no);
    printf("Error: not implemented\n");
    return -1;
}

/* Evicts the frame decided by the policy pol, and returns that
 * frame number. Needs to clear the entry in the frame table,
 * the page table, move the contents into swap or write it back to
 * the file that it belongs to. */
uint32_t frame_evict(policy_t pol) {
    printf("frame_evict called with policy %p\n", pol);
    printf("But not implemented...\n");
    return -1;
}

/* Eviction policies. */
struct frame *random_frame() {
    random_init((unsigned) timer_ticks());
    uint32_t num;
    num = (uint32_t) random_ulong() % NUM_FRAMES;
    while (!VALID_FRAME_NO(num)) {
        num = (uint32_t) random_ulong() % NUM_FRAMES;
    }
    return find_frame(num);
}

struct frame *lru_frame() {
    lock_acquire(&ft_lock);
    struct list_elem *e;
    for(e = list_begin(&ft_list); 
          e != list_end(&ft_list);
          e = list_next(e)) {
        struct frame *f = list_entry(e, struct frame, list_elem);
        pageinfo *pages_for_f =  f->ptes;
        /* Iterate through the pages that map to the frame f,
         * and say that the frame is dirty if any page is
         * and accessed if any page is. */
        bool accessed = false;
        bool dirty = false;
        int i;
        uint32_t pte;
        for (i = 0; i < PAGE_FRAME_RATIO; i++) {
            pte = pages_for_f[i].pte;
            accessed = accessed && (pte & PTE_A);
            dirty = dirty && (pte & PTE_D);
        }
        if (!accessed)
            continue; /* Don't evict a frame not used yet. */
        f->dirty = dirty;
        lock_release(&ft_lock);
        return f;
    }
    lock_release(&ft_lock);
    return NULL;
}