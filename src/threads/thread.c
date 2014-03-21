#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "threads/malloc.h"

#ifdef USERPROG
#include "userprog/process.h"
#endif

/*! Random value for struct thread's `magic' member.
    Used to detect stack overflow.  See the big comment at the top
    of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* The ready queue is implemented as an array of list structs, where the
 * index of the list struct corresponds to a thread's priority.
 */
static struct list ready_lists[PRI_NUM];

/*! List of all processes.  Processes are added to this list
    when they are first scheduled and removed when they exit. */
static struct list all_list;

/* List of sleeping processes. */
static struct list sleep_list;

/*! Idle thread. */
static struct thread *idle_thread;

/*! Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/*! Lock used by allocate_tid(). */
static struct lock tid_lock;

/*! Stack frame for kernel_thread(). */
struct kernel_thread_frame {
    void *eip;                  /*!< Return address. */
    thread_func *function;      /*!< Function to call. */
    void *aux;                  /*!< Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;    /*!< # of timer ticks spent idle. */
static long long kernel_ticks;  /*!< # of timer ticks in kernel threads. */
static long long user_ticks;    /*!< # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /*!< # of timer ticks to give each thread. */
static unsigned thread_ticks;   /*!< # of timer ticks since last yield. */

// Keeps track of threads' exit statuses.
bool thread_exit_status_initialized;
struct vector thread_exit_status;

/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

/*! Load average for the system */
static fixed_point_t load_avg = inttofp(0);

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *running_thread(void);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static bool is_thread(struct thread *) UNUSED;
static void *alloc_frame(struct thread *, size_t size);
static void schedule(void);
void thread_schedule_tail(struct thread *prev);
static tid_t allocate_tid(void);
static struct thread *get_highest_ready_thread(void);
static void yield_if_higher_priority(struct thread *other);


static void update_priority(struct thread *t, void *aux);
static void update_recent_cpu(struct thread *t, void *aux);
static void update_load_avg(int num_ready);

static int ready_lists_size(void);

/*! Initializes the threading system by transforming the code
    that's currently running into a thread.  This can't work in
    general and it is possible in this case only because loader.S
    was careful to put the bottom of the stack at a page boundary.

    Also initializes the run queue and the tid lock.

    After calling this function, be sure to initialize the page allocator
    before trying to create any threads with thread_create().

    It is not safe to call thread_current() until this function finishes. */
void thread_init(void) {
    int i;
    ASSERT(intr_get_level() == INTR_OFF);

    lock_init(&tid_lock);
    list_init(&all_list);
    list_init(&sleep_list);

    thread_exit_status_initialized = false;

    /* Initialize each list in ready_lists. */
    for (i = PRI_MIN; i <= PRI_MAX; i++) {
        list_init(&ready_lists[i]);
    }

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread();
    init_thread(initial_thread, "main", PRI_DEFAULT);
    initial_thread->status = THREAD_RUNNING;
    initial_thread->tid = allocate_tid();
    initial_thread->nice = 0;
    initial_thread->recent_cpu = 0;
    initial_thread->dir = NULL;
}

/*! Starts preemptive thread scheduling by enabling interrupts.
    Also creates the idle thread. */
void thread_start(void) {
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init(&idle_started, 0);
    thread_create("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down(&idle_started);
}

/*! Called by the timer interrupt handler at each timer tick.
    Thus, this function runs in an external interrupt context. */
void thread_tick(int64_t ticks) {
    struct thread *t = thread_current();

    /* Update statistics. */
    if (t == idle_thread)
        idle_ticks++;
#ifdef USERPROG
    else if (t->pagedir != NULL)
        user_ticks++;
#endif
    else
        kernel_ticks++;

    // Iterate through sleeping list, waking up threads if needed
    struct list_elem *i;
    for (i = list_begin(&sleep_list);
         i != list_end(&sleep_list);
         i = list_next(i)) {
        t = list_entry( i, struct thread, sleepelem);
        if (ticks >= t->wait_ticks) {
            list_remove(i);
            thread_wake(t);
            continue;
        }
        break;
    }

    // Update the priority, system load, and recent_cpu in
    // the advanced scheduler
    if (thread_mlfqs) {
        t = thread_current();
        t->recent_cpu = fpaddint(t->recent_cpu, 1);
        if (timer_ticks() % 4 == 0) {
            update_priority(thread_current(), NULL);
            if (intr_get_level() == INTR_ON)
                yield_if_higher_priority(NULL);
        }
        if (timer_ticks() % TIMER_FREQ == 0) {
            /* Update the load average on the second. */
            update_load_avg(ready_lists_size() + 1);
            thread_foreach(update_priority, NULL);
            thread_foreach(update_recent_cpu, NULL);
        }
    }

    /* Enforce preemption. */
    if (++thread_ticks >= TIME_SLICE)
        intr_yield_on_return();
}

int ready_lists_size() {
    int num_ready = 0;
    int pri;
    for (pri = PRI_MIN; pri <= PRI_MAX; pri++) {
        num_ready += list_size(&ready_lists[pri]);
    }
    return num_ready;
}

/*! Prints thread statistics. */
void thread_print_stats(void) {
    printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
           idle_ticks, kernel_ticks, user_ticks);
}

/*! Creates a new kernel thread named NAME with the given initial PRIORITY,
    which executes FUNCTION passing AUX as the argument, and adds it to the
    ready queue.  Returns the thread identifier for the new thread, or
    TID_ERROR if creation fails.

    If thread_start() has been called, then the new thread may be scheduled
    before thread_create() returns.  It could even exit before thread_create()
    returns.  Contrariwise, the original thread may run for any amount of time
    before the new thread is scheduled.  Use a semaphore or some other form of
    synchronization if you need to ensure ordering.

    The code provided sets the new thread's `priority' member to PRIORITY, but
    no actual priority scheduling is implemented.  Priority scheduling is the
    goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority, thread_func *function,
                    void *aux) {
    struct thread *t;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;

    ASSERT(function != NULL);

    /* Allocate thread. */
    t = palloc_get_page(PAL_ZERO);
    if (t == NULL)
        return TID_ERROR;

