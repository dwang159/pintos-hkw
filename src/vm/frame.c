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
}

/* Returns an array to the ptes associated with the frame number. */
uint32_t *frame_table_lookup(uint32_t frame_no) {
    struct frame f;
    struct frame *fp;
    struct hash_elem *he;
    he = hash_find(&ft_hash, &f.hash_elem);
    if (he != NULL) {
        fp = hash_entry(he, struct frame, hash_elem); 
        return fp->pages;
    } else {
        return NULL; 
    }
}

/* Eviction policies. */
uint32_t random_frame(const frame_table_t, const page_table_t) {
    srand(time(NULL));
    return rand() % NUM_FRAMES;
}

struct frame *lru_frame(const frame_table_t ft, const uint32_t *pt) {
    struct list frame_list = frame_table_queue(ft);
    struct list_elem *e;
    for(e = list_begin(&frame_list); 
          e != list_end(&frame_list);
          e = list_next(e)) {
        struct frame *f = list_entry(e, frame_t, elem);
        page pages_for_f[PAGE_FRAME_RATIO] = frame_table_lookup(ft, f);
        /* Iterate through the pages that map to the frame f,
         * and say that the frame is dirty if any page is
         * and accessed if any page is. */
        bool accessed = false;
        bool dirty = false;
        int i;
        uint32_t pte;
        for (i = 0; i < PAGE_FRAME_RATIO; i++) {
            pte = page_for_f[i];
            accessed &= !!(p & PTE_A);
            dirty &= !!(p & PTE_D);
        }
        if (!accessed)
            continue; /* Don't evict a frame not used yet. */
        f->dirty = dirty;
        return f;
    }
    return NULL;
}
