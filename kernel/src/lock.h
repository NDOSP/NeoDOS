#ifndef LOCK_H
#define LOCK_H

typedef struct {
    volatile int locked;
} Spinlock;

static inline void lock(Spinlock* s) {
    while(__sync_lock_test_and_set(&s->locked, 1)) {
        asm volatile("pause");
    }
}

static inline void unlock(Spinlock* s) {
    __sync_lock_release(&s->locked);
}

#endif // LOCK_H