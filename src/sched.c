/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2020 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#include "sched.h"
#include "config.h"
#include "perf.h"
#include "main.h"
#include "task.h"
#include "event.h"
#include "game/public/game.h"
#include "phys/public/phys.h"
#include "script/public/script.h"
#include "lib/public/pqueue.h"
#include "lib/public/queue.h"
#include "lib/public/khash.h"
#include "lib/public/pf_string.h"
#include "lib/public/mem.h"

#include <SDL.h>
#include <inttypes.h>


enum taskstate{
    TASK_STATE_ACTIVE,
    TASK_STATE_READY,
    TASK_STATE_SEND_BLOCKED,
    TASK_STATE_RECV_BLOCKED,
    TASK_STATE_REPLY_BLOCKED,
    TASK_STATE_EVENT_BLOCKED,
    TASK_STATE_ZOMBIE,
};

#if defined(__x86_64__)

#if defined(_WIN32)

struct context{
    uint64_t rbx;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint32_t mxcsr;
    uint16_t fpucw;
    uint16_t __pad0;
    __int128 xmm6;
    __int128 xmm7;
    __int128 xmm8;
    __int128 xmm9;
    __int128 xmm10;
    __int128 xmm11;
    __int128 xmm12;
    __int128 xmm13;
    __int128 xmm14;
    __int128 xmm15;
} __attribute__((packed, aligned(16)));

#else // System V ABI

struct context{
    uint64_t rbx;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint32_t mxcsr;
    uint16_t fpucw;
    uint16_t __pad0;
} __attribute__((packed, aligned(8)));

#endif

#else
#error "Unsupported architecture"
#endif

enum{
    _SCHED_REQ_FREE = _SCHED_REQ_COUNT + 1,
    _SCHED_REQ_RUN_SYNC,
};

struct task{
    enum taskstate state;
    struct context ctx;
    int            prio;
    uint32_t       tid;
    uint32_t       parent_tid;
    uint32_t       flags;
    struct request req;
    uint64_t       retval;
    void          *stackmem;
    struct future *future;
    void          *arg;
    void         (*destructor)(void*);
    void          *darg;
    struct task   *prev, *next;
    void          *earg;
    void         (*erelease)(void*);
};

#define MAX_TASKS               (8192)
#define MAX_WORKER_THREADS      (64)
#define STACK_SZ                (16 * 1024)
#define BIG_STACK_SZ            (4 * 1024 * 1024)
#define SCHED_TICK_MS           (1.0f / CONFIG_SCHED_TARGET_FPS * 1000.0f)
#define ALIGNED(val, align)     (((val) + ((align) - 1)) & ~((align) - 1))

PQUEUE_TYPE(task, struct task*)
PQUEUE_IMPL(static, task, struct task*)

QUEUE_TYPE(tid, uint32_t)
QUEUE_IMPL(static, tid, uint32_t)

KHASH_MAP_INIT_INT64(tid, uint32_t)
KHASH_MAP_INIT_INT(tqueue, queue_tid_t)


uint64_t    sched_switch_ctx(struct context *save, struct context *restore, uint64_t retval, void *arg);
void        sched_task_exit_trampoline(void);
static void sched_task_exit(struct result ret);

/*****************************************************************************/
/* STATIC VARIABLES                                                          */
/*****************************************************************************/

/* After initialization, no new keys are added to the maps, 
 * so they are safe to read from different threads. 
 */
static khash_t(tid)    *s_thread_tid_map;
static khash_t(tid)    *s_thread_worker_id_map;

static struct context   s_main_ctx;
static struct task     *s_freehead;
static unsigned         s_nfree = MAX_TASKS;

static struct task      s_tasks[MAX_TASKS];
static char             s_stacks[MAX_TASKS][STACK_SZ];
static queue_tid_t      s_msg_queues[MAX_TASKS];
static bool             s_parent_waiting[MAX_TASKS];
static khash_t(tqueue) *s_event_queues;

/* Lock used to serialzie the scheduler requests */
static SDL_mutex       *s_request_lock;

/* The active worker threads wait on the ready cond to be 
 * notified when the ready queue(s) becomes non-empty, so that
 * they can pop a task to execute. At the end of a frame, the 
 * ready condition variable is also used to notify the workers 
 * that the 'quiesce' flags has been set, instructing them to 
 * go back to waiting on a start/quit command. 
 * 
 * We have 2 diff. queues to allow only de-queuing tasks based
 * on the thread. The worker threads will not dequeue from the 
 * 'main' queues.
 *
 * All the queues are protected by the ready lock.
 */
static pq_task_t        s_ready_queue;
static pq_task_t        s_ready_queue_main;

static SDL_mutex       *s_ready_lock;
static SDL_cond        *s_ready_cond;
static int              s_nwaiters;     /* protected by ready lock */
static bool             s_quiesce;      /* protected by ready lock */
static int              s_idle_workers; /* protected by ready lock */

static size_t           s_nworkers;
static SDL_Thread      *s_worker_threads[MAX_WORKER_THREADS];
//static threadID_t       s_worker_thread_id[MAX_WORKER_THREADS] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63}; // Makes fake ID's since SDL_GetThreadID() gives 0 or something on macOS  // NVM_TODO: ensure these ID's don't clash with real thread ID's that actually work!....
static struct context   s_worker_contexts[MAX_WORKER_THREADS];

/* At the start of each frame, the workers wait on either the
 * 'start' or 'quit' flag to be set by the main thread. 
 */
static bool             s_worker_start[MAX_WORKER_THREADS];
static bool             s_worker_quit[MAX_WORKER_THREADS];
static SDL_mutex       *s_worker_locks[MAX_WORKER_THREADS];
static SDL_cond        *s_worker_conds[MAX_WORKER_THREADS];

static enum simstate    s_prev_ss;

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

#if defined(__x86_64__)

#if defined(_WIN32)

