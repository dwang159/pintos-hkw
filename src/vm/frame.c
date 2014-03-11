#include "vm/frame.h"
#include "vm/page.h"
#include <devices/timer.h>
#include <lib/random.h>
#include "threads/pte.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include <stdlib.h>
#include <stdio.h>

typedef uint32_t flags_t;
static int ft_num_free = NUM_FRAMES;

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

struct frame *frame_create_entry(frame_number_t frame_no) {
    struct frame *fp;
    fp = (struct frame *) malloc(sizeof(struct spt_entry));
    if (!fp)
        return NULL;
    fp->frame_no = frame_no;
    return fp;
}

/* Returns an array to the pageinfos associated with the frame number. */
pageinfo *frame_table_lookup(frame_number_t frame_no) {
    struct frame *fp;
    fp = find_frame(frame_no);
    if (fp != NULL) {
        return fp->pages;
    } else {
        return NULL; 
    }
}

struct frame *find_frame(frame_number_t frame_no) {
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
        lock_release(&ft_lock);
        return NULL;
    }
}

/* Adds or removes a page to the array associated with that frame in
 * the frame table. */
void frame_table_add_vpage(frame_number_t frame_no, uint32_t pte) {
    lock_acquire(&ft_lock);
    struct frame *fp = find_frame(frame_no);
    bool found = false;
    int i;
    for (i = 0; i < PAGE_FRAME_RATIO; i++) {
        if (fp->pages[i].pte != 0)
            continue;
        else {
            fp->pages[i].pte = pte;
            fp->pages[i].owner = thread_tid();
            found = true;
            break;
        }
    }
    lock_release(&ft_lock);
    ASSERT(found);
}

void frame_table_rem_vpage(frame_number_t frame_no, uint32_t pte) {
    lock_acquire(&ft_lock);
    struct frame *fp = find_frame(frame_no);
    int i;
    for (i = 0; i < PAGE_FRAME_RATIO; i++) {
        if (fp->pages[i].pte == pte) {
            fp->pages[i].pte = 0;
            return;
        }
    }
    lock_release(&ft_lock);
}

void frame_table_set_flags(frame_number_t frame_no, flags_t flags) {
    printf("frame_table_set_flags called with %u\n", frame_no);
    printf("Attempted flags were %d\n", flags);
    printf("Error: not implemented\n");
}

flags_t frame_table_get_flags(frame_number_t frame_no) {
    printf("frame_table_get_flags called with %u\n", frame_no);
    printf("Error: not implemented\n");
    return -1;
}

/* Evicts the frame decided by the policy pol, and returns that
 * frame number. Needs to clear the entry in the frame table,
 * the page table, move the contents into swap or write it back to
 * the file that it belongs to. */
frame_number_t frame_get(policy_t pol, void *vaddr, bool writeable) {
    struct frame *fp;
    uint32_t pte;
    frame_number_t frame_no;
    lock_acquire(&ft_lock);
    void *p = palloc_get_page(PAL_USER);
    if (p) {
        frame_no = (frame_number_t) p;
        pte = pte_create_user(vaddr, writeable);
        fp = frame_create_entry(frame_no);
        hash_insert(&ft_hash, &(fp->hash_elem));
    } else {
        PANIC("No evicting");
        fp = pol();
        if (fp->dirty) {
            frame_writeback(fp);
        }
        uint32_t *pd = thread_current()->pagedir;
        pagedir_clear_page(pd, (void *) pte);
        frame_no = fp->frame_no;
    }
    fp->pages[0].pte = pte;
    fp->pages[0].owner = thread_tid();
    fp->dirty = false;
    lock_release(&ft_lock);
    return frame_no;
}

/* Frees all frames associated with the exiting process. */
void frame_free_all(void) {
    tid_t curr = thread_tid();
    struct hash_iterator i;
    hash_first(&i, &ft_hash);
    while (hash_next(&i)) {
        struct frame *fp = hash_entry(hash_cur(&i), 
                                      struct frame, 
                                      hash_elem);
        int i;
        for (i = 0; i < PAGE_FRAME_RATIO; i++) {
            if (fp-> pages[i].pte != 0 && fp->pages[i].owner == curr) {
                uint32_t *pd = thread_current()->pagedir;
                frame_writeback(fp);
                pagedir_clear_page(pd, (void *)fp->pages[0].pte);
                fp->pages[i].pte = 0;
                ft_num_free++;
            }
        }
    }
}

/* Writes the frame back to the swap partition or to a file. */
void frame_writeback(struct frame *fp) {
    struct spt_table *curr_spt = thread_current()->spt;
    int i;
    unsigned key;
    uint32_t *pd;
    for (i = 0; i < PAGE_FRAME_RATIO; i++) {
        if (fp->pages[i].pte != 0) {
            key = spt_get_key((void *) fp->pages[i].pte);
            struct spt_entry *se = spt_lookup(curr_spt, key);
            pd = thread_current()->pagedir;
            switch (se->type) {
            case SPT_INVALID:
                printf("Invalid memory access.\n");
                sys_exit(-1);
                break;
            case SPT_ZERO:
                if (pagedir_is_dirty(pd, (void *)key)) {
                    se->data.slot = swap_swalloc_and_write((void *) key);
                    se->type = SPT_SWAP;
                } else {
                    spt_remove(curr_spt, key);
                }
                break;
            case SPT_SWAP:
                se->data.slot = swap_swalloc_and_write((void *) key);
                pagedir_clear_page(pd, (void *)key);
                break;
            case SPT_FILESYS:
                if (pagedir_is_dirty(pd, (void *)key)) {
                    struct file *handle = se->data.fdata.file;
                    off_t offset = se->data.fdata.offset;
                    uintptr_t kpage = vtop((void *) key);
                    file_write_at(handle, (void *) kpage, PGSIZE, offset);
                }
                break;
            default:
                PANIC("Memory corruption in SPT.\n");
            }
        }
        void *page = pte_get_page(fp->pages[i].pte);
        palloc_free_page(page);
    }
}

/* Eviction policies. */
struct frame *random_frame(void) {
    random_init((unsigned) timer_ticks());
    frame_number_t num;
    num = (frame_number_t) random_ulong() % NUM_FRAMES;
    while (!VALID_FRAME_NO(num)) {
        num = (frame_number_t) random_ulong() % NUM_FRAMES;
    }
    return find_frame(num);
}

struct frame *lru_frame(void) {
    /* This might be nonsensical. */
    struct list_elem *e;
    for(e = list_begin(&ft_list); 
          e != list_end(&ft_list);
          e = list_next(e)) {
        struct frame *f = list_entry(e, struct frame, list_elem);
        pageinfo *pages_for_f = f->pages;
        /* Iterate through the pages that map to the frame f,
         * and say that the frame is dirty if any page is
         * and accessed if any page is. */
        bool accessed = false;
        bool dirty = false;
        int i;
        uint32_t pte;
        for (i = 0; i < PAGE_FRAME_RATIO; i++) {
            pte = pages_for_f[i].pte;
            pages_for_f[i].pte &= ~PTE_A; /* Turn off the accessed bit. */
            accessed = accessed || (pte & PTE_A);
            dirty = dirty || (pte & PTE_D);
        }
        if (accessed) {
            continue;
        }
        f->dirty = dirty;
        lock_release(&ft_lock);
        return f;
    }
    return NULL;
}
