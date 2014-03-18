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
    struct lock bflock;
    unsigned flags;
    char content[BLOCK_SECTOR_SIZE];
};

struct lock full_buf_lock;
static struct cache_slot fs_buffer[BUF_NUM_SLOTS];

static char bounce[BLOCK_SECTOR_SIZE];

static pid_t daemon_pid;
static bool daemon_should_live;

/* Finds the index of the sector in the cache if present */
static int buff_lookup(block_sector_t);

/* Routines to find an empty slot, or create an empty slot */
static int force_empty_slot(void);
static int passive_empty_slot(void);
static int choice_to_evict(void);

/* Physical writes to the disk, if necessary. */
static void writeback(int);
static void writeback_all(void);

/* Thread func for reading ahead in the background, and caller. */
static void read_ahead(void *aux);
static void async_read(block_sector_t sect);

/* Debugging routine that ensures the contents of the sector
 * is identical to the contents of the slot, unless the slot is
 * dirty. */
static void check_equal(block_sector_t sect, int slot_id);

// Flag access and mutation routines
static void set_inuse(int slot);
static void set_dirty(int slot);
static void clear_dirty(int slot);
static bool is_dirty(int slot);
static bool is_inuse(int slot);

/* Synchronization wrappers. */
static void slot_acquire(int slot_id);
static void slot_release(int slot_id);

/* Debugging checks. */
static void have_buffer(void);
static void have_slot(int);

void have_buffer(void) {
    ASSERT(lock_held_by_current_thread(&full_buf_lock));
}

void have_slot(int slot_id) {
    ASSERT(lock_held_by_current_thread(&fs_buffer[slot_id].bflock));
}

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
    ASSERT(sect <= block_size(fs_device));

    lock_acquire(&full_buf_lock);

    if ((slot_id = buff_lookup(sect)) != -1) {
        ASSERT(fs_buffer[slot_id].sect_id == sect);
        buff_actual = fs_buffer[slot_id].content;
        slot_acquire(slot_id);
        lock_release(&full_buf_lock);
        //check_equal(sect, slot_id);
    } else {
        slot_id = force_empty_slot();
        ASSERT(0 <= slot_id);
        ASSERT(slot_id < BUF_NUM_SLOTS);
        fs_buffer[slot_id].sect_id = sect;
        fs_buffer[slot_id].flags = FS_BUF_INUSE;
        buff_actual = fs_buffer[slot_id].content;
        slot_acquire(slot_id);
        lock_release(&full_buf_lock);
        block_read(fs_device, sect, buff_actual);
        //check_equal(sect, slot_id);
    }
    //async_read(sect + 1);
    memcpy(addr, buff_actual + offset, size);
    slot_release(slot_id);
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
    lock_acquire(&full_buf_lock);
    if ((slot_id = buff_lookup(sect)) != -1) {
        ASSERT(fs_buffer[slot_id].sect_id == sect);
        ASSERT(is_inuse(slot_id));
        buff_actual = fs_buffer[slot_id].content;
        slot_acquire(slot_id);
        set_dirty(slot_id);
        lock_release(&full_buf_lock);
    } else {
        slot_id = force_empty_slot();
        slot_acquire(slot_id);
        ASSERT(slot_id >= 0);
        ASSERT(slot_id < BUF_NUM_SLOTS);
        fs_buffer[slot_id].sect_id = sect;
        set_inuse(slot_id);
        set_dirty(slot_id);
        buff_actual = fs_buffer[slot_id].content;
        lock_release(&full_buf_lock);
        if (offset > 0 || offset + size < BLOCK_SECTOR_SIZE) {
            block_read(fs_device, sect, buff_actual);
        } else {
            memset(buff_actual, 0, BLOCK_SECTOR_SIZE);
        }
    }
    ASSERT(is_dirty(slot_id));
    memcpy(buff_actual + offset, addr, size);
    slot_release(slot_id);
    //check_equal(sect, slot_id);
}
        
/* Finds the slot that the sector is loaded into, returns
 * -1 on inability to locate it. */
int buff_lookup(block_sector_t sect) {
    have_buffer();
    int i;
    for (i = 0; i < BUF_NUM_SLOTS; i++) {
        if (!is_inuse(i)) {
            continue;
        }
        if (fs_buffer[i].sect_id == sect) {
            return i;
        }
    }
    return -1;
}

