#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define PROCESS_COUNT 8
#define UPPER_BOUND 100000000

int main(int argc, char const *argv[])
{
    int pid = getpid();
    int i;

    for (i = 1; i < PROCESS_COUNT; i++)
    {
	if (pid > 0)
	{
	    if (i == 2)
	    {
		set_proc_queue(pid, 3);
		set_tickets(pid, 30);
	    }

	    else if (i == 4)
	    {
		set_proc_queue(pid, 3);
	    }

	    else if (i == 6)
	    {
		set_tickets(pid, 25);
	    }

	    else if (i == 8)
	    {
		set_proc_queue(pid, 2);
		set_tickets(pid, 15);
	    }

	    else if (i == PROCESS_COUNT - 1)
	    {
	//	print_info();
	    }
	   
	    pid = fork();
	    }
    }

    if(pid < 0)
    {
        printf(2, "fork failed!\n");
    }
    else if (pid == 0)
    {
        int sum = 0;
        for(int i = 1; i < UPPER_BOUND; i++)
		{
            sum += i;
			int j = 0;
			while (j < UPPER_BOUND)
			{
				j = j * 7;
			}
			
		}
		//printf (2, "", sum);	
    }
    else
    {
		for (i = 0; i < PROCESS_COUNT; i++)
			wait();
	//	printf(1, "The test has been conducted successfully.\n");
    }
	wait();
    exit();
}
