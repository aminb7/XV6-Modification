#include "types.h"
#include "stat.h"
#include "user.h"

void
test(char *argv[])
{
  printf(1, "testing ... \n");
  getpid();
  reverse_number(2);
  get_children(getpid());
  if (fork() == 0)
  {
    exec("ls", argv);
    sleep(1000);
  }
  else
  {
    sleep(1000);
    wait();
  }
}

int
main(int argc, char *argv[])
{
  if (argc == 1)
    test(argv);
  else if (argc == 2)
  {
    if (!strcmp(argv[1], "on"))
      trace_syscalls(1);
    else if (!strcmp(argv[1], "off"))
      trace_syscalls(0);
  }
  exit();
}