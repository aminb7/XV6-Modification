#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_semaphore_initialize(void)
{
  int i;
  int v;
  int m;

  if(argint(0, &i) < 0)
    return -1;
  if(argint(1, &v) < 0)
    return -1;
  if(argint(2, &m) < 0)
    return -1;

  semaphore_initialize(i, v, m);
  return 0;
}

int
sys_semaphore_aquire(void)
{
  int i;
  if (argint(0, &i) < 0)
    return -1;
  semaphore_aquire(i);
  return 0;
}

int
sys_semaphore_release(void)
{
  int i;
  if (argint(0, &i) < 0)
    return -1;
  semaphore_release(i);
  return 0;
}

void
sys_inc_counter(void)
{
  inc_counter();
}

void
sys_dec_counter(void)
{
  dec_counter();
}

void
sys_reset_counter(void)
{
  reset_counter();
}

int
sys_get_counter(void)
{
  return get_counter();
}

int
sys_cv_wait(void) {
  struct condvar *cv;
  if (argptr(0, (void*)&cv, sizeof(*cv)) < 0)
    return -1;
  cv_wait(cv);
  return 0;
}

int
sys_cv_signal(void) {
  struct condvar *cv;
  if (argptr(0, (void*)&cv, sizeof(*cv)) < 0)
    return -1;
  cv_signal(cv);
  return 0;
}

int
sys_acquire_lock(void)
{
  struct spinlock *cv;
  if (argptr(0, (void*)&cv, sizeof(*cv)) < 0)
    return -1;
  acquire_lock(cv);
  return 0;
}

int
sys_release_lock(void)
{
  struct spinlock *cv;
  if (argptr(0, (void*)&cv, sizeof(*cv)) < 0)
    return -1;
  release_lock(cv);
  return 0;
}
