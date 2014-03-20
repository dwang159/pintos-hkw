#include "devices/block.h"
#include "devices/timer.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"
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
    block_sector_t sect_id;          /* Sector stored here. */
    struct lock bflock;              /* Lock to prevent race conditions. */
    unsigned flags;                  /* Dirty, In use, Accessed. */
    char content[BLOCK_SECTOR_SIZE]; /* The actual contents on disk. */
};

/* One struct cache_slot per slot in the buffer. */
struct lock full_buf_lock;
static struct cache_slot fs_buffer[BUF_NUM_SLOTS];

/* A writeback daemon that occassional backs up the cache to disk. */
static pid_t daemon_pid;
static bool daemon_should_live;
static void cache_daemon(void *aux);

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

/* Associated a sector with the slot. */
static void set_sect(int slot_id, block_sector_t sect);

/* Flag access and mutation routines. */
static void set_inuse(int slot);
static void set_unused(int slot);
static void set_dirty(int slot);
static void clear_dirty(int slot);
static bool is_dirty(int slot);
static bool is_inuse(int slot);

/* Synchronization wrappers. */
static bool slot_try_acquire(int slot_id);
static void slot_release(int slot_id);

/* Debugging checks. */
static bool have_buffer(void);
static bool have_slot(int);
static void at_most_one(void);

/* Starts running the cache daemon at filesystem initialization */
void cache_init(void) {
     lock_init(&full_buf_lock);
     int i;
     for (i = 0; i < BUF_NUM_SLOTS; i++) {
         lock_init(&fs_buffer[i].bflock);
     }
    daemon_should_live = true;
    /*daemon_pid = thread_create("cache_daemon", PRI_DEFAULT, cache_daemon,
        NULL);*/
}

/* Kills the daemon when the filesystem is closed. */
void cache_destroy(void) {
    daemon_should_live = false;
    writeback_all();
}

/* Reads the filesys sector sect into addr. Starts at offset into the
 * sector and reads size bytes. This is a read of at most *one* sector.
 * Will consult the cache and only go to the disk if necessary. */