__asm__(
".text                                          \n"
"                                               \n"
"# parameter 0 (%rcx) - save ctx ptr            \n"
"# parameter 1 (%rdx) - load ctx ptr            \n"
"# parameter 2 (%r8)  - return value            \n"
"# parameter 3 (%r9)  - arg passed to           \n"
"#                      context code            \n"
"                                               \n"
"sched_switch_ctx:                              \n"
"Lsave_ctx:                                     \n"
"   lea Lback(%rip), %rax                       \n" 
"   push %rax                                   \n" 
"   mov %rbx,  0x0(%rcx)                        \n"
"   mov %rsp,  0x8(%rcx)                        \n"
"   mov %rbp, 0x10(%rcx)                        \n"
"   mov %rdi, 0x18(%rcx)                        \n"
"   mov %rsi, 0x20(%rcx)                        \n"
"   mov %r12, 0x28(%rcx)                        \n"
"   mov %r13, 0x30(%rcx)                        \n"
"   mov %r14, 0x38(%rcx)                        \n"
"   mov %r15, 0x40(%rcx)                        \n"
"   stmxcsr 0x48(%rcx)                          \n"
"   fstcw   0x4c(%rcx)                          \n"
"   movdqa %xmm6, 0x50(%rcx)                    \n"
"   movdqa %xmm7, 0x60(%rcx)                    \n"
"   movdqa %xmm8, 0x70(%rcx)                    \n"
"   movdqa %xmm9, 0x80(%rcx)                    \n"
"   movdqa %xmm10, 0x90(%rcx)                   \n"
"   movdqa %xmm11, 0xa0(%rcx)                   \n"
"   movdqa %xmm12, 0xb0(%rcx)                   \n"
"   movdqa %xmm13, 0xc0(%rcx)                   \n"
"   movdqa %xmm14, 0xd0(%rcx)                   \n"
"   movdqa %xmm15, 0xe0(%rcx)                   \n"
"Lload_ctx:                                     \n"
"   mov  0x0(%rdx), %rbx                        \n"
"   mov  0x8(%rdx), %rsp                        \n"
"   mov 0x10(%rdx), %rbp                        \n"
"   mov 0x18(%rdx), %rdi                        \n"
"   mov 0x20(%rdx), %rsi                        \n"
"   mov 0x28(%rdx), %r12                        \n"
"   mov 0x30(%rdx), %r13                        \n"
"   mov 0x38(%rdx), %r14                        \n"
"   mov 0x40(%rdx), %r15                        \n"
"   ldmxcsr 0x48(%rdx)                          \n"
"   fldcw   0x4c(%rdx)                          \n"
"   movdqa 0x50(%rdx), %xmm6                    \n"
"   movdqa 0x60(%rdx), %xmm7                    \n"
"   movdqa 0x70(%rdx), %xmm8                    \n"
"   movdqa 0x80(%rdx), %xmm9                    \n"
"   movdqa 0x90(%rdx), %xmm10                   \n"
"   movdqa 0xa0(%rdx), %xmm11                   \n"
"   movdqa 0xb0(%rdx), %xmm12                   \n"
"   movdqa 0xc0(%rdx), %xmm13                   \n"
"   movdqa 0xd0(%rdx), %xmm14                   \n"
"   movdqa 0xe0(%rdx), %xmm15                   \n"
"   mov %rdx, %rcx # write result to ctx mem    \n"
"   mov %r9, %rdx                               \n"
"   mov %r8, %rax                               \n"
"Lback:                                         \n"
"   ret                                         \n"
"                                               \n"
);

__asm__(
".text                                          \n"
"                                               \n"
"# (%rax) - pointer to 128 bits of task result  \n"
"                                               \n"
"_sched_task_exit_trampoline:                    \n"
"   mov %rax, %rcx                              \n"
"   jmp _sched_task_exit                         \n"
);

#else

__asm__(
".text                                          \n"
"                                               \n"
#ifndef __APPLE__ // https://stackoverflow.com/questions/19720084/what-is-the-difference-between-assembly-on-mac-and-assembly-on-linux/19725269#19725269 <- https://stackoverflow.com/questions/20907946/error-unknown-directive-type-func-function
".type sched_switch_ctx, @function              \n"
#endif
"                                               \n"
"# parameter 0 (%rdi) - save ctx ptr            \n"
"# parameter 1 (%rsi) - load ctx ptr            \n"
"# parameter 2 (%rdx) - return value            \n"
"# parameter 3 (%rcx) - arg passed to           \n"
"#                      context code            \n"
"                                               \n"
"_sched_switch_ctx:                              \n"
"Lsave_ctx:                                     \n"
"   lea Lback(%rip), %r8                        \n" 
"   push %r8                                    \n" 
"   mov %rbx,  0x0(%rdi)                        \n"
"   mov %rsp,  0x8(%rdi)                        \n"
"   mov %rbp, 0x10(%rdi)                        \n"
"   mov %r12, 0x18(%rdi)                        \n"
"   mov %r13, 0x20(%rdi)                        \n"
"   mov %r14, 0x28(%rdi)                        \n"
"   mov %r15, 0x30(%rdi)                        \n"
"   stmxcsr 0x38(%rdi)                          \n"
"   fstcw   0x3c(%rdi)                          \n"
"Lload_ctx:                                     \n"
"   mov  0x0(%rsi), %rbx                        \n"
"   mov  0x8(%rsi), %rsp                        \n"
"   mov 0x10(%rsi), %rbp                        \n"
"   mov 0x18(%rsi), %r12                        \n"
"   mov 0x20(%rsi), %r13                        \n"
"   mov 0x28(%rsi), %r14                        \n"
"   mov 0x30(%rsi), %r15                        \n"
"   ldmxcsr 0x38(%rsi)                          \n"
"   fldcw   0x3c(%rsi)                          \n"
"   mov %rcx, %rdi                              \n"
"   mov %rdx, %rax                              \n"
"Lback:                                         \n"
"   ret                                         \n"
"                                               \n"
);

__asm__(
".text                                          \n"
"                                               \n"
#ifndef __APPLE__
".type sched_task_exit_trampoline, @function    \n"
#endif
"                                               \n"
"# (%rax) - low 64 bits of task result          \n"
"# (%rdx) - high 64 bits of task result         \n"
"                                               \n"
"_sched_task_exit_trampoline:                    \n"
"   movq %rax, %rdi                             \n"
"   movq %rdx, %rsi                             \n"
"   jmp _sched_task_exit                         \n"
);

#endif