    /* Initialize thread. */
    init_thread(t, name, priority);
    t->nice = thread_get_nice(); /* Nice of child is same as parent */

    if (thread_mlfqs) { /* 4.4 BSD Scheduler */
        t->recent_cpu = thread_current()->recent_cpu;
        update_priority(t, NULL);
    }

    tid = t->tid = allocate_tid();
    t->dir = thread_current()->dir;

    // We can't initialize this in thread_init, so we do it here.
    if (!thread_exit_status_initialized) {
        vector_init(&thread_exit_status);
        // Fill it with zeros up to the current tid.
        vector_zeros(&thread_exit_status, tid);
        thread_exit_status_initialized = true;
    }

    // Add this thread to the exit status list. The thread is indexed
    // by its tid, so vector[tid] should access the struct for this thread.
    // Since tid is incremental, we can simply append to do this.
    struct exit_state *es;
    es = (struct exit_state *) malloc(sizeof(struct exit_state));
    ASSERT(es);
    es->parent = thread_current()->tid;
    sema_init(&es->waiting, 0);
    vector_append(&thread_exit_status, es);

    /* Stack frame for kernel_thread(). */
    kf = alloc_frame(t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;

    /* Stack frame for switch_entry(). */
    ef = alloc_frame(t, sizeof *ef);
    ef->eip = (void (*) (void)) kernel_thread;

    /* Stack frame for switch_threads(). */
    sf = alloc_frame(t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;

    /* Add to run queue. */
    thread_unblock(t);

    return tid;
}

/* Returns true if e1 has lower priority than e2. Used to compare
 * thread priorities.
 */
bool thread_cmp(const struct list_elem *e1,
                const struct list_elem *e2,
                void*aux_ UNUSED) {
    return (list_entry(e1, struct thread, elem)->priority
            < list_entry(e2, struct thread, elem)->priority);
}

/*! Puts the current thread to sleep.  It will not be scheduled
    again until awoken by thread_unblock().

    This function must be called with interrupts turned off.  It is usually a
    better idea to use one of the synchronization primitives in synch.h. */
void thread_block(void) {
    ASSERT(!intr_context());
    ASSERT(intr_get_level() == INTR_OFF);

    thread_current()->status = THREAD_BLOCKED;
    schedule();
}

/* Puts the current thread to sleep on the sleeping list. It will be placed
 * back on the ready queue after the global ticks value reaches the saved
 * value in the thread's internal state.
 */
void thread_sleep(void) {
    ASSERT(!intr_context());

    struct thread * cur = thread_current();
    cur->status = THREAD_SLEEPING;
    // Insert into the sleeping list, maintaining a sorted order by
    // ascending wait_ticks values.
    enum intr_level old_level = intr_disable();
    list_insert_ordered(&sleep_list, &cur->sleepelem, &sleep_cmp, NULL);
    schedule();
    intr_set_level(old_level);
}

/* Comparison function for inserting into the sleeping list. Compares the
 * wait_ticks of each element's thread
 */
bool sleep_cmp(const struct list_elem * e1,
               const struct list_elem * e2,
               void * aux_ UNUSED) {
    return (list_entry(e1, struct thread, sleepelem)->wait_ticks
                < list_entry(e2, struct thread, sleepelem)->wait_ticks);
}

/*! Transitions a blocked thread T to the ready-to-run state.  This is an
    error if T is not blocked.  (Use thread_yield() to make the running
    thread ready.) */

void thread_unblock(struct thread *t) {
    enum intr_level old_level;

    ASSERT(is_thread(t));

    old_level = intr_disable();
    ASSERT(t->status == THREAD_BLOCKED);
    list_push_back(&ready_lists[t->priority], &t->elem);
    t->status = THREAD_READY;
    intr_set_level(old_level);
    if (intr_get_level() == INTR_ON) {
        yield_if_higher_priority(t);
    }
}

/* Wakes a sleeping thread T into a ready state. */
void thread_wake(struct thread *t) {
    enum intr_level old_level;

    ASSERT(is_thread(t));

    old_level = intr_disable();
    ASSERT(t->status == THREAD_SLEEPING);
    list_push_back(&ready_lists[t->priority], &t->elem);
    t->status = THREAD_READY;
    intr_set_level(old_level);
    if (intr_get_level() == INTR_ON) {
        yield_if_higher_priority(t);
    }
}

/*! Returns the name of the running thread. */
const char * thread_name(void) {
    return thread_current()->name;
}

/*! Returns the running thread.
    This is running_thread() plus a couple of sanity checks.
    See the big comment at the top of thread.h for details. */
struct thread * thread_current(void) {
    struct thread *t = running_thread();

    /* Make sure T is really a thread.
       If either of these assertions fire, then your thread may
       have overflowed its stack.  Each thread has less than 4 kB
       of stack, so a few big automatic arrays or moderate
       recursion can cause stack overflow. */
    ASSERT(is_thread(t));
    ASSERT(t->status == THREAD_RUNNING);

    return t;
}

/*! Returns the running thread's tid. */
tid_t thread_tid(void) {
    return thread_current()->tid;
}

/*! Deschedules the current thread and destroys it.  Never
    returns to the caller. */
void thread_exit(void) {
    ASSERT(!intr_context());

#ifdef USERPROG
    process_exit();
#endif

    /* Release all held locks */
    struct list_elem *e;
    struct list *lock_list = &thread_current()->locks_held;

    for (e = list_begin(lock_list); e != list_end(lock_list);
            e = list_next(e)) {
        lock_release(list_entry(e, struct lock, elem));
    }

    /* Remove thread from all threads list, set our status to dying,
       and schedule another process.  That process will destroy us
       when it calls thread_schedule_tail(). */
    intr_disable();

    // Wake up the parent if it was waiting on this thread. This indicates
    // that this thread has exited. We do this inside the interrupt
    // disabled section so that the parent can't preempt the thread before
    // it completely finishes exiting.
    struct exit_state *es = thread_exit_status.data[thread_current()->tid];
    sema_up(&es->waiting);

    list_remove(&thread_current()->allelem);
    thread_current()->status = THREAD_DYING;
    schedule();
    NOT_REACHED();
}

/*! Yields the CPU.  The current thread is not put to sleep and
    may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void) {
    struct thread *cur = thread_current();
    enum intr_level old_level;

    ASSERT(!intr_context());

    old_level = intr_disable();
    if (cur != idle_thread)
        list_push_back(&ready_lists[cur->priority], &cur->elem);
    cur->status = THREAD_READY;
    schedule();
    intr_set_level(old_level);
}

/*! Invoke function 'func' on all threads, passing along 'aux'.
    This function must be called with interrupts off. */
void thread_foreach(thread_action_func *func, void *aux) {
    struct list_elem *e;

    ASSERT(intr_get_level() == INTR_OFF);

    for (e = list_begin(&all_list); e != list_end(&all_list);
         e = list_next(e)) {
        struct thread *t = list_entry(e, struct thread, allelem);
        func(t, aux);
    }
}

/*! Sets the current thread's base priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority) {
    /* Calls to thread_set_priority are ignored in 4.4BSD */
    if (thread_mlfqs)
        return;

    struct thread *curr = thread_current();
    if (curr->base_priority < curr->priority) {
        if (new_priority >= curr->priority) {
            curr->base_priority = curr->priority = new_priority;
        } else {
            curr->base_priority = new_priority;
        }
    } else {
        curr->base_priority = curr->priority = new_priority;
    }
    if (intr_get_level() == INTR_ON)
        yield_if_higher_priority(NULL);
}

/* Reset's the current thread's priority after releasing a lock. If
 * the thread holds other locks, then we set the priority to the
 * highest of the threads waiting on its other locks. Otherwise we
 * reset it to the base priority.
 */
void thread_reset_priority() {
    if (thread_mlfqs)
        return;
    struct thread *curr = thread_current();
    int highest_priority = PRI_MIN - 1;
    struct list_elem *e;
    struct lock *l;

    for (e = list_begin(&curr->locks_held);
            e != list_end(&curr->locks_held);
            e = list_next(e)) {
        l = list_entry(e, struct lock, elem);
        if (l->highest_wait_priority > highest_priority)
            highest_priority = l->highest_wait_priority;
    }
    /* The priority should never be smaller than the base priority.
     * This also handles the case where there are no other locks.
     */
    if (curr->base_priority > highest_priority) {
        highest_priority = curr->base_priority;
    }

    curr->priority = highest_priority;
    if (intr_get_level() == INTR_ON)
        yield_if_higher_priority(NULL);
}

/* Temporarily elevates the receiving thread's priority to new_priority.
 * If new_priority is less than the receiving thread's current priority, it
 * does nothing.
 */
void thread_donate_priority(struct thread *receiver, int new_priority) {
    if (thread_mlfqs)
        return;
    struct thread *next;

    ASSERT(is_thread(receiver));
    if (new_priority <= receiver->priority)
        return;

    /* Raise the priority to the new priority. This does not change
     * the base priority.
     */
    receiver->priority = new_priority;

    switch (receiver->status) {
    case THREAD_READY:
        /* Remove the thread from its current list in the ready queue
         * and add it to the new list.
         */
        list_remove(&receiver->elem);
        list_push_back(&ready_lists[receiver->priority], &receiver->elem);
        break;
    case THREAD_BLOCKED:
        /* If the receiving thread is blocked because it is waiting on
         * a lock owned by another thread, then we also need to elevate
         * the priority of the thread it is waiting on. We also may need
         * to update the lock's highest_wait_priority.
         */
        if (receiver->lock_requested) {
            if (receiver->lock_requested->highest_wait_priority <
                    new_priority) {
                receiver->lock_requested->highest_wait_priority = new_priority;
            }
            next = receiver->lock_requested->holder;
            thread_donate_priority(next, new_priority);
        }
        break;
    case THREAD_SLEEPING:
        /* Do nothing; when it wakes up everything will be good. */
        break;
    default:
        /* This function should never need to be called on a thread
         * that is in the state THREAD_RUNNING (it shouldn't be donating
         * priority to itself) or THREAD_DYING.
         */
        ASSERT(false);
    }
}

/*! Returns the current thread's priority. */
int thread_get_priority(void) {
    return thread_current()->priority;
}

/*! Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice) {
    /* If the attempted nice value passed is not in range [-20, 20],
     * convert it to be so. */
    nice = (nice >= -20) ? nice : -20;
    nice = (nice <= 20) ? nice : 20;
    struct thread *t = thread_current();
    t->nice = nice;
    update_priority(t, NULL);
    if (intr_get_level() == INTR_ON)
         yield_if_higher_priority(NULL);
}

