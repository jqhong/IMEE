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

struct arg_blk
{
    u64 cr3;
    u64 num_x_page;
    u64 num_w_page;
    u64 offset;
    u64 code_host;
    u64 data_host;
    u64 stack;
    u64 entry;
    u64 thunk;
    u64 got;
    u64 got_len;
    u64 gotplt;
    u64 gotplt_len;
};

struct arg_blk args;

static int go ();

int main()
{
    cpu_set_t cpuset;
    CPU_ZERO (&cpuset);
    CPU_SET (1, &cpuset);
    sched_setaffinity (0, sizeof (cpuset), &cpuset);

    void* dl_h = dlopen ("./vmi.so", RTLD_NOW);
    if (!dl_h)
    {
        printf ("%s\n", dlerror());
        exit(1);
    }
    // void* (*p2)() = (void*(*)()) dlsym (dl_h, "get_buf");
    // buffer = (*p2)();
    buffer = valloc (4096);
    // the watermark
    *((int*) (buffer + 4092)) = 0xDEADBEEF;

    p1 = dlsym (dl_h, "entry");
    printf ("p1: %p\n", p1);
    printf ("buffer: %p\n", buffer);
    printf ("pid: %d\n", getpid());

    // preparing args
    // dl_iterate_phdr (phdr_iterator, NULL);

    args.entry = (unsigned long) p1;
    args.code_host = args.entry & ~0xFFFU;
    args.data_host = buffer;
    args.num_x_page = 1;
    args.num_w_page = 1;

    mlockall(MCL_CURRENT);

    // start ImEE
    void* stk = valloc (4096);
    * (int*) stk = 0; // touch the page, so linux allocates it.
    clone (go, (char*)stk + 4092, CLONE_VM | CLONE_FS | CLONE_FILES, NULL);
    // go();

    sleep(1);

    volatile int* queue = (int*) buffer;
    volatile int tmp;

    int c = 0; 
    unsigned long long t = 0;

	unsigned long start = init_task; 
	unsigned long cur = start;
	unsigned long *res_buffer = (unsigned long *)malloc(4096 * 2);		
	memset(res_buffer, 0, 4096 * 2);
	

    for (; c < 100; c ++)
    {
        queue[1] = cur;
        queue[2] = ts_tasks;
        queue[3] = ts_cred;
        queue[4] = 1; // 4bytes
        queue[0] = 1; // lock
        t0 = rdtsc ();
        while(queue[0]);
        t1 = rdtsc ();
        t += t1 - t0;
    }

    printf("%lld\n", t / 100);

    int i = 0;
    for (; i < 150; i ++)
    {
		printf("%X\n", queue[i + 5]);
    }

    sleep (9999);

    return 0;
}

static int go ()
{
    cpu_set_t cpuset;

    CPU_ZERO (&cpuset);
    CPU_SET (2, &cpuset);
    sched_setaffinity (0, sizeof (cpuset), &cpuset);

    run_imee (&args, p1);

	return 0; 
}

static int phdr_iterator (struct dl_phdr_info* info, size_t size, void* data)
{
    if (!strcmp ("./vmi.so", info->dlpi_name))
    {
        args.code_host = (info->dlpi_addr + info->dlpi_phdr[0].p_vaddr) & ~0xFFF;
        args.data_host = (info->dlpi_addr + info->dlpi_phdr[1].p_vaddr) & ~0xFFF;
        args.offset = args.data_host - args.code_host - 0x1000; // in bytes
        args.num_x_page = (info->dlpi_phdr[0].p_memsz + 0xFFF) / 0x1000;
        args.num_w_page = (info->dlpi_phdr[1].p_memsz + 0xFFF) / 0x1000;
#ifdef DEBUG
        printf ("code_host: %lX data_host: %lX, offset: %lX\n", args.code_host, args.data_host, args.offset);
        printf ("num_x_page: %ld num_w_page: %ld\n", args.num_x_page, args.num_w_page);
#endif

        return 1;
    }

    return 0;
}

