#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define CYCLES_PRECISION 1
#define RATIO_PRECISION 3

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct proc *prev_proc;
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->q_num = LOTTERY_QUEUE;
  p->waited_cycles = 0;
  p->executed_cycles = 0;
  acquire(&tickslock);
  p->arrival_time = ticks;
  release(&tickslock);
  p->tickets = 10;
  p->priority_ratio = 1;
  p->arrival_time_ratio = 1;
  p->executed_cycles_ratio = 1;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  if (curproc->name[0] == 's' && curproc->name[1] == 'h' && curproc->name[2] == 0)
    np->q_num = ROUND_ROBIN_QUEUE;

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

void
get_old(void)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state == RUNNABLE && p->waited_cycles > 10000)
    {
      p->waited_cycles = 0;
      if (p->q_num == LOTTERY_QUEUE)
        p->q_num = ROUND_ROBIN_QUEUE;
      else if(p->q_num == BJF_QUEUE)
        p->q_num = LOTTERY_QUEUE;
    }
  }
}

struct proc*
round_robin()
{
  struct proc *p;
  for(p = ptable.prev_proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE || p->q_num != ROUND_ROBIN_QUEUE)
        continue;
    
    if (p < &ptable.proc[NPROC - 1])
      ptable.prev_proc = p + 1;
    else
      ptable.prev_proc = ptable.proc;
    return p;
  }

  for(p = ptable.proc; p < ptable.prev_proc; p++){
    if(p->state != RUNNABLE || p->q_num != ROUND_ROBIN_QUEUE)
        continue;
    
    if (p < &ptable.proc[NPROC - 1])
      ptable.prev_proc = p + 1;
    else
      ptable.prev_proc = ptable.proc;
    return p;
  }

  p = NULL_PROC;
  return p;
}

struct proc*
lottery(void)
{
  struct proc *p = NULL_PROC;
  uint total_tickets = 0;
  uint cur_tickets = 0;
  uint random_ticket;
  uint random_number;
  int has_proc = 0;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE || p->q_num != LOTTERY_QUEUE)
      continue;

    total_tickets += p->tickets;
    has_proc = 1;
  }

  if(has_proc){
    random_number = ticks;
    random_ticket = (random_number) % total_tickets;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE || p->q_num != LOTTERY_QUEUE)
        continue;

      cur_tickets += p->tickets;

      if(random_ticket < cur_tickets){
        return p;
      }
    }
  }
  p = NULL_PROC;
  return p;
}

struct proc*
bjf(void)
{
  struct proc *p;
  struct proc *return_value = NULL_PROC;
  float rank_p = 0;
  float rank_rv = -1;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != RUNNABLE || p->q_num != BJF_QUEUE)
      continue;

    rank_p = (p->priority_ratio / p->tickets) + (p->arrival_time * p->arrival_time_ratio) + (p->executed_cycles * p->executed_cycles_ratio); 
    
    if (rank_rv == -1 || rank_p < rank_rv){
      rank_rv = rank_p;
      return_value = p;
    }
  }
  return return_value;
}

struct proc*
get_next_proc(void)
{
  struct proc *p = round_robin();
  
  if (p == NULL_PROC)
    p = lottery();
  
  if (p == NULL_PROC)
    p = bjf();

  return p;
}

void
update_waited_cycles(struct proc* executing_proc, uint cycles)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == RUNNABLE)
      p->waited_cycles += cycles;
  
  executing_proc->waited_cycles = 0;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  uint t1, t2;
  c->proc = 0;
  
  ptable.prev_proc = ptable.proc;
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    t1 = ticks;
    p = get_next_proc();

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    if (p != NULL_PROC) 
    {
      get_old();
      p->executed_cycles += 0.1;
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();
      t2 = ticks;
      update_waited_cycles(p, t2 - t1);
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void
set_tickets(int pid, int tickets)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if (p->pid == pid){
      p->tickets = tickets;
      return;
    }
  }
}

void
set_proc_queue(int pid, int q_num)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if (p->pid == pid){
      p->q_num = q_num;
      return;
    }
  }
}

void
set_bjf_params_in_proc(int pid, int pratio, int atratio, int excratio)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if (p->pid == pid){
      p->priority_ratio = pratio;
      p->arrival_time_ratio = atratio;
      p->executed_cycles_ratio = excratio;
      return;
    }
  }
}

void
set_bjf_params_in_system(int pratio, int atratio, int excratio)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    p->priority_ratio = pratio;
    p->arrival_time_ratio = atratio;
    p->executed_cycles_ratio = excratio;
  }
  return;
}

// print
void adjust_columns(int space_ahead)
{
  for (int i = 0; i < space_ahead; i++)
    cprintf(" ");
  return;
}

int
count_digits(int number)
{
  int digits = 0;
  if (number == 0)
  {
    digits = 1;
  }
  else
  {
    while(number != 0)
    {
      number /= 10;
      digits++;
    }
  }
  return digits;
}

void reverse_string(char* str, int strlen) 
{ 
    int start = 0, end = strlen - 1, temp; 
    while (start < end) { 
        temp = str[start]; 
        str[start] = str[end]; 
        str[end] = temp; 
        start++; 
        end--; 
    } 
} 


int itoa(int num, char snum[], int base) 
{ 
    int i = 0; 

    if(num == 0)
      snum[i++] = '0';

    while (num) { 
        snum[i++] = (num % 10) + '0'; 
        num = num / 10; 
    } 
  
    while (i < base) 
        snum[i++] = '0'; 
  
    reverse_string(snum, i); 
    snum[i] = '\0'; 
    return i; 
} 

