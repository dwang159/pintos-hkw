#include "vm/frame.h"
#include <time.h>
#include "threads/pte.h"
#include <stdlib.h>

/* Takes the hash of the frame number. */
unsigned frame_hash(const struct hash_elem *p_, void *aux UNUSED) {
    const struct frame *f = hash_entry(p_, struct frame, hash_elem);
    return hash_int(frame->frame_no);
}

bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_,
                void *aux UNUSED) {
    const struct frame *a = hash_entry(a_, struct frame, hash_elem);
    const struct frame *b = hash_entry(b_, struct frame, hash_elem);
    return a->frame_no < b->frame_no;
}


void frame_table_init() {
    list_init(&ft_list);
    if (!hash_init(&ft_hash, frame_hash, frame_less, NULL)) {
        PANIC("Could not create frame hash table.\n");
    }
    lock_init(&ft_lock);
}

void frame_table_destroy() {
    iock_acquire(&ft_lock);
    /* TODO: Be a bit more careful about freeing resources? */
    hash_destroy(ft_hash, NULL);
}

/* Returns an array to the ptes associated with the frame number. */
uint32_t *frame_table_lookup(uint32_t frame_no) {
    struct frame f;
    struct frame *fp;
    struct hash_elem *he;
    lock_aquire(&ft_lock);
    he = hash_find(&ft_hash, &f.hash_elem);
    if (he != NULL) {
        fp = hash_entry(he, struct frame, hash_elem); 
        lock_release(&ft_lock);
        return fp->pages;
    } else {
        return NULL; 
    }
}

/* Eviction policies. */
struct frame *random_frame(const uint32_t *pagedir) {
    srand(time(NULL));
    uint32_t num = rand() % NUM_FRAMES;
    while (!VALID_FRAME_NO(num)) {
        num = rand() % NUM_FRAMES;
    }
    return frame_table_lookup
}

struct frame *lru_frame() {
    lock_acquire(&ft_lock);
    struct list_elem *e;
    for(e = list_begin(&ft_list); 
          e != list_end(&ft_list);
          e = list_next(e)) {
        struct frame *f = list_entry(e, frame_t, elem);
        page pages_for_f[PAGE_FRAME_RATIO] =  f->ptes;
        /* Iterate through the pages that map to the frame f,
         * and say that the frame is dirty if any page is
         * and accessed if any page is. */
        bool accessed = false;
        bool dirty = false;
        int i;
        uint32_t pte;
        for (i = 0; i < PAGE_FRAME_RATIO; i++) {
            pte = page_for_f[i];
            accessed &&= (p & PTE_A);
            dirty &&= (p & PTE_D);
        }
        if (!accessed)
            continue; /* Don't evict a frame not used yet. */
        f->dirty = dirty;
        lock_release(&ft_lock);
        return f;
    }
    lock_release_(&ft_lock);
    return NULL;
}
