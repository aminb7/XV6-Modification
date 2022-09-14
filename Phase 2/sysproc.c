#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

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

////////////////////////////////////////////////////////
/////////////////////////////////// Get children handler
int
sys_get_children(void)
{
  int pid;
  if(argint(0, &pid) < 0)
    return -1;
  
  // Aray of children's pids
  int* chilldren_array = children(pid);

  // Number made by concating all pids

  int output_number = 0;
  int i = 0, modulus = 1, temp_pid = 0;
	while (chilldren_array[i])
	{
		output_number += modulus * chilldren_array[i];
    temp_pid = chilldren_array[i];
    while (temp_pid != 0) 
    {
      temp_pid /= 10;
      modulus *= 10;
    }
    i++;
	}
  print_trace_syscalls();
  return output_number;
}
////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
////////////////////////////// Get grandchildren handler
int
sys_get_grandchildren(void)
{
  int pid;
  if(argint(0, &pid) < 0)
    return -1;
  
  // Aray of grandchildren's pids
  int* chilldren_array = grandchildren(pid);

  // Number made by concating all pids
  int output_number = 0;
  int i = 0, modulus = 1, temp_pid = 0;
	while (chilldren_array[i])
	{
		output_number += modulus * chilldren_array[i];
    temp_pid = chilldren_array[i];
    while (temp_pid != 0) 
    {
      temp_pid /= 10;
      modulus *= 10;
    }
    i++;
	}

  return output_number;
}
////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
///////////////////////////////// Trace syscalls handler
int
sys_trace_syscalls(void)
{
  int state;
  if(argint(0, &state) < 0)
    return -1;

  set_trace_syscalls_state(state);

  uint ticks0 = ticks;
  if (myproc()->pid == 2)
    while (1){
      acquire(&tickslock);
      if (ticks - ticks0 > 500)
      {
        print_trace_syscalls();
        ticks0 = ticks;
      }
      release(&tickslock);
    }

  if (state == 0)
    reset_trace_syscalls();

  return 1;
}
////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

////////////////////////////////////////////////////////
///////////////////////////////// Reverse number handler
int
sys_reverse_number(void)
{
  int num = myproc()->tf->edx;
  while (num != 0)
  {
    cprintf("%d", num % 10);
    num = num / 10;
  }
  cprintf("\n");
  return 0;
}
////////////////////////////////////////////////////////
////////////////////////////////////////////////////////
