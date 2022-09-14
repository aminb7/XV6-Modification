#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define SYS_GETPID_ADDR 0x00000070

int 
main() {
	int (*fptr) (void);
	fptr = (int(*)(void))SYS_GETPID_ADDR;
	fptr();
	exit();
}
