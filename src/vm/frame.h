#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#define NUM_FRAMES (1 << 20) 
#define PAGE_FRAME_RATIO 1
typedef void frame_table_t;/* Not defined yet. */
typedef void frame_t;
typedef uint32_t *page_dir_t;
typedef void *supp_page_tab_t;
typedef void* file_map_tab_t;
typedef void* page_t;

/* A policy should not mutate, and it should only need to consult a
 * frame table to see which frames are older, and a page table
 * to examine dirty and accessed bits. */
typedef struct frame *policy_t(const frame_table_t, const uint32_t *);

/* The structure corresponding to a frame table entry.
 * Records its position in ft_hash, ft_list, as well as
 * the frame number and the pages associated with the page. */
struct frame {
    struct hash_elem hash_elem;
    struct list_elem list_elem;
    uint32_t frame_no;
    bool dirty;
    uint32_t ptes[PAGE_FRAME_RATIO];
};

struct list ft_list;
struct hash ft_hash;

/* Initializes the frame table for the process that has 
 * just started. */
void frame_table_init(void);

/* Evicts the frame decided by the policy pol, and returns that
 * frame number. Needs to clear the entry in the frame table,
 * the page table, move the contents into swap or write it back to
 * the file that it belongs to. */
frame_t frame_evict(frame_table_t ft, page_table_t, 
                    supp_page_tab_t, file_map_tab_t,
                    policy_t pol);
/* Frees all frames associated with the exiting process. */
void frame_free_all(supp_page_tab_t, file_map_tab_t);
/* Mutators and accessors for the flags corresponding to frm in ft. */
void frame_table_set_flags(frame_table_t ft, frame_t frm, flags_t flags);
flags_t frame_table_get_flags(frame_t frm);

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
