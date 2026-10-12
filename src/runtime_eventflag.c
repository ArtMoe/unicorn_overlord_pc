/* Kernel event flags: a 64-bit pattern that threads wait on (AND/OR of bits, optional
 * clear on wake-up). Handles are 64-bit IDs. Waiters are served in FIFO order on every
 * set, so a clearing waiter hides the bits from the ones queued after it, as on the PS4. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#define ERR(n) ((int32_t)(UINT32_C(0x80020000)|(n)))
enum { ATTR_SINGLE=0x10, ATTR_MULTI=0x20,
       WAIT_AND=0x01, WAIT_OR=0x02, CLEAR_ALL=0x10, CLEAR_PAT=0x20 };
typedef struct Waiter {
    uint64_t bits, result_pattern;
    uint32_t mode;
    int32_t result;
    int done;
    pthread_cond_t event;
    struct Waiter *next;
} Waiter;
typedef struct Flag {
    uint64_t id, pattern;
    uint32_t attr;
    unsigned active;
    int deleted;
    char name[32];
    Waiter *first;
    struct Flag *next;
} Flag;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static Flag *registry;
static uint64_t next_id=1;
static size_t created, deleted, sets, waits, timed_out;
static Flag *find(uint64_t id) {
    for (Flag *f=registry;f;f=f->next) if (f->id==id && !f->deleted) return f;
    return NULL;
}
static void host_check(int e) {
    if (e) { fprintf(stderr,"STOP: event flag host error %d\n",e); exit(21); }
}
/* Same deadline clock as the semaphores (winpthreads only accepts CLOCK_REALTIME). */
#ifdef _WIN32
#define WAIT_CLOCK CLOCK_REALTIME
#else
#define WAIT_CLOCK CLOCK_MONOTONIC
#endif
static uint64_t now_ns(void) {
    struct timespec t;
    if (clock_gettime(WAIT_CLOCK,&t)) { perror("clock_gettime"); exit(1); }
    return (uint64_t)t.tv_sec*1000000000+(uint64_t)t.tv_nsec;
}
static int valid_mode(uint32_t mode) {
    uint32_t wait=mode&(WAIT_AND|WAIT_OR), clear=mode&(CLEAR_ALL|CLEAR_PAT);
    return (wait==WAIT_AND || wait==WAIT_OR) && clear!=(CLEAR_ALL|CLEAR_PAT) &&
           !(mode&~(uint32_t)(WAIT_AND|WAIT_OR|CLEAR_ALL|CLEAR_PAT));
}
static int satisfied(uint64_t pattern,uint64_t bits,uint32_t mode) {
    return mode&WAIT_AND ? (pattern&bits)==bits : (pattern&bits)!=0;
}
static void clear_after(Flag *f,uint64_t bits,uint32_t mode) {
    if (mode&CLEAR_ALL) f->pattern=0;
    else if (mode&CLEAR_PAT) f->pattern&=~bits;
}
static ABI int32_t flag_create(uint64_t *out,const char *name,uint32_t attr,uint64_t pattern,const void *options) {
    if (!out || !name || options || (attr&~(uint32_t)0x33) || (attr&0x30)==0x30) return ERR(22);
    Flag *f=calloc(1,sizeof(*f));
    if (!f) return ERR(12);
    pthread_mutex_lock(&lock);
    f->id=next_id++; f->attr=attr; f->pattern=pattern;
    snprintf(f->name,sizeof(f->name),"%s",name);
    f->next=registry; registry=f; *out=f->id; ++created;
    printf("Runtime: event flag id=%llu \"%s\", attr=%#x, pattern=%#llx\n",
           (unsigned long long)f->id,f->name,attr,(unsigned long long)pattern);
    pthread_mutex_unlock(&lock);
    return 0;
}
static ABI int32_t flag_delete(uint64_t id) {
    pthread_mutex_lock(&lock);
    Flag *f=find(id);
    int32_t result=0;
    if (!f) result=ERR(3);
    else {
        f->deleted=1; ++deleted;
        for (Waiter *w=f->first;w;w=w->next) {
            w->result=ERR(13); w->result_pattern=f->pattern; w->done=1; /* EACCES, as the PS4 */
            host_check(pthread_cond_signal(&w->event));
        }
        f->first=NULL;
        for (Flag **p=&registry;*p;p=&(*p)->next) if (*p==f) { *p=f->next; break; }
        if (!f->active) free(f);
    }
    pthread_mutex_unlock(&lock);
    return result;
}
static ABI int32_t flag_set(uint64_t id,uint64_t bits) {
    pthread_mutex_lock(&lock);
    Flag *f=find(id);
    int32_t result=0;
    if (!f) result=ERR(3);
    else {
        f->pattern|=bits; ++sets;
        Waiter **p=&f->first;
        while (*p) {
            Waiter *w=*p;
            if (!satisfied(f->pattern,w->bits,w->mode)) { p=&w->next; continue; }
            w->result_pattern=f->pattern; clear_after(f,w->bits,w->mode);
            *p=w->next; w->done=1;
            host_check(pthread_cond_signal(&w->event));
        }
    }
    pthread_mutex_unlock(&lock);
    return result;
}
/* Sets the pattern and releases every waiter with ECANCELED; *count gets how many. */
static ABI int32_t flag_cancel(uint64_t id,uint64_t pattern,int32_t *count) {
    pthread_mutex_lock(&lock);
    Flag *f=find(id);
    int32_t result=0, released=0;
    if (!f) result=ERR(3);
    else {
        f->pattern=pattern;
        for (Waiter *w=f->first;w;w=w->next) {
            w->result=ERR(89); w->result_pattern=pattern; w->done=1; ++released;
            host_check(pthread_cond_signal(&w->event));
        }
        f->first=NULL;
        if (count) *count=released;
    }
    pthread_mutex_unlock(&lock);
    return result;
}
static ABI int32_t flag_clear(uint64_t id,uint64_t bits) {
    pthread_mutex_lock(&lock);
    Flag *f=find(id);
    int32_t result=0;
    if (!f) result=ERR(3);
    else f->pattern&=bits;
    pthread_mutex_unlock(&lock);
    return result;
}
static int32_t wait_flag(uint64_t id,uint64_t bits,uint32_t mode,uint64_t *out,uint32_t *timeout,int block) {
    if (!bits || !valid_mode(mode)) return ERR(22);
    pthread_mutex_lock(&lock);
    Flag *f=find(id);
    int32_t result=0;
    if (!f) result=ERR(3);
    else if (satisfied(f->pattern,bits,mode)) {
        if (out) *out=f->pattern;
        clear_after(f,bits,mode); ++waits;
    } else if (!block) { result=ERR(16); if (out) *out=f->pattern; }
    else if (!(f->attr&ATTR_MULTI) && f->first) result=ERR(1); /* single-waiter flag already waited on */
    else if (timeout && !*timeout) { result=ERR(60); if (out) *out=f->pattern; ++timed_out; }
    else {
        Waiter w={.bits=bits,.mode=mode};
        pthread_condattr_t attr;
        host_check(pthread_condattr_init(&attr));
        host_check(pthread_condattr_setclock(&attr,WAIT_CLOCK));
        host_check(pthread_cond_init(&w.event,&attr));
        host_check(pthread_condattr_destroy(&attr));
        Waiter **tail=&f->first;
        while (*tail) tail=&(*tail)->next;
        *tail=&w; ++f->active;
        uint64_t deadline=timeout ? now_ns()+(uint64_t)*timeout*1000 : 0;
        struct timespec end={.tv_sec=(time_t)(deadline/1000000000),.tv_nsec=(long)(deadline%1000000000)};
        while (!w.done) {
            int e=timeout ? pthread_cond_timedwait(&w.event,&lock,&end) : pthread_cond_wait(&w.event,&lock);
            if (e==ETIMEDOUT && !w.done) {
                Waiter **p=&f->first;
                while (*p!=&w) p=&(*p)->next;
                *p=w.next; w.result=ERR(60); w.result_pattern=f->pattern; w.done=1; ++timed_out;
            } else if (e!=ETIMEDOUT) host_check(e);
        }
        result=w.result;
        if (!result) ++waits;
        if (out) *out=w.result_pattern;
        if (timeout) {
            uint64_t now=now_ns();
            *timeout=result ? 0 : (uint32_t)(now>=deadline ? 0 : (deadline-now)/1000);
        }
        host_check(pthread_cond_destroy(&w.event));
        if (!--f->active && f->deleted) free(f);
    }
    pthread_mutex_unlock(&lock);
    return result;
}
static ABI int32_t flag_wait(uint64_t id,uint64_t bits,uint32_t mode,uint64_t *out,uint32_t *timeout) {
    return wait_flag(id,bits,mode,out,timeout,1);
}
static ABI int32_t flag_poll(uint64_t id,uint64_t bits,uint32_t mode,uint64_t *out) {
    return wait_flag(id,bits,mode,out,NULL,0);
}
static const RuntimeExport exports[]={
    {"sceKernelCreateEventFlag",flag_create}, {"sceKernelDeleteEventFlag",flag_delete},
    {"sceKernelSetEventFlag",flag_set}, {"sceKernelClearEventFlag",flag_clear},
    {"sceKernelWaitEventFlag",flag_wait}, {"sceKernelPollEventFlag",flag_poll},
    {"sceKernelCancelEventFlag",flag_cancel},
};
uintptr_t runtime_eventflag_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
void runtime_eventflag_report(void) {
    printf("Runtime: event flags created=%zu, deleted=%zu, sets=%zu, waits=%zu, timeouts=%zu\n",
           created,deleted,sets,waits,timed_out);
}