/*! Returns the current thread's nice value. */
int thread_get_nice(void) {
    return thread_current()->nice;
}

/*! Returns 100 times the system load average. */
int thread_get_load_avg(void) {
    fixed_point_t temp = fpmulint(load_avg, 100);
    return fptoint(temp);
}

void update_priority(struct thread *t, void *aux_ UNUSED) {

    fixed_point_t rcpu = t->recent_cpu;
    int nice = t->nice;
    int np = PRI_MAX - 2 * nice;
    rcpu = fpdivint(rcpu, 4);
    np -= fptoint(rcpu);
    np = (np > PRI_MAX) ? PRI_MAX : np;
    np = (np < PRI_MIN) ? PRI_MIN : np;
    t->priority = np;

    /* If the thread is on the ready queue, we need to change its location. */
    if (t->status == THREAD_READY) {
        list_remove(&t->elem);
        list_push_back(&ready_lists[t->priority], &t->elem);
    }
}

void update_recent_cpu(struct thread *t, void *aux_ UNUSED) {
    /* new_rcpu = (2 * load) / (2 * load + 1) * rcpu + nice */
    fixed_point_t numer = fpmulint(load_avg, 2);
    fixed_point_t denom = fpaddint(numer, 1);
    fixed_point_t quot = fpdiv(numer, denom);
    quot = fpmul(quot, t->recent_cpu);
    t->recent_cpu = fpaddint(quot, t->nice);
}