static void sched_init_ctx(struct task *task, void *code)
{
    const size_t stack_size = (task->flags & TASK_BIG_STACK) ? BIG_STACK_SZ : STACK_SZ;
    char *stack_end = task->stackmem;
    char *stack_base = stack_end + stack_size;
    stack_base = (char*)ALIGNED((uintptr_t)stack_base, 32);
    if(stack_base >= stack_end + stack_size)
        stack_base -= 32;

    /* This is the address where we will jump to upon 
     * returning from the task function. This routine
     * is responsible for context-switching out of the 
     * task context. */
    stack_base -= 8;
    *(uint64_t*)stack_base = (uintptr_t)sched_task_exit_trampoline;

    /* This is where we will jump to the next time we 
     * will context switch to this task. */
    stack_base -= 8;
    *(uint64_t*)stack_base = (uintptr_t)code;

    memset(&task->ctx, 0, sizeof(task->ctx));
    task->ctx.rsp = (uintptr_t)stack_base;

    /* +-------------------------------------------------------------------+
     * | Register[bits] | Setting                                          |
     * + ---------------+--------------------------------------------------+
     * | FPCSR[0:6]     | Exception masks all 1's (all exceptions masked)  |
     * | FPCSR[7]       | Reserved - 0                                     |
     * | FPCSR[8:9]     | Precision Control - 10B (double precision)       |
     * | FPCSR[10:11]   | Rounding control - 0 (round to nearest)          |
     * | FPCSR[12]      | Infinity control - 0 (not used)                  |
     * +----------------+--------------------------------------------------+
     */
    task->ctx.fpucw = (((uint16_t)(0x0  & 0x1 )) << 12)
                    | (((uint16_t)(0x0  & 0x3 )) << 10)
                    | (((uint16_t)(0x2  & 0x3 )) << 8 )
                    | (((uint16_t)(0x0  & 0x1 )) << 7 )
                    | (((uint16_t)(0x3f & 0x3f)) << 0 );

    /* The volatile portion consists of the six status flags, in MXCSR[0:5].
     * They are caller-saved, so their initial state doesn't matter.
     *
     * +-------------------------------------------------------------------+
     * | Register[bits] | Setting                                          |
     * + ---------------+--------------------------------------------------+
     * | MXCSR[6]       | Denormals are zeros - 0                          |
     * | MXCSR[7:12]    | Exception masks all 1's (all exceptions masked)  |
     * | MXCSR[13:14]   | Rounding control - 0 (round to nearest)          |
     * | MXCSR[15]      | Flush to zero for masked underflow - 0 (off)     |
     * +----------------+--------------------------------------------------+
     */
    task->ctx.mxcsr = (((uint32_t)(0x0  & 0x1 )) << 15)
                    | (((uint32_t)(0x0  & 0x3 )) << 13)
                    | (((uint32_t)(0x3f & 0x3f)) << 7 )
                    | (((uint32_t)(0x0  & 0x1 )) << 6 );
}

__attribute__((always_inline)) static inline uint64_t sched_get_sp(void)
{
    uint64_t sp;
    __asm__("mov %%rsp, %0" : "=rm" (sp));
    return sp;
}

static inline uint64_t sched_task_get_sp(const struct task *task)
{
    return task->ctx.rsp;
}

/* The return address queries are only valid when the 
 * code is compiled with -fno-omit-framepointer.
 */
__attribute__((always_inline)) static inline uint64_t sched_get_retaddr(void)
{
    uint64_t rbp;
    __asm__("mov %%rbp, %0" : "=rm" (rbp));
    return *(((uint64_t*)(rbp)) + 1);
}

static inline uint64_t sched_task_get_retaddr(const struct task *task)
{
    return *(((uint64_t*)(task->ctx.rbp)) + 1);
}

__attribute__((always_inline)) static inline void sched_print_backtrace(void)
{
    uint64_t rip, rbp, retaddr;

    __asm__("lea 0x0(%%rip), %0\n" : "=rm" (rip));
    __asm__("mov %%rbp, %0" : "=rm" (rbp));

    printf("#0  0x%016" PRIx64 "\n", rip);

    int i = 1;
    retaddr = *(((uint64_t*)rbp) + 1);

    while((uintptr_t)retaddr != (uintptr_t)sched_task_exit_trampoline) {
        printf("#%-2d 0x%016" PRIx64 "\n", i++, retaddr);
        rbp = *((uint64_t*)rbp);
        retaddr = *(((uint64_t*)rbp) + 1);
    }

    fflush(stdout);
}

#endif

static bool stack_pointer_valid(const struct task *task)
{
    uintptr_t sp = task->ctx.rsp;
    if(((uint64_t)sp) & 0x7)
        return false; /* violated alignment */

    size_t size = (task->flags & TASK_BIG_STACK) ? BIG_STACK_SZ : STACK_SZ;
    if(sp < (uintptr_t)task->stackmem)
        return false; /* overflow */

    if(sp >= ((uintptr_t)task->stackmem) + size)
        return false; /* underflow */

    return true;
}

#ifdef __APPLE__
static uint64_t thread_id_to_key(threadID_t tid)
{
    return (uint64_t)tid;
}
#else
static uint64_t thread_id_to_key(SDL_threadID tid)
{
    union{
        SDL_threadID as_tid;
        uint64_t     as_u64;
    }ret = {0};
    ret.as_tid = tid;
    return ret.as_u64;
}
#endif

static void sched_set_thread_tid(threadID_t id, uint32_t tid)
{
    uint64_t key = thread_id_to_key(id);
    khiter_t k = kh_get(tid, s_thread_tid_map, key);
    assert(k != kh_end(s_thread_tid_map));
    kh_val(s_thread_tid_map, k) = tid;
}

static uint32_t sched_curr_thread_tid(void)
{
    printf("thisThreadID(): %ju\n", thisThreadID()); fflush(stdout);
    uint64_t key = thread_id_to_key(thisThreadID());
    khiter_t k = kh_get(tid, s_thread_tid_map, key);
    assert(k != kh_end(s_thread_tid_map));
    return kh_val(s_thread_tid_map, k);
}

static uint32_t sched_curr_thread_worker_id(void)
{
    uint64_t key = thread_id_to_key(thisThreadID());
    khiter_t k = kh_get(tid, s_thread_worker_id_map, key);
    assert(k != kh_end(s_thread_worker_id_map));
    return kh_val(s_thread_worker_id_map, k);
}

static struct task *sched_task_alloc(void)
{
    if(!s_freehead)
        return NULL;

    struct task *ret = s_freehead;
    if(ret->prev)
        ret->prev->next = ret->next;
    if(ret->next)
        ret->next->prev = ret->prev;

    s_freehead = s_freehead->next;
    s_nfree--;
    return ret;
}

static void sched_task_free(struct task *task)
{
    task->next = s_freehead;
    task->prev = NULL;
    if(s_freehead)
        s_freehead->prev = task;
    s_freehead = task;
    s_nfree++;
}

static void sched_reactivate(struct task *task)
{
    SDL_LockMutex(s_ready_lock); 
    task->state = TASK_STATE_READY;

    if(task->flags & TASK_MAIN_THREAD_PINNED) {
        pq_task_push(&s_ready_queue_main, task->prio, task);
    }else{
        pq_task_push(&s_ready_queue, task->prio, task);
    }

    SDL_CondSignal(s_ready_cond);
    SDL_UnlockMutex(s_ready_lock);
}

