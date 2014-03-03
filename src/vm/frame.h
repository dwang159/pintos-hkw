#ifndef VM_FRAME_H
#define VM_FRAME_H

typedef void frame_table_t;/* Not defined yet. */
typedef void frame_t;
typedef uint32_t *page_dir_t;
typedef void supp_page_tab_t;
typedef void file_map_tab_t;
typedef void page_t;

/* A policy should not mutate, and it should only need to consult a
 * frame table to see which frames are older, and a page table
 * to examine dirty and accessed bits. */
typedef frame_t (*policy_t)(const frame_table_t, const page_table_t);

/* Initializes the frame table for the process that has 
 * just started. */
void frame_table_init(void);

/* Retrieves the pages associated with the frame in the frame table.
 * Returns a 0 terminated list, so presumes that 0 isn't a valid
 * page number. */
page_t *frame_get_pages(frame_table_t ft, frame_t frm);

/* Evicts the frame decided by the policy pol, and returns that
 * frame number. Needs to clear the entry in the frame table,
 * the page table, move the contents into swap or write it back to
 * the file that it belongs to. */
frame_t frame_evict(frame_table_t ft, page_table_t, 
                    supp_page_tab_t, file_map_tab_t
                    policy_t pol);
/* Frees all frames associated with the exiting process. */
void frame_free_all(frame_table_t, page_table_t, 
                    supp_page_tab_t, file_map_tab_t);
/* Mutators and accessors for the flags corresponding to frm in ft. */
void frame_table_set_flags(frame_table_t ft, frame_t frm, flags_t flags);
flags_t frame_table_get_flags(const frame_table_t ft, frame_t frm);

/* Adds or removes a page to the list associated with that frame in
 * the frame table. */
void frame_table_add_vpage(frame_table_t ft, frame_t frm, page_t vp);
void frame_table_rem_vpage(frame_table_t ft, frame_t frm, page_t vp);

policy_t random_frame;
policy_t lru_frame;
#endif /* VM_FRAME_H */