int pow(int x, unsigned int y) 
{ 
    if (y == 0) 
        return 1; 
    else if (y % 2 == 0) 
        return pow(x, y / 2) * pow(x, y / 2); 
    else
        return x * pow(x, y / 2) * pow(x, y / 2); 
} 
  
void gcvt(float num, char* snum, int precision) 
{ 
    int integer = (int)num; 
  
    float fraction = num - (float)integer; 
  
    int i = itoa(integer, snum, 0); 
  
    if (precision != 0) { 
        snum[i] = '.'; 
  
        fraction = fraction * pow(10, precision); 
  
        itoa((int)fraction, snum + i + 1, precision); 
    } 
}



double calculate_rank(struct proc *p)
{
  float priority_share = p->priority_ratio / p->tickets;
  float arrival_time_share = p->arrival_time * p->arrival_time_ratio;
  float executed_cycles_share = p->executed_cycles * p->executed_cycles_ratio;
  return (priority_share + arrival_time_share + executed_cycles_share);
}

void print_info(void)
{
  static char *states[] = {
      [UNUSED] "UNUSED",
      [EMBRYO] "EMBRYO",
      [SLEEPING] "SLEEPING",
      [RUNNABLE] "RUNNABLE",
      [RUNNING] "RUNNING",
      [ZOMBIE] "ZOMBIE"};

  enum column_names
  {
    NAME,
    PID,
    STATE,
    QUEUE_NUM,
    TICKET,
    PRIORITY_RATIO,
    ARRIVAL_TIME_RATIO,
    EXECUTED_CYCLES_RATIO,
    RANK,
    CYCLES
  };
  static const int table_columns = 10;

  static const char *titles_str[] = {
      [NAME] "name",
      [PID] "pid",
      [STATE] "state",
      [QUEUE_NUM] "queue_num",
      [TICKET] "ticket",
      [PRIORITY_RATIO] "priority_ratio",
      [ARRIVAL_TIME_RATIO] "arrival_time_ratio",
      [EXECUTED_CYCLES_RATIO] "executed_cycles_ratio",
      [RANK] "rank",
      [CYCLES] "cycles",
  };
  int min_space_between_words = 8;
  int max_column_lens[] = {
      [NAME] 10 + min_space_between_words,
      [PID] strlen(titles_str[PID]) + min_space_between_words,
      [STATE] 8 + min_space_between_words,
      [QUEUE_NUM] strlen(titles_str[QUEUE_NUM]) + min_space_between_words,
      [TICKET] strlen(titles_str[TICKET]) + min_space_between_words,
      [PRIORITY_RATIO] 10 + min_space_between_words,
      [ARRIVAL_TIME_RATIO] 12 + min_space_between_words,
      [EXECUTED_CYCLES_RATIO] 15 + min_space_between_words,
      [RANK] 5 + min_space_between_words,
      [CYCLES] strlen(titles_str[CYCLES]) + min_space_between_words};

  for (int i = 0; i < table_columns; i++)
  {
    cprintf("%s", titles_str[i]);
    adjust_columns(max_column_lens[i] - strlen(titles_str[i]));
  }
  cprintf("\n------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

  struct proc *p;
  char *state;
  int ticket_len;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == 0)
      continue;
    state = states[p->state];
    cprintf("%s", p->name);
    adjust_columns(max_column_lens[NAME] - strlen(p->name));
    cprintf("%d", p->pid);
    adjust_columns(max_column_lens[PID] - count_digits(p->pid));
    cprintf("%s", state);
    adjust_columns(max_column_lens[STATE] - strlen(state));
    cprintf("%d", p->q_num);
    adjust_columns(max_column_lens[QUEUE_NUM] - count_digits(p->q_num));
    cprintf("%d", p->tickets);
    ticket_len = count_digits(p->tickets);

    adjust_columns(max_column_lens[TICKET] - ticket_len);

    char priority_ratio_str[30];
    gcvt(p->priority_ratio, priority_ratio_str, 0);
    cprintf("%s", priority_ratio_str);
    adjust_columns(max_column_lens[PRIORITY_RATIO] - strlen(priority_ratio_str));

    char arrival_time_ratio_str[30];
    gcvt(p->arrival_time_ratio, arrival_time_ratio_str, 0);
    cprintf("%s", arrival_time_ratio_str);
    adjust_columns(max_column_lens[ARRIVAL_TIME_RATIO] - strlen(arrival_time_ratio_str));


    char executed_cycles_ratio_str[30];
    gcvt(p->executed_cycles_ratio, executed_cycles_ratio_str, 0);
    cprintf("%s", executed_cycles_ratio_str);
    adjust_columns(max_column_lens[EXECUTED_CYCLES_RATIO] - strlen(executed_cycles_ratio_str));


    double rank = calculate_rank(p);

    char rank_str[30];
    gcvt(rank, rank_str, RATIO_PRECISION);

    cprintf("%s", rank_str);
    adjust_columns(max_column_lens[RANK] - strlen(rank_str));
    
    char cycles_str[30];
    gcvt(p->executed_cycles, cycles_str, CYCLES_PRECISION);

    cprintf("%s\n", cycles_str);
    cprintf("\n");
  }
  release(&ptable.lock);
}