__attribute__((used)) static void sched_task_exit(struct result ret)
{
    uint32_t tid = sched_curr_thread_tid();
    struct task *task = &s_tasks[tid - 1];

    if(task->future) {
        task->future->res = ret;
        SDL_AtomicSet(&task->future->status, FUTURE_COMPLETE);
    }

    if(task->destructor) {
        task->destructor(task->darg);
    }

    if(task->erelease) {
        task->erelease(task->earg);
        task->erelease = NULL;
        task->earg = NULL;
    }

    task->req.type = _SCHED_REQ_FREE;

    if(thisThreadID() == g_main_thread_id) {
        sched_switch_ctx(&task->ctx, &s_main_ctx, 0, NULL);
    }else{
        int id = sched_curr_thread_worker_id();
        sched_switch_ctx(&task->ctx, &s_worker_contexts[id], 0, NULL);
    }

    assert(0);
}

static void sched_task_init(struct task *task, int prio, uint32_t flags, 
                            void *code, void *arg, struct future *future, uint32_t parent)
{
    task->prio = prio;
    task->parent_tid = parent;
    task->flags = flags;
    task->retval = 0;
    task->arg = arg;
    task->destructor = NULL;
    task->darg = NULL;
    task->future = future;
    task->earg = NULL;
    task->erelease = NULL;

    if(task->future) {
        SDL_AtomicSet(&task->future->status, FUTURE_INCOMPLETE);    
    }

    if(flags & TASK_BIG_STACK) {
        task->stackmem = malloc(BIG_STACK_SZ);
    }else{
        task->stackmem = s_stacks[task->tid - 1];
    }

    sched_init_ctx(task, code);
    sched_reactivate(task);
}

static void sched_send(struct task *task, uint32_t tid, void *msg, size_t msglen)
{
    struct task *recv_task = &s_tasks[tid - 1];

    /* write data to blocked send-blocked task to unblock it */
    if(recv_task->state == TASK_STATE_SEND_BLOCKED) {

        uint32_t *out_send = (uint32_t*)recv_task->req.argv[0];
        void *dst = (void*)recv_task->req.argv[1];
        size_t dstlen = (size_t)recv_task->req.argv[2];

        assert(dstlen == msglen);
        memcpy(dst, msg, msglen);
        *out_send = task->tid;

        task->state = TASK_STATE_REPLY_BLOCKED;
        assert(recv_task->state != TASK_STATE_EVENT_BLOCKED);
        sched_reactivate(recv_task);

    }else{

        task->state = TASK_STATE_RECV_BLOCKED;
        queue_tid_push(&s_msg_queues[tid - 1], &task->tid);
    }
}

static void sched_receive(struct task *task, uint32_t *out_tid, void *msg, size_t msglen)
{
    if(queue_size(s_msg_queues[task->tid - 1]) > 0) {
    
        uint32_t send_tid = 0;
        assert(task->state != TASK_STATE_SEND_BLOCKED);
        queue_tid_pop(&s_msg_queues[task->tid - 1], &send_tid);
        assert(send_tid > 0);

        struct task *send_task = &s_tasks[send_tid - 1];
        void *src = (void*)send_task->req.argv[1];
        size_t srclen = (size_t)send_task->req.argv[2];

        assert(srclen == msglen);
        memcpy(msg, src, msglen);
        *out_tid = send_tid;

        assert(send_task->state == TASK_STATE_RECV_BLOCKED);
        send_task->state = TASK_STATE_REPLY_BLOCKED;
        assert(task->state != TASK_STATE_EVENT_BLOCKED);
        sched_reactivate(task);

    }else{

        task->state = TASK_STATE_SEND_BLOCKED;
    }
}

static void sched_reply(struct task *task, uint32_t tid, void *reply, size_t replylen)
{
    struct task *send_task = &s_tasks[tid - 1];
    assert(send_task->state == TASK_STATE_REPLY_BLOCKED);

    void *dst = (void*)send_task->req.argv[3];
    size_t dstlen = (size_t)send_task->req.argv[4];

    assert(dstlen == replylen);
    memcpy(dst, reply, replylen);

    assert(send_task->state != TASK_STATE_EVENT_BLOCKED);
    sched_reactivate(send_task);
    assert(task->state != TASK_STATE_EVENT_BLOCKED);
    sched_reactivate(task);
}

static uint64_t sched_retain_arg(struct task *task, int event, int event_source, void *arg)
{
    void *ret = NULL;
    if(event_source == ES_SCRIPT) {
        /* Scripting references will be released by the event consumer */
        S_Retain(arg);
        ret = arg;
    }else{
        if(event < SDL_LASTEVENT) {
            ret = malloc(sizeof(SDL_Event));
            if(arg) {
                memcpy(ret, arg, sizeof(SDL_Event));
            }
            task->erelease = free;
        }else if(event == EVENT_ENTERED_REGION || event == EVENT_EXITED_REGION) {
            ret = pf_strdup(arg);
            task->erelease = free;
        }else if(event == EVENT_PROJECTILE_HIT) {
            ret = malloc(sizeof(struct proj_hit));
            if(arg) {
                memcpy(ret, arg, sizeof(struct proj_hit));
            }
            task->erelease = free;
        }else{
            /* We'd better be sure that the pointer won't have been 'free'd or something 
             * by the time the waiting task finally runs... */
            ret = arg;
            task->erelease = NULL;
        }
    }
    task->earg = (void*)(uintptr_t)ret;
    return (uint64_t)ret;
}

static void sched_task_cleanup(const struct task *task)
{
    if(task->destructor) {
        task->destructor(task->darg);
    }
    if(task->erelease) {
        task->erelease(task->earg);
    }
}

static void sched_await_event(struct task *task, int event)
{
    task->state = TASK_STATE_EVENT_BLOCKED;

    int status;
    khiter_t k = kh_get(tqueue, s_event_queues, event);

    if(k == kh_end(s_event_queues)) {

        k = kh_put(tqueue, s_event_queues, event, &status);
        assert(status != -1 && status != 0);
        queue_tid_init(&kh_val(s_event_queues, k), 32);
    }
    queue_tid_push(&kh_val(s_event_queues, k), &task->tid);
}

