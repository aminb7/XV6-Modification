#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc <= 4){
    printf(1, "Too few args\n");
    exit();
  }

  set_bjf_params_in_proc(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
  exit();
}
