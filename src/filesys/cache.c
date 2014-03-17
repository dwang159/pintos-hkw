#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "devices/block.h"
#include "devices/timer.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "lib/random.h"
#include <stdio.h>
#include <string.h>

#define BUF_NUM_SLOTS 64

/* Flags to describe state of a slot in the filesystem buffer. */
#define FS_BUF_UNUSED 0
#define FS_BUF_INUSE 1
#define FS_BUF_ACCESSED 2
#define FS_BUF_DIRTY (4 | FS_BUF_ACCESSED)

struct cache_slot {
    block_sector_t sect_id;
    unsigned flags;
    char content[BLOCK_SECTOR_SIZE];
};

static pid_t daemon_pid;
static bool daemon_should_live;
static struct cache_slot fs_buffer[BUF_NUM_SLOTS];
static char bounce[BLOCK_SECTOR_SIZE];

/* Determines if a sector is in the cache. */
static int buff_lookup(block_sector_t);
static bool is_slot_empty(int slot_id UNUSED) UNUSED;
static int force_empty_slot(void);
static int passive_empty_slot(void);
static void writeback(int);
static int choice_to_evict(void);
static void check_equal(block_sector_t sect, int slot_id);
static void print_slot(int);
static void set_inuse(int slot);
static void set_dirty(int slot);
static bool is_dirty(int slot);
static bool is_inuse(int slot);
static void cache_daemon(void *aux);

/* Reads the filesys sector sect into addr. Starts at offset into the
 * sector and reads size bytes. This is a read of at most *one* sector.
 * Will consult the cache and only go to the disk if necessary. */
void cache_read_spec(block_sector_t sect, void *addr, off_t offset,
        off_t size) {
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    ASSERT(size + offset <= BLOCK_SECTOR_SIZE);
    char *buff_actual;
    int slot_id;
    if ((slot_id = buff_lookup(sect)) != -1) {
        ASSERT(fs_buffer[slot_id].sect_id == sect);
        buff_actual = fs_buffer[slot_id].content;
        check_equal(sect, slot_id);
    } else {
        slot_id = force_empty_slot();
        ASSERT(0 <= slot_id);
        ASSERT(slot_id < BUF_NUM_SLOTS);
        fs_buffer[slot_id].sect_id = sect;
        fs_buffer[slot_id].flags = FS_BUF_INUSE;
        buff_actual = fs_buffer[slot_id].content;
        block_read(fs_device, sect, buff_actual);
        check_equal(sect, slot_id);
    }
    memcpy(addr, buff_actual + offset, size);
}

void cache_read(block_sector_t sect, void *addr) {
    cache_read_spec(sect, addr, 0, BLOCK_SECTOR_SIZE);
}

void cache_write(block_sector_t sect, const void *addr) {
    cache_write_spec(sect, addr, 0, BLOCK_SECTOR_SIZE);
}

/* Writes the sector sect of the filesystem block device into sect,
 * but of course checks if it is in the cache first. */
void cache_write_spec(block_sector_t sect, const void *addr, off_t offset,
        off_t size) {
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    ASSERT(size + offset <= BLOCK_SECTOR_SIZE);
    char *buff_actual;
    int slot_id;
    if ((slot_id = buff_lookup(sect)) != -1) {
        ASSERT(fs_buffer[slot_id].sect_id == sect);
        ASSERT(is_inuse(slot_id));
        buff_actual = fs_buffer[slot_id].content;
        set_dirty(slot_id);
    } else {
        slot_id = force_empty_slot();
        ASSERT(slot_id >= 0);
        ASSERT(slot_id < BUF_NUM_SLOTS);
        fs_buffer[slot_id].sect_id = sect;
        set_inuse(slot_id);
        set_dirty(slot_id);
        buff_actual = fs_buffer[slot_id].content;
        if (offset > 0 || offset + size < BLOCK_SECTOR_SIZE) {
            block_read(fs_device, sect, buff_actual);
        } else {
            memset(buff_actual, 0, BLOCK_SECTOR_SIZE);
        }
    }
    ASSERT(is_dirty(slot_id));
    memcpy(buff_actual + offset, addr, size);
    check_equal(sect, slot_id);
}

void print_slot(int id) {
    struct cache_slot c = fs_buffer[id];
    printf("id: %d sect: %d accessed: %d dirty: %u inuse: %u",
        id, c.sect_id, c.flags & FS_BUF_ACCESSED, c.flags & FS_BUF_DIRTY,
                c.flags & FS_BUF_INUSE);
}
        
/* Finds the slot that the sector is loaded into, returns
 * -1 on inability to locate it. */
int buff_lookup(block_sector_t sect) {
    int i;
    for (i = 0; i < BUF_NUM_SLOTS; i++) {
        if (is_slot_empty(i)) {
            continue;
        }
        if (fs_buffer[i].sect_id == sect) {
            return i;
        }
    }
    return -1;
}

bool is_slot_empty(int slot_id) {
    return !(fs_buffer[slot_id].flags & FS_BUF_INUSE);
}
void cache_writeback_all(void) {
    int i;
    for (i = 0; i < BUF_NUM_SLOTS; i++) {
        writeback(i);
    }
}

int force_empty_slot(void) {
    int ret = passive_empty_slot();
    if (ret == -1) {
        ret = choice_to_evict();
        writeback(ret);
    }
    ASSERT(fs_buffer[ret].flags == FS_BUF_UNUSED);
    ASSERT(ret >= 0);
    ASSERT(ret < BUF_NUM_SLOTS);
    return ret;
}
static int count = 0;
int choice_to_evict(void) {
    //random_init((unsigned) timer_ticks());
    //int num = (int) (random_ulong() % BUF_NUM_SLOTS);
    int num;
    num = count % BUF_NUM_SLOTS;
    ASSERT(num >= 0);
    ASSERT(num < BUF_NUM_SLOTS);
    count++;
    return num;
}

int passive_empty_slot(void) {
//    int i;
 //   for (i = 0; i < BUF_NUM_SLOTS; i++) {
 //       if (!is_inuse(i))
 //           return i;
 //   }
    return -1;
}

void writeback(int cache_slot) {
    if (is_dirty(cache_slot)) {
        block_write(fs_device, fs_buffer[cache_slot].sect_id,
                fs_buffer[cache_slot].content);
    }
    fs_buffer[cache_slot].flags = FS_BUF_UNUSED;
}

void check_equal(block_sector_t sect, int slot_id) {
    block_read(fs_device, sect, bounce);
    if (fs_buffer[slot_id].flags & FS_BUF_INUSE && 
            !(fs_buffer[slot_id].flags & FS_BUF_DIRTY)){
        ASSERT(!memcmp(bounce, fs_buffer[slot_id].content, 
                BLOCK_SECTOR_SIZE));
    }
}

/* Flag manipulation. */

void set_inuse(int slot) {
    fs_buffer[slot].flags |= FS_BUF_INUSE;
}

void set_dirty(int slot) {
    fs_buffer[slot].flags |= FS_BUF_DIRTY;
}

bool is_dirty(int slot) {
    return fs_buffer[slot].flags & FS_BUF_DIRTY;
}

bool is_inuse(int slot) {
    return fs_buffer[slot].flags & FS_BUF_INUSE;
}

void cache_daemon(void *aux UNUSED) {
    while (daemon_should_live) {
        cache_writeback_all();
        timer_sleep(100);
    }
}

void cache_destroy(void) {
    daemon_should_live = false;
    cache_writeback_all();
}


void cache_init(void) {
    daemon_should_live = true;
    daemon_pid = thread_create("cache_daemon", PRI_DEFAULT, cache_daemon, 
        NULL);
}