static uint32_t sched_create(int prio, task_func_t code, void *arg, struct future *result, 
                             int flags, uint32_t parent)
{
    struct task *task = sched_task_alloc();
    if(!task)
        return NULL_TID;

    sched_task_init(task, prio, flags, code, arg, result, parent);
    return task->tid;
}

static bool sched_wait(struct task *task, uint32_t child_tid)
{
    if(child_tid > MAX_TASKS)
        return false;

    struct task *child = &s_tasks[child_tid - 1];
    if(child->parent_tid != task->tid
    || (child->flags & TASK_DETACHED))
        return false;

    if(child->state == TASK_STATE_ZOMBIE) {
        sched_task_free(child);
        assert(task->state != TASK_STATE_EVENT_BLOCKED);
        sched_reactivate(task);
        return true;
    }

    s_parent_waiting[child_tid - 1] = true;
    return true;
}

static void sched_task_run(struct task *task)
{
    assert(sched_curr_thread_tid() == NULL_TID);
    sched_set_thread_tid(thisThreadID(), task->tid);
    task->state = TASK_STATE_ACTIVE;
    assert(stack_pointer_valid(task));

    char name[64];
    pf_snprintf(name, sizeof(name), "Task %03u", task->tid);
    PERF_PUSH(name);

    if(thisThreadID() == g_main_thread_id) {
        sched_switch_ctx(&s_main_ctx, &task->ctx, task->retval, task->arg);
    }else{
        int id = sched_curr_thread_worker_id();
        sched_switch_ctx(&s_worker_contexts[id], &task->ctx, task->retval, task->arg);
    }

    PERF_POP();
    assert(stack_pointer_valid(task));
    sched_set_thread_tid(thisThreadID(), NULL_TID);
}

static void sched_task_service_request(struct task *task)
{
    assert(sched_curr_thread_tid() == NULL_TID);
    SDL_LockMutex(s_request_lock);

    switch((int)task->req.type) {
    case SCHED_REQ_CREATE:
        task->retval = sched_create(
            (int)           task->req.argv[0],
            (task_func_t)   task->req.argv[1],
            (void*)         task->req.argv[2],
            (struct future*)task->req.argv[3],
            (int)           task->req.argv[4],
            task->tid
        );
        sched_reactivate(task);
        break;
    case SCHED_REQ_MY_TID:
        task->retval = task->tid;
        sched_reactivate(task);
        break;
    case SCHED_REQ_MY_PARENT_TID:
        task->retval = task->parent_tid;
        sched_reactivate(task);
        break;
    case SCHED_REQ_YIELD:
        sched_reactivate(task);
        break;
    case SCHED_REQ_SEND:
        sched_send(
            task, 
            (uint32_t)  task->req.argv[0], 
            (void*)     task->req.argv[1], 
            (size_t)    task->req.argv[2]
        );
        break;
    case SCHED_REQ_RECEIVE:
        sched_receive(
            task, 
            (uint32_t*) task->req.argv[0], 
            (void*)     task->req.argv[1], 
            (size_t)    task->req.argv[2]
        );
        break;
    case SCHED_REQ_REPLY:
        sched_reply(
            task, 
            (uint32_t)  task->req.argv[0], 
            (void*)     task->req.argv[1], 
            (size_t)    task->req.argv[2]
        );
        break;
    case SCHED_REQ_AWAIT_EVENT:
        sched_await_event(
            task, 
            (int)       task->req.argv[0]
        );
        break;
    case SCHED_REQ_SET_DESTRUCTOR:

        task->destructor = (void (*)(void*))task->req.argv[0];
        task->darg = (void*)task->req.argv[1];
        sched_reactivate(task);
        break;
    case SCHED_REQ_WAIT:
        task->retval = sched_wait(task, task->req.argv[0]);
        break;
    case _SCHED_REQ_FREE:

        if(task->flags & TASK_BIG_STACK) {
            PF_FREE(task->stackmem);
        }
        if(task->flags & TASK_DETACHED) {
            sched_task_free(task);
        }else if(s_parent_waiting[task->tid - 1]) {

            struct task *parent = &s_tasks[task->parent_tid - 1];
            s_parent_waiting[task->tid - 1] = false;
            assert(parent->state != TASK_STATE_EVENT_BLOCKED);
            sched_reactivate(parent);
            sched_task_free(task);
        }else{
            task->state = TASK_STATE_ZOMBIE;
        }
        break;
    case _SCHED_REQ_RUN_SYNC: {

        struct task *requested = &s_tasks[((uint32_t)task->req.argv[0]) - 1];
        sched_task_run(requested);
        sched_task_service_request(requested);
        sched_reactivate(task);
        break;
    }
    default: assert(0);    
    }

    if(task->erelease) {
        task->erelease(task->earg);
        task->erelease = NULL;
        task->earg = NULL;
    }

    SDL_UnlockMutex(s_request_lock);
}

static void sched_signal_worker_quit(int id)
{
    SDL_LockMutex(s_worker_locks[id]);
    s_worker_quit[id] = true;
    SDL_CondSignal(s_worker_conds[id]);
    SDL_UnlockMutex(s_worker_locks[id]);
}

static void sched_init_thread_tid_map(void)
{
    khiter_t k;
    int status;
    uint64_t main_key = thread_id_to_key(g_main_thread_id);

    printf("Put thread ID: %ju\n", g_main_thread_id);
    k = kh_put(tid, s_thread_tid_map, g_main_thread_id, &status);
    assert(status != -1 && status != 0);
    kh_value(s_thread_tid_map, k) = NULL_TID;

    for(int i = 0; i < s_nworkers; i++) {

        //uint64_t key = thread_id_to_key(SDL_GetThreadID(s_worker_threads[i]));
        //uint64_t key = thread_id_to_key(s_worker_thread_id[&s_worker_threads[i] - s_worker_threads]);
        uint64_t key = thread_id_to_key(getThreadID(s_worker_threads[i]));
        printf("Put thread ID: %ju\n", key);
        k = kh_put(tid, s_thread_tid_map, key, &status);
        assert(status != -1 && status != 0);
        kh_value(s_thread_tid_map, k) = NULL_TID;
    }
}

static void sched_init_thread_worker_id_map(void)
{
    for(int i = 0; i < s_nworkers; i++) {

        int status;
        //uint64_t key = thread_id_to_key(SDL_GetThreadID(s_worker_threads[i]));
        //uint64_t key = thread_id_to_key(s_worker_thread_id[&s_worker_threads[i] - s_worker_threads]);
        uint64_t key = thread_id_to_key(getThreadID(s_worker_threads[i]));
        
        printf("Worker threads: put thread ID: %ju\n", key);
        khiter_t k = kh_put(tid, s_thread_worker_id_map, key, &status);
        assert(status != -1 && status != 0);
        kh_val(s_thread_worker_id_map, k) = i;
    }
}

