#include "types.h"
#include "stat.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "user.h"

#define BUFFER_SIZE 5
#define EMPTY 0
#define FULL 1
#define MUTEX 2
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
    in = (in + 1) % BUFFER_SIZE;

    semaphore_aquire(EMPTY);
    semaphore_aquire(MUTEX);
    printf(STDOUT, "Producer aquired locks.\n");
    /*
      Update shared memory.
    */
    inc_counter();
    printf(STDOUT, "Counter is: %d\n", get_counter());
    printf(STDOUT, "Producer releasing locks.\n");
    semaphore_release(MUTEX);
    semaphore_release(FULL);

    sleep(300);
  }
}

void
consumer()
{
  int out = 0;
  while (1)
  {
    out = (out + 1) % BUFFER_SIZE;

    semaphore_aquire(FULL);
    semaphore_aquire(MUTEX);
    printf(STDOUT, "Consumer aquired locks.\n");
    /*
      Update shared memory.
    */
    dec_counter();
    printf(STDOUT, "Counter is: %d\n", get_counter());
    printf(STDOUT, "Consumer releasing locks...\n");
    semaphore_release(MUTEX);
    semaphore_release(EMPTY);

    /*
      Consume an item.
    */
    sleep(500);
  }
}

int
main(int argc, char *argv[])
{
  semaphore_initialize(FULL, BUFFER_SIZE, BUFFER_SIZE);
  semaphore_initialize(MUTEX, 1, 0);
  semaphore_initialize(EMPTY, BUFFER_SIZE, 0);
  int pid = fork();
  
  // Let child be producer
  if (pid == 0)
    producer();
  else
    consumer();

  wait();
  exit();
}