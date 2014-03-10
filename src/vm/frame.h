#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include "kernel/list.h"
#include "threads/thread.h"
#include "lib/kernel/hash.h"
#define NUM_FRAMES (1 << 20) 
#define PAGE_FRAME_RATIO 1
#define VALID_FRAME_NO(x) ((0 < x) && (x < NUM_FRAMES))

/* A policy should not mutate, and it should only need to consult a
 * frame table to see which frames are older, and a page table
 * to examine dirty and accessed bits. */
typedef struct frame *policy_t(void);

typedef struct _pageinfo {
    uint32_t pte;
    pid_t owner;
} pageinfo;

/* The structure corresponding to a frame table entry.
 * Records its position in ft_hash, ft_list, as well as
 * the frame number and the pages associated with the page. */
struct frame {
    struct hash_elem hash_elem;
    struct list_elem list_elem;
    uint32_t frame_no;
    bool dirty;
    pageinfo ptes[PAGE_FRAME_RATIO];
};

struct list ft_list;
struct hash ft_hash;
struct lock ft_lock;

void frame_table_init(void);
void frame_table_destroy(void);

void frame_table_add_vpage(uint32_t frame_no, uint32_t page_no);
void frame_table_rem_vpage(uint32_t frame_no, uint32_t page_no);

struct frame *find_frame(uint32_t frame_no);
pageinfo *frame_table_lookup(uint32_t frame_no);

void frame_table_set_flags(uint32_t frame_no, uint32_t flags);
uint32_t frame_table_get_flags(uint32_t frame_no);

uint32_t frame_evict(policy_t pol, uint32_t pte);
void frame_free_all(void);
void frame_writeback(struct frame *);

/* Adds or removes a page to the array associated with that frame in
 * the frame table. */
void frame_table_add_vpage(uint32_t frame_no, uint32_t page_no);
void frame_table_rem_vpage(uint32_t frame_no, uint32_t page_no);

/* Returns the pte's associated with the frame as a null
 * terminated array. */
uint32_t *frame_table_get_ptes(uint32_t frame_no);

policy_t random_frame;
policy_t lru_frame;
#endif /* VM_FRAME_H */
