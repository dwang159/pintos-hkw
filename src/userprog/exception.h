#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

/*! Page fault error code bits that describe the cause of the exception. @{ */
#define PF_P 0x1    /*!< 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /*!< 0: read, 1: write. */
#define PF_U 0x4    /*!< 0: kernel, 1: user process. */
/*! @} */

#define MAX_STACK 0x800000
#define STACK_HEURISTIC 32

#define possibly_stack(esp, ptr) (ptr >= esp - STACK_HEURISTIC &&\
                                  ptr >= PHYS_BASE - MAX_STACK &&\
                                  ptr < PHYS_BASE) 
void exception_init(void);
void exception_print_stats(void);

#endif /* userprog/exception.h */