static void sched_wait_workers_done(void)
{
    SDL_LockMutex(s_ready_lock);
    while(s_idle_workers < s_nworkers)
        SDL_CondWait(s_ready_cond, s_ready_lock);
    SDL_UnlockMutex(s_ready_lock);

    assert(s_idle_workers == s_nworkers);
    assert(s_nwaiters == 0);
}

static void sched_quiesce_workers(void)
{
    ASSERT_IN_MAIN_THREAD();
    PERF_ENTER();

    SDL_LockMutex(s_ready_lock);
    s_quiesce = true;
    SDL_CondBroadcast(s_ready_cond);
    SDL_UnlockMutex(s_ready_lock);

    sched_wait_workers_done();

    s_quiesce = false;
    PERF_RETURN_VOID();
}

static void worker_wait_on_cmd(int id)
{
    SDL_LockMutex(s_worker_locks[id]);
    while(!s_worker_start[id] && !s_worker_quit[id])
        SDL_CondWait(s_worker_conds[id], s_worker_locks[id]);
    s_worker_start[id] = false;
    SDL_UnlockMutex(s_worker_locks[id]);
}

static void worker_notify_done(int id)
{
    SDL_LockMutex(s_ready_lock);
    s_idle_workers++;
    SDL_CondSignal(s_ready_cond);
    SDL_UnlockMutex(s_ready_lock);
}

static struct task *worker_wait_task_or_quiesce(void)
{
    struct task *task = NULL;

    SDL_LockMutex(s_ready_lock);
    s_nwaiters++;

    if(s_nwaiters == s_nworkers) {
        SDL_CondBroadcast(s_ready_cond);
    }

    while(!s_quiesce && !pq_task_pop(&s_ready_queue, &task)) {
        SDL_CondWait(s_ready_cond, s_ready_lock);
    }

    s_nwaiters--;
    SDL_UnlockMutex(s_ready_lock);

    return task;
}

static void worker_do_work(int id)
{
    while(true) {

        struct task *task = worker_wait_task_or_quiesce();
        if(!task)
            return;

        assert(task);
        sched_task_run(task);
        sched_task_service_request(task);
    }
}

static int worker_threadfn(void *arg)
{
    int id = (uintptr_t)arg;

    while(true) {

        worker_wait_on_cmd(id);
        if(s_worker_quit[id])
            break;
        worker_do_work(id);
        worker_notify_done(id);
    }
    return 0;
}

static int tasks_compare(void *a, void *b)
{
    struct task *ta = *(struct task**)a;
    struct task *tb = *(struct task**)b;
    return ((uintptr_t)(ta) - (uintptr_t)(tb));
}

/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

bool Sched_Init(void)
{
    ASSERT_IN_MAIN_THREAD();

    s_thread_tid_map = kh_init(tid);
    if(!s_thread_tid_map)
        goto fail_thread_tid_map;

    s_thread_worker_id_map = kh_init(tid);
    if(!s_thread_worker_id_map)
        goto fail_thread_worker_id_map;

    s_request_lock = SDL_CreateMutex();
    if(!s_request_lock)
        goto fail_req_lock;

    s_event_queues = kh_init(tqueue);
    if(!s_event_queues)
        goto fail_event_queue;

    s_ready_lock = SDL_CreateMutex();
    if(!s_ready_lock)
        goto fail_ready_lock;

    s_ready_cond = SDL_CreateCond();
    if(!s_ready_cond)
        goto fail_ready_cond;

    pq_task_init(&s_ready_queue);
    if(!pq_task_reserve(&s_ready_queue, MAX_TASKS))
        goto fail_ready_queue;

    pq_task_init(&s_ready_queue_main);
    if(!pq_task_reserve(&s_ready_queue_main, MAX_TASKS))
        goto fail_ready_queue_main;

    assert(MAX_TASKS >= 2);
    s_tasks[0].prev = NULL;
    s_tasks[0].next = &s_tasks[1];
    s_tasks[MAX_TASKS-1].prev = &s_tasks[MAX_TASKS - 2];
    s_tasks[MAX_TASKS-1].next = NULL;

    for(int i = 1; i < MAX_TASKS-1; i++) {
        s_tasks[i].next = &s_tasks[i + 1];
        s_tasks[i].prev = &s_tasks[i - 1];
    }
    s_freehead = s_tasks;

    for(int i = 0; i < MAX_TASKS; i++) {

        s_tasks[i].tid = i + 1;
        s_tasks[i].state = TASK_STATE_ACTIVE;
        if(!queue_tid_init(&s_msg_queues[i], MAX_TASKS))
            goto fail_msg_queue;
    }

    /* On a single-core system, all the tasks will just be run on the main thread */
    s_nworkers = SDL_GetCPUCount() - 1;

    for(int i = 0; i < s_nworkers; i++) {

        s_worker_locks[i] = SDL_CreateMutex();
        if(!s_worker_locks[i])
            goto fail_worker_sync;

        s_worker_conds[i] = SDL_CreateCond();
        if(!s_worker_conds[i])
            goto fail_worker_sync;

        s_worker_start[i] = false;
        s_worker_quit[i] = false;
    }

    for(int i = 0; i < s_nworkers; i++) {

        char threadname[128];
        pf_snprintf(threadname, sizeof(threadname), "worker-%d", i);
        s_worker_threads[i] = SDL_CreateThread(worker_threadfn, threadname, (void*)((uintptr_t)i));
        if(!s_worker_threads[i])
            goto fail_workers;
        //Perf_RegisterThread(SDL_GetThreadID(s_worker_threads[i]), threadname);
        Perf_RegisterThread(getThreadID(s_worker_threads[i]), threadname);
    }

    sched_init_thread_tid_map();
    sched_init_thread_worker_id_map();
    Task_CreateServices();
    return true;

fail_workers:
    for(int i = 0; i < s_nworkers; i++) {
        if(!s_worker_threads[i])
            continue;
        sched_signal_worker_quit(i);
        SDL_WaitThread(s_worker_threads[i], NULL);
    }
fail_worker_sync:
    for(int i = 0; i < s_nworkers; i++) {
        if(s_worker_locks[i])
            SDL_DestroyMutex(s_worker_locks[i]);
        if(s_worker_conds[i])
            SDL_DestroyCond(s_worker_conds[i]);
    }
fail_msg_queue:
    for(int i = 0; i < MAX_TASKS; i++) {
        queue_tid_destroy(s_msg_queues + i);
    }
    pq_task_destroy(&s_ready_queue_main);
fail_ready_queue_main:
    pq_task_destroy(&s_ready_queue);
fail_ready_queue:
    SDL_DestroyCond(s_ready_cond);
fail_ready_cond:
    SDL_DestroyMutex(s_ready_lock);
fail_ready_lock:
    kh_destroy(tqueue, s_event_queues);
fail_event_queue:
    SDL_DestroyMutex(s_request_lock);
fail_req_lock:
    kh_destroy(tid, s_thread_worker_id_map);
fail_thread_worker_id_map:
    kh_destroy(tid, s_thread_tid_map);
fail_thread_tid_map:
    return false;
}