void update_load_avg(int num_ready) {
    if (thread_current() == idle_thread) {
        num_ready = 0;
    }
    fixed_point_t numer = fpmulint(load_avg, 59);
    numer = fpaddint(numer, num_ready);
    load_avg = fpdivint(numer, 60);
}

/*! Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void) {
    fixed_point_t new_rcpu = fpmulint(thread_current()->recent_cpu, 100);
    return fptoint(new_rcpu);
}

/*! Idle thread.  Executes when no other thread is ready to run.

    The idle thread is initially put on the ready list by thread_start().
    It will be scheduled once initially, at which point it initializes
    idle_thread, "up"s the semaphore passed to it to enable thread_start()
    to continue, and immediately blocks.  After that, the idle thread never
    appears in the ready list.  It is returned by next_thread_to_run() as a
    special case when the ready list is empty. */
static void idle(void *idle_started_ UNUSED) {
    struct semaphore *idle_started = idle_started_;
    idle_thread = thread_current();
    sema_up(idle_started);

    for (;;) {
        /* Let someone else run. */
        intr_disable();
        thread_block();

        /* Re-enable interrupts and wait for the next one.

           The `sti' instruction disables interrupts until the completion of
           the next instruction, so these two instructions are executed
           atomically.  This atomicity is important; otherwise, an interrupt
           could be handled between re-enabling interrupts and waiting for the
           next one to occur, wasting as much as one clock tick worth of time.

           See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
           7.11.1 "HLT Instruction". */
        asm volatile ("sti; hlt" : : : "memory");
    }
}