int force_empty_slot(void) {
    have_buffer();
    int ret = passive_empty_slot();
    if (ret == -1) {
        ret = choice_to_evict();
        slot_acquire(ret);
        writeback(ret);
        slot_release(ret);
        fs_buffer[ret].flags = FS_BUF_UNUSED;
    }
    ASSERT(fs_buffer[ret].flags == FS_BUF_UNUSED);
    ASSERT(ret >= 0);
    ASSERT(ret < BUF_NUM_SLOTS);
    return ret;
}

int passive_empty_slot(void) {
    have_buffer();
    int i;
    for (i = 0; i < BUF_NUM_SLOTS; i++) {
        if (!is_inuse(i)) {
            return i;
        }
    }
    return -1;
}

int choice_to_evict(void) {
    random_init((unsigned) timer_ticks());
    int num = (int) (random_ulong() % BUF_NUM_SLOTS);
    ASSERT(num >= 0);
    ASSERT(num < BUF_NUM_SLOTS);
    return num;
}

void writeback(int cache_slot) {
    have_slot(cache_slot);
    if (is_dirty(cache_slot)) {
        block_write(fs_device, fs_buffer[cache_slot].sect_id,
                fs_buffer[cache_slot].content);
        clear_dirty(cache_slot);
    }
    fs_buffer[cache_slot].flags = FS_BUF_INUSE;
}

void writeback_all(void) {
    int i;
    for (i = 0; i < BUF_NUM_SLOTS; i++) {
        lock_acquire(&full_buf_lock);
        slot_acquire(i);
        lock_release(&full_buf_lock);
        writeback(i);
        slot_release(i);
    }
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
    have_slot(slot);
    fs_buffer[slot].flags |= FS_BUF_INUSE;
}

void set_dirty(int slot) {
    have_slot(slot);
    fs_buffer[slot].flags |= FS_BUF_DIRTY;
}

void clear_dirty(int slot) {
    have_slot(slot);
    fs_buffer[slot].flags &= !FS_BUF_DIRTY;
}

bool is_dirty(int slot) {
    return fs_buffer[slot].flags & FS_BUF_DIRTY;
}

bool is_inuse(int slot) {
    return fs_buffer[slot].flags & FS_BUF_INUSE;
}

/* Synchronization */
void slot_acquire(int slot_id) {
    ASSERT(0 <= slot_id);
    ASSERT(slot_id < BUF_NUM_SLOTS);
    have_buffer();
    lock_acquire(&fs_buffer[slot_id].bflock);;
}

void slot_release(int slot_id) {
    ASSERT(0 <= slot_id);
    ASSERT(slot_id < BUF_NUM_SLOTS);
    lock_release(&fs_buffer[slot_id].bflock);
}

/* Regularly scheduled writebacks*/
static void cache_daemon(void *aux);
void cache_daemon(void *aux UNUSED) {
    while (daemon_should_live) {
        writeback_all();
        timer_sleep(100);
    }
}

static void read_ahead(void *aux) {
    block_sector_t sect = *(block_sector_t *) aux;
    int slot_id;
    char *buff_actual;
    if (sect > block_size(fs_device)) {
        /* Don't want to read past the bounds of the device. */
        return;
    }
    lock_acquire(&full_buf_lock);
    if ((slot_id = buff_lookup(sect)) == -1) {
        slot_id = force_empty_slot();
        ASSERT(0 <= slot_id);
        ASSERT(slot_id < BUF_NUM_SLOTS);
        fs_buffer[slot_id].sect_id = sect;
        fs_buffer[slot_id].flags = FS_BUF_INUSE;
        buff_actual = fs_buffer[slot_id].content;
        slot_acquire(slot_id);
        lock_release(&full_buf_lock);
        block_read(fs_device, sect, buff_actual);
        slot_release(slot_id);
        //check_equal(sect, slot_id);
    } else {
        lock_release(&full_buf_lock);
    }
}

static void async_read(block_sector_t sect) {
    thread_create("async_read", PRI_DEFAULT, read_ahead, (void *) &sect);
}

/* Starts running the cache daemon at filesystem initialization */
void cache_init(void) {
     lock_init(&full_buf_lock);
     int i;
     for (i = 0; i < BUF_NUM_SLOTS; i++) {
         lock_init(&fs_buffer[i].bflock);
     }
    daemon_should_live = true;
    daemon_pid = thread_create("cache_daemon", PRI_DEFAULT, cache_daemon, 
        NULL);
}
/* Kills the daemon when the filesystem is closed. */
void cache_destroy(void) {
    daemon_should_live = false;
    writeback_all();
}