void cache_read_spec(block_sector_t sect, void *addr, off_t offset,
        off_t size) {
    ASSERT(offset >= 0);
    ASSERT(size >= 0);
    at_most_one();
    ASSERT(size + offset <= BLOCK_SECTOR_SIZE);

    char *buff_actual;
    int slot_id;
    ASSERT(sect <= block_size(fs_device));

    lock_acquire(&full_buf_lock);

    if ((slot_id = buff_lookup(sect)) != -1) {
        ASSERT(have_slot(slot_id));
        ASSERT(fs_buffer[slot_id].sect_id == sect);
        have_slot(slot_id);

        buff_actual = fs_buffer[slot_id].content;
        lock_release(&full_buf_lock);
    } else {
        slot_id = force_empty_slot();

        ASSERT(have_slot(slot_id));
        ASSERT(0 <= slot_id);
        at_most_one();
        ASSERT(slot_id < BUF_NUM_SLOTS);
        ASSERT(have_slot(slot_id));
        set_sect(slot_id, sect);
        set_inuse(slot_id);
        buff_actual = fs_buffer[slot_id].content;
        lock_release(&full_buf_lock);
        block_read(fs_device, sect, buff_actual);
    }
    //async_read(sect + 1);
    at_most_one();
    memcpy(addr, buff_actual + offset, size);
    slot_release(slot_id);
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
    at_most_one();
    if ((slot_id = buff_lookup(sect)) != -1) {

        ASSERT(have_slot(slot_id));
        ASSERT(fs_buffer[slot_id].sect_id == sect);
        ASSERT(is_inuse(slot_id));

        buff_actual = fs_buffer[slot_id].content;
        set_dirty(slot_id);
        lock_release(&full_buf_lock);
    } else {
        slot_id = force_empty_slot();

        ASSERT(have_slot(slot_id));
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
}

void cache_read(block_sector_t sect, void *addr) {
    cache_read_spec(sect, addr, 0, BLOCK_SECTOR_SIZE);
}

void cache_write(block_sector_t sect, const void *addr) {
    cache_write_spec(sect, addr, 0, BLOCK_SECTOR_SIZE);
}
        
/* Regularly scheduled writebacks*/
void cache_daemon(void *aux UNUSED) {
    PANIC("NOT RUNNING\n");
    while (daemon_should_live) {
        writeback_all();
        timer_sleep(100);
    }
}

/* Finds the slot that the sector is loaded into, returns
 * -1 on inability to locate it. */
int buff_lookup(block_sector_t sect) {
    ASSERT(have_buffer());
    int i;
    for (i = 0; i < BUF_NUM_SLOTS; i++) {
        if (!is_inuse(i)) {
            continue;
        }
        if (fs_buffer[i].sect_id == sect) {
            ASSERT(!have_slot(i));
            if(slot_try_acquire(i)) {
                return i;
            } else {
                return -1;
            }
        }
    }
    return -1;
}

int force_empty_slot(void) {
    ASSERT(have_buffer());
    int ret = passive_empty_slot();
    if (ret == -1) {
        ret = choice_to_evict();
        ASSERT(have_slot(ret));
        writeback(ret);
        set_unused(ret);
    }
    ASSERT(fs_buffer[ret].flags == FS_BUF_UNUSED);
    ASSERT(ret >= 0);
    ASSERT(ret < BUF_NUM_SLOTS);
    ASSERT(have_slot(ret));
    return ret;
}

int passive_empty_slot(void) {
    ASSERT(have_buffer());
    int i;
    for (i = 0; i < BUF_NUM_SLOTS; i++) {
        ASSERT(!have_slot(i));
        if (!is_inuse(i) && slot_try_acquire(i)) {
            ASSERT(have_slot(i));
            return i;
        }
    }
    return -1;
}

int choice_to_evict(void) {
    random_init((unsigned) timer_ticks());
    int num;
    do {
        at_most_one();
        num = (int) (random_ulong() % BUF_NUM_SLOTS);
        ASSERT(!have_slot(num));
    } while (!slot_try_acquire(num));
    ASSERT(num >= 0);
    ASSERT(num < BUF_NUM_SLOTS);
    return num;
}

void writeback(int cache_slot) {
    ASSERT(have_slot(cache_slot));
    if (is_dirty(cache_slot)) {
        block_write(fs_device, fs_buffer[cache_slot].sect_id,
                fs_buffer[cache_slot].content);
        clear_dirty(cache_slot);
    }
    set_inuse(cache_slot);
}

void writeback_all(void) {
    int slot;
    at_most_one();
    for (slot = 0; slot < BUF_NUM_SLOTS; slot++) {
        lock_acquire(&full_buf_lock);
        ASSERT(!have_slot(slot));
        bool got = slot_try_acquire(slot);
        lock_release(&full_buf_lock);
        if (got) {
            writeback(slot);
            slot_release(slot);
        }
    }
}

/* Flag manipulation. */
void set_sect(int slot_id, block_sector_t sect) {
    fs_buffer[slot_id].sect_id = sect;
}

void set_inuse(int slot) {
    ASSERT(have_slot(slot));
    fs_buffer[slot].flags |= FS_BUF_INUSE;
}

void set_unused(int slot) {
    ASSERT(have_slot(slot));
    fs_buffer[slot].flags = FS_BUF_UNUSED;
}

void set_dirty(int slot) {
    ASSERT(have_slot(slot));
    fs_buffer[slot].flags |= FS_BUF_DIRTY;
}

void clear_dirty(int slot) {
    ASSERT(have_slot(slot));
    fs_buffer[slot].flags &= !FS_BUF_DIRTY;
}

bool is_dirty(int slot) {
    return fs_buffer[slot].flags & FS_BUF_DIRTY;
}

bool is_inuse(int slot) {
    return fs_buffer[slot].flags & FS_BUF_INUSE;
}

/* Synchronization */
bool slot_try_acquire(int slot_id) {
    ASSERT(0 <= slot_id);
    ASSERT(slot_id < BUF_NUM_SLOTS);
    ASSERT(have_buffer());
    ASSERT(!have_slot(slot_id));
    bool out = lock_try_acquire(&fs_buffer[slot_id].bflock);
    ASSERT(out == have_slot(slot_id));
    return out;
}

void slot_release(int slot_id) {
    ASSERT(0 <= slot_id);
    ASSERT(slot_id < BUF_NUM_SLOTS);
    ASSERT(have_slot(slot_id));
    lock_release(&fs_buffer[slot_id].bflock);
    ASSERT(!have_slot(slot_id));
}

/* Pulls the sector in aux into the cache if it isn't already there. */
void read_ahead(void *aux) {
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
        lock_release(&full_buf_lock);

        ASSERT(have_slot(slot_id));
        ASSERT(0 <= slot_id);
        ASSERT(slot_id < BUF_NUM_SLOTS);

        set_sect(slot_id, sect);
        fs_buffer[slot_id].sect_id = sect;
        set_inuse(slot_id);
        ASSERT(fs_buffer[slot_id].flags == FS_BUF_INUSE);
        buff_actual = fs_buffer[slot_id].content;
        block_read(fs_device, sect, buff_actual);
        slot_release(slot_id);
    } else {
        slot_release(slot_id);
        lock_release(&full_buf_lock);
    }
}

void async_read(block_sector_t sect) {
    PANIC("Noope\n");
    thread_create("async_read", PRI_DEFAULT, read_ahead, (void *) &sect);
}

// Debugging functions.

/* Checks if thread_current() has permission to access the buffer. */
bool have_buffer(void) {
    return lock_held_by_current_thread(&full_buf_lock);
}

/* Checks if thread_current() can access this slot. */
bool have_slot(int slot_id) {
    return lock_held_by_current_thread(&fs_buffer[slot_id].bflock);
}

/* Checks that the current thread has at most one slot open. */
void at_most_one(void) {
    int i;
    int count = 0;
    for (i = 0; i < BUF_NUM_SLOTS; i++) {
        if (have_slot(i)) {
            count++;
        }
        if (count >= 2) {
            PANIC("Everything you know is wrong\n");
        }
    }
}