/*! Function used as the basis for a kernel thread. */
static void kernel_thread(thread_func *function, void *aux) {
    ASSERT(function != NULL);

    intr_enable();       /* The scheduler runs with interrupts off. */
    function(aux);       /* Execute the thread function. */
    thread_exit();       /* If function() returns, kill the thread. */
}

/*! Returns the running thread. */
struct thread * running_thread(void) {
    uint32_t *esp;

    /* Copy the CPU's stack pointer into `esp', and then round that
       down to the start of a page.  Because `struct thread' is
       always at the beginning of a page and the stack pointer is
       somewhere in the middle, this locates the curent thread. */
    asm ("mov %%esp, %0" : "=g" (esp));
    return pg_round_down(esp);
}

/*! Returns true if T appears to point to a valid thread. */
static bool is_thread(struct thread *t) {
    return t != NULL && t->magic == THREAD_MAGIC;
}

/*! Does basic initialization of T as a blocked thread named NAME. */
static void init_thread(struct thread *t, const char *name, int priority) {
    enum intr_level old_level;

    ASSERT(t != NULL);
    ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT(name != NULL);

    memset(t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy(t->name, name, sizeof t->name);
    t->stack = (uint8_t *) t + PGSIZE;
    t->base_priority = t->priority = priority;
    t->lock_requested = NULL;
    t->magic = THREAD_MAGIC;

    list_init(&t->locks_held);

    old_level = intr_disable();
    list_push_back(&all_list, &t->allelem);
    intr_set_level(old_level);
}

/*! Allocates a SIZE-byte frame at the top of thread T's stack and
    returns a pointer to the frame's base. */
static void * alloc_frame(struct thread *t, size_t size) {
    /* Stack data is always allocated in word-size units. */
    ASSERT(is_thread(t));
    ASSERT(size % sizeof(uint32_t) == 0);

    t->stack -= size;
    return t->stack;
}

/*! Chooses and returns the next thread to be scheduled.  Should return a
    thread from the run queue, unless the run queue is empty.  (If the running
    thread can continue running, then it will be in the run queue.)  If the
    run queue is empty, return idle_thread. */
static struct thread *next_thread_to_run(void) {
    struct thread *next_thread = NULL;

    next_thread = get_highest_ready_thread();
    if (next_thread == NULL)
        return idle_thread;
    list_remove(&next_thread->elem);
    return next_thread;
}

/*! Completes a thread switch by activating the new thread's page tables, and,
    if the previous thread is dying, destroying it.

    At this function's invocation, we just switched from thread PREV, the new
    thread is already running, and interrupts are still disabled.  This
    function is normally invoked by thread_schedule() as its final action
    before returning, but the first time a thread is scheduled it is called by
    switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is complete.  In
   practice that means that printf()s should be added at the end of the
   function.

   After this function and its caller returns, the thread switch is complete. */
void thread_schedule_tail(struct thread *prev) {
    struct thread *cur = running_thread();

    ASSERT(intr_get_level() == INTR_OFF);

    /* Mark us as running. */
    cur->status = THREAD_RUNNING;

    /* Start new time slice. */
    thread_ticks = 0;

#ifdef USERPROG
    /* Activate the new address space. */
    process_activate();
#endif

    /* If the thread we switched from is dying, destroy its struct thread.
       This must happen late so that thread_exit() doesn't pull out the rug
       under itself.  (We don't free initial_thread because its memory was
       not obtained via palloc().) */
    if (prev != NULL && prev->status == THREAD_DYING &&
        prev != initial_thread) {
        ASSERT(prev != cur);
        palloc_free_page(prev);
    }
}

/*! Schedules a new process.  At entry, interrupts must be off and the running
    process's state must have been changed from running to some other state.
    This function finds another thread to run and switches to it.

    It's not safe to call printf() until thread_schedule_tail() has
    completed. */
static void schedule(void) {
    struct thread *cur = running_thread();
    struct thread *next = next_thread_to_run();
    struct thread *prev = NULL;
    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(cur->status != THREAD_RUNNING);
    ASSERT(is_thread(next));

    if (cur != next)
        prev = switch_threads(cur, next);
    thread_schedule_tail(prev);

}

/*! Returns a tid to use for a new thread. */
static tid_t allocate_tid(void) {
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire(&tid_lock);
    tid = next_tid++;
    lock_release(&tid_lock);

    return tid;
}

/* Returns the thread in the ready queue with the highest priority.
 * Does NOT pop the thread from the ready queue. Returns NULL if there
 * are no ready threads. If there are multiple threads with the
 * highest priority, this returns the first one it sees.
 */
static struct thread *get_highest_ready_thread(void) {
    int i;
    struct list_elem *e;

    for (i = PRI_MAX; i >= PRI_MIN; i--) {
        if (list_empty(&ready_lists[i]))
            continue;
        e = list_front(&ready_lists[i]);
        return list_entry(e, struct thread, elem);
    }
    return NULL;
}

/* Checks that the current running thread has the highest priority.
 * This function should be called when a thread is added to the ready list
 * and also when the current thread changes its priority.
 *
 * Expects a pointer to the thread just added to the ready list as the
 * argument (for O(1) behavior). If the argument is NULL (for the case
 * where the current thread changed priority), then this function will
 * scan all elements in the ready list and check priorities.
 *
 * If a thread with higher priority is found, then we immediately yield
 * to the new thread.
 */
void yield_if_higher_priority(struct thread *other) {
    struct thread *curr;

    curr = thread_current();
    if (!other) {
        other = get_highest_ready_thread();
        if (other == NULL) {
            return;
        }
    }
    ASSERT (other);
    if (other->priority > curr->priority) {
        if (intr_context()) {
            intr_yield_on_return();
        } else {
            thread_yield();
        }
    }
}

/*! Offset of `stack' member within `struct thread'.
    Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof(struct thread, stack);

