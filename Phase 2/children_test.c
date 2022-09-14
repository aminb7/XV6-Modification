#include "types.h"
#include "stat.h"
#include "user.h"

void
test()
{
  if(fork() == 0) // child
  {
    if(fork() == 0 || fork() == 0 || fork() == 0) // child
    {
      sleep(100);
    }
    else // parent
    {
      int children2 = get_children(getpid());
      printf(1, "Children of root children: %d\n", children2);
      wait();
      wait();
      wait();
    }
    sleep(200);
  }
  else // parent
  {
    sleep(200);
    int children1 = get_children(getpid());
    int grandchildren1 = get_grandchildren(getpid());
    printf(1, "Root children: %d\n", children1);
    printf(1, "Root grandchildren: %d\n", grandchildren1);
    wait();
  }
}

int
main(int argc, char *argv[])
{
  test();
  exit();
}