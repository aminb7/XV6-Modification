#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"
#include "spinlock.h"

struct user_spinlock {
    uint locked;
    struct spinlock lk;
};

void
init_user_lock(struct user_spinlock *lk) {
  lk->locked = 0;
}

void user_lock(struct user_spinlock *lk) {
  while(xchg(&lk->locked, 1) != 0)
    ;
}

void
user_unlock(struct user_spinlock *lk) {
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );
}