void Sched_Shutdown(void)
{
    ASSERT_IN_MAIN_THREAD();

    uint32_t key;
    queue_tid_t curr;
    (void)key;

    kh_foreach(s_event_queues, key, curr, {
        queue_tid_destroy(&curr);
    });
    kh_destroy(tqueue, s_event_queues);

    SDL_DestroyCond(s_ready_cond);
    SDL_DestroyMutex(s_ready_lock);
    kh_destroy(tid, s_thread_tid_map);
    kh_destroy(tid, s_thread_worker_id_map);
    SDL_DestroyMutex(s_request_lock);
    pq_task_destroy(&s_ready_queue);
    pq_task_destroy(&s_ready_queue_main);

    for(int i = 0; i < s_nworkers; i++) {
        sched_signal_worker_quit(i);
        SDL_WaitThread(s_worker_threads[i], NULL);
    }
    for(int i = 0; i < s_nworkers; i++) {
        SDL_DestroyMutex(s_worker_locks[i]);
        SDL_DestroyCond(s_worker_conds[i]);
    }
    for(int i = 0; i < MAX_TASKS; i++) {
        queue_tid_destroy(s_msg_queues + i);
    }
}

void Sched_HandleEvent(int event, void *arg, int event_source, bool immediate)
{
    ASSERT_IN_MAIN_THREAD();
    SDL_LockMutex(s_request_lock);

    queue_tid_t torun;
    queue_tid_init(&torun, 32);

    khiter_t k = kh_get(tqueue, s_event_queues, event);
    if(k == kh_end(s_event_queues))
        goto out;

    queue_tid_t *waiters = &kh_val(s_event_queues, k);
    while(queue_size(*waiters) > 0) {

        uint32_t tid;
        queue_tid_pop(waiters, &tid);

        struct task *task = &s_tasks[tid - 1];
        assert(task->state == TASK_STATE_EVENT_BLOCKED);

        int *source = (void*)task->req.argv[1];
        *source = event_source;

        /* We don't know when the task will be scheduled next, so we've 
         * got to be careful about making sure that the event arg pointer
         * doesn't become stale between now and when the task is actually
         * run. 
         */
        task->retval = sched_retain_arg(task, event, event_source, arg);

        if(immediate) {
            queue_tid_push(&torun, &tid);
        }else{
            sched_reactivate(task);
        }
    }

    while(queue_size(torun) > 0) {

        uint32_t tid;
        queue_tid_pop(&torun, &tid);

        struct task *task = &s_tasks[tid - 1];
        sched_task_run(task);
        sched_task_service_request(task);
    }

out:
    queue_tid_destroy(&torun);
    SDL_UnlockMutex(s_request_lock);
}    

void Sched_StartBackgroundTasks(void)
{
    ASSERT_IN_MAIN_THREAD();

    s_prev_ss = G_GetSimState();
    if(s_prev_ss != G_RUNNING)
        return;

    SDL_LockMutex(s_ready_lock);
    s_idle_workers = 0;
    SDL_UnlockMutex(s_ready_lock);

    for(int i = 0; i < s_nworkers; i++) {
    
        SDL_LockMutex(s_worker_locks[i]);
        s_worker_start[i] = true;
        SDL_CondSignal(s_worker_conds[i]);
        SDL_UnlockMutex(s_worker_locks[i]);
    }
}

void Sched_Tick(void)
{
    ASSERT_IN_MAIN_THREAD();
    PERF_ENTER();

    if(s_prev_ss != G_RUNNING)
        PERF_RETURN_VOID();

    /* Use a do-while to ensure we're always making at least _some_ forward progress */
    do{
        int nwaiters = 0;
        struct task *curr = NULL;

        SDL_LockMutex(s_ready_lock);
        while(!pq_size(&s_ready_queue_main) 
           && !pq_size(&s_ready_queue) 
           && ((nwaiters = s_nwaiters) < s_nworkers)
           && (s_idle_workers < s_nworkers)) {

            size_t left = (Perf_CurrFrameMS() < SCHED_TICK_MS) 
                        ? SCHED_TICK_MS - Perf_CurrFrameMS() 
                        : 0;

            SDL_CondWaitTimeout(s_ready_cond, s_ready_lock, left);
            if(left == 0) {
                s_quiesce = true;
                SDL_CondBroadcast(s_ready_cond); 
            }
        }
        if(pq_size(&s_ready_queue_main) || pq_size(&s_ready_queue)) {

            float prio_main = -1.0, prio_gen = -1.0;
            pq_task_top_prio(&s_ready_queue_main, &prio_main);
            pq_task_top_prio(&s_ready_queue, &prio_gen);

            if(prio_gen > prio_main) {
                pq_task_pop(&s_ready_queue, &curr);
            }else {
                pq_task_pop(&s_ready_queue_main, &curr);
            }
        }
        SDL_UnlockMutex(s_ready_lock);

        /* When the ready queue is empty and all the workers are in a state of waiting, 
         * there is no more work to be done. In that case, let's not waste any more time. 
         */
        if(curr == NULL && nwaiters == s_nworkers)
            break;
        if(curr == NULL && s_idle_workers == s_nworkers)
            break;

        assert(curr);
        sched_task_run(curr);
        sched_task_service_request(curr);

    }while(Perf_CurrFrameMS() < SCHED_TICK_MS);

    sched_quiesce_workers();
    PERF_RETURN_VOID();
}

uint32_t Sched_Create(int prio, task_func_t code, void *arg, struct future *result, int flags)
{
    ASSERT_IN_MAIN_THREAD();

    SDL_LockMutex(s_request_lock);
    uint32_t ret = sched_create(prio, code, arg, result, flags | TASK_DETACHED, NULL_TID);
    SDL_UnlockMutex(s_request_lock);

    return ret;
}

