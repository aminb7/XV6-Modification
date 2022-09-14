#include "types.h"
#include "stat.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "user.h"

#define BUFFER_SIZE 5
#define STDOUT 1

void
producer()
{
  int in = 0;
  while (1)
  {
    /*
      Produce an item.
    */
    while (get_counter() == BUFFER_SIZE)
    ; // Wait

    in = (in + 1) % BUFFER_SIZE;

    semaphore_aquire(1);
    /*
      Update shared memory.
    */
    printf(STDOUT, "Producer aquired lock\n");
    inc_counter();
    printf(STDOUT, "Producer released lock\n");
    semaphore_release(1);

    sleep(300);
  }
}

void
consumer()
{
  int out = 0;
  while (1)
  {
    
    while (get_counter() == 0)
    ; // Wait
    out = (out + 1) % BUFFER_SIZE;

    semaphore_aquire(1);
    printf(2, "Consumer aquired lock\n");
    /*
      Update shared memory.
    */
    dec_counter();
    printf(STDOUT, "Consumer released lock\n");
    semaphore_release(1);
    
    /*
      Consume an item.
    */
    sleep(300);
  }
}

int
main(int argc, char *argv[])
{
  semaphore_initialize(1, 1, 0);
  int pid = fork();
  
  // Let child be producer
  if (pid == 0)
    producer();
  else
    consumer();

  wait();
  exit();

  // semaphore_initialize(1,1,0);
  // fork();
  // fork();
  // semaphore_aquire(1);
  // printf(2, "salaaaaaaaaaam\n");
  // sleep(100);
  // semaphore_release(1);
  // wait();
  // wait();  
  // exit();
}