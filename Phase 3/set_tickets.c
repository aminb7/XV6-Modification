#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc <= 2){
    exit();
  }

  set_tickets(atoi(argv[1]), atoi(argv[2]));
  exit();
}
