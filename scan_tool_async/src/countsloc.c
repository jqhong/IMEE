#define _GNU_SOURCE
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <link.h>
#include <sched.h>
#include <linux/types.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#include "imee.h"
#include "util.h"

void* p1;
void* buffer;


int main()
{
    cpu_set_t cpuset;
    CPU_ZERO (&cpuset);
    CPU_SET (1, &cpuset);
    sched_setaffinity (0, sizeof (cpuset), &cpuset);


    volatile int* queue = (int*) buffer;
    volatile int tmp;

    int r = 0;
    int c = 0; 
    unsigned long long t = 0;

	unsigned long start = init_task; 
	unsigned long cur = start;
	void *res_buffer = (void *)malloc(4096 * 2);		
	memset(res_buffer, 0, 4096 * 2);
	

    // for (; c < 100; c ++)
    // {
        queue[1] = cur;
        queue[2] = ts_tasks;
        queue[3] = ts_pid;
        queue[4] = 1; // 4bytes
        queue[5] = ts_comm;
        queue[6] = 4; // 16bytes
        queue[0] = 1; // lock
        t0 = rdtsc ();
        r = run_imee ();
        // while(queue[0]);
        t1 = rdtsc ();
        if (r < 0) 
        {
            printf ("sleeping\n");
            sleep (9999);
            return 0;
        }
        t += t1 - t0;
    // }

    // printf("%lld\n", t / 100);
    printf("%lld %lld %lld\n", t0, t1, t);

    int i = 0;
    for (; i < 30; i ++)
    {
		// printf("%d %s\n", *(queue + 7 + i * 5), (queue +7 + i * 5 + 1));
    }

    // sleep (9999);

    return 0;
}