bool Sched_RunSync(uint32_t tid)
{
    SDL_LockMutex(s_request_lock);
    bool ret = false;

    struct task *task = &s_tasks[tid - 1];
    if(!(task->flags & TASK_DETACHED)) {
        goto out;
    }
    if(task->state != TASK_STATE_READY) {
        goto out;
    }

    SDL_LockMutex(s_ready_lock);
    bool found = pq_task_remove(&s_ready_queue, tasks_compare, task) 
              || pq_task_remove(&s_ready_queue_main, tasks_compare, task);
    SDL_UnlockMutex(s_ready_lock);

    if(!found)
        goto out;

    if(sched_curr_thread_tid() == NULL_TID) {
        sched_task_run(task);
        sched_task_service_request(task);
    }else{
        SDL_UnlockMutex(s_request_lock);
        S_Task_MaybeExit();

        Sched_Request((struct request){
            .type = _SCHED_REQ_RUN_SYNC,
            .argv = {tid}
        });

        S_Task_MaybeEnter();
        SDL_LockMutex(s_request_lock);
    }
    ret = true;

out:
    SDL_UnlockMutex(s_request_lock);
    return ret;
}

void Sched_ClearState(void)
{
    ASSERT_IN_MAIN_THREAD();

    sched_quiesce_workers();

    SDL_LockMutex(s_ready_lock);

    while(pq_size(&s_ready_queue)) {
        struct task *curr = NULL;
        pq_task_pop(&s_ready_queue, &curr);
        sched_task_cleanup(curr);
    }

    while(pq_size(&s_ready_queue_main)) {
        struct task *curr = NULL;
        pq_task_pop(&s_ready_queue_main, &curr);
        sched_task_cleanup(curr);
    }

    SDL_UnlockMutex(s_ready_lock);

    for(khiter_t k = kh_begin(s_event_queues); k != kh_end(s_event_queues); k++) {
        if(!kh_exist(s_event_queues, k))
            continue;

        queue_tid_t *queue = &kh_val(s_event_queues, k);
        for(int i = 0; i < queue_size(*queue); i++) {
            struct task *curr = &s_tasks[queue->mem[i] - 1];
            sched_task_cleanup(curr);
        }
        queue_tid_clear(queue);
    }

    for(int i = 0; i < MAX_TASKS; i++) {

        queue_tid_t *queue = &s_msg_queues[i];
        for(int i = 0; i < queue_size(*queue); i++) {
            struct task *curr = &s_tasks[queue->mem[i] - 1];
            sched_task_cleanup(curr);
        }
        queue_tid_clear(queue);

        if(s_tasks[i].state == TASK_STATE_SEND_BLOCKED) {
            struct task *curr = &s_tasks[i];
            sched_task_cleanup(curr);
        }
        s_parent_waiting[i] = false;
        s_tasks[i].state = TASK_STATE_ACTIVE;
    }

    /* Reset the free list */
    s_tasks[0].prev = NULL;
    s_tasks[0].next = &s_tasks[1];
    s_tasks[MAX_TASKS-1].prev = &s_tasks[MAX_TASKS - 2];
    s_tasks[MAX_TASKS-1].next = NULL;

    for(int i = 1; i < MAX_TASKS-1; i++) {
        s_tasks[i].next = &s_tasks[i + 1];
        s_tasks[i].prev = &s_tasks[i - 1];
    }
    s_freehead = s_tasks;
    s_nfree = MAX_TASKS;

    Task_CreateServices();
}

uint64_t Sched_Request(struct request req)
{
    uint32_t tid = sched_curr_thread_tid();
    struct task *task = &s_tasks[tid - 1];

    task->req = req;

    if(thisThreadID() == g_main_thread_id) {
        return sched_switch_ctx(&task->ctx, &s_main_ctx, 0, NULL);
    }else{
        int id = sched_curr_thread_worker_id();
        return sched_switch_ctx(&task->ctx, &s_worker_contexts[id], 0, NULL);
    }
}

uint32_t Sched_ActiveTID(void)
{
    return sched_curr_thread_tid();
}

bool Sched_FutureIsReady(const struct future *future)
{
    return (SDL_AtomicGet((SDL_atomic_t*)&future->status) == FUTURE_COMPLETE);
}

/* Like Sched_Create, but attempts to run a task to completion 
 * when it's not able to allocate a TID 
 */
uint32_t Sched_CreateBlocking(int prio, task_func_t code, void *arg, struct future *result, int flags)
{
    ASSERT_IN_MAIN_THREAD();

    bool status;
    struct task *task;
    uint32_t ret;

    do{
        ret = Sched_Create(prio, code, arg, result, flags);

        if(ret == NULL_TID) {

            SDL_LockMutex(s_ready_lock); 
            status = pq_task_pop(&s_ready_queue_main, &task) || pq_task_pop(&s_ready_queue, &task);
            SDL_UnlockMutex(s_ready_lock);

            if(status) {
                sched_task_run(task);
                sched_task_service_request(task);
            }else{
                /* All the tasks are blocked. Nothing more we can do until the next event pump */
                break;
            }
        }
    }while(ret == NULL_TID);

    return ret;
}

bool Sched_UsingBigStack(void)
{
    uint32_t tid = Sched_ActiveTID();
    if(tid == NULL_TID)
        return true;
    struct task *task = &s_tasks[tid - 1];
    return (task->flags & TASK_BIG_STACK);
}

void Sched_Flush(void)
{
    ASSERT_IN_MAIN_THREAD();

    sched_quiesce_workers();
    struct task *curr;

    while(pq_task_pop(&s_ready_queue_main, &curr)) {
        assert(curr);
        sched_task_run(curr);
        sched_task_service_request(curr);
    }
    while(pq_task_pop(&s_ready_queue, &curr)) {
        assert(curr);
        sched_task_run(curr);
        sched_task_service_request(curr);
    }
}

bool Sched_HasBlocked(void)
{
    ASSERT_IN_MAIN_THREAD();

    SDL_LockMutex(s_ready_lock); 
    bool ret = (pq_size(&s_ready_queue) > 0)
            || (pq_size(&s_ready_queue_main) > 0);
    SDL_UnlockMutex(s_ready_lock);
    return ret;
}

bool Sched_IsReady(uint32_t tid)
{
    bool ret = false;
    SDL_LockMutex(s_request_lock);
    ret = s_tasks[tid - 1].state == TASK_STATE_READY;
    SDL_UnlockMutex(s_request_lock);
    return ret;
}

