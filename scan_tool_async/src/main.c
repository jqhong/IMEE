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

//struct arg_blk
//{
//    u64 cr3;
//    u64 num_x_page;
//    u64 num_w_page;
//    u64 offset;
//    u64 code_host;
//    u64 data_host;
//    u64 stack;
//    u64 entry;
//    u64 int_handler;
//    u64 thunk;
//    u64 got;
//    u64 got_len;
//    u64 gotplt;
//    u64 gotplt_len;
//};

struct arg_blk args;

static int go ();

int main()
{
    cpu_set_t cpuset;
    CPU_ZERO (&cpuset);
    CPU_SET (1, &cpuset);
    sched_setaffinity (0, sizeof (cpuset), &cpuset);

    printf("This is parent thread: %d\n", getpid());

    void* dl_h = dlopen ("./vmi.so", RTLD_NOW);
    if (!dl_h)
    {
        printf ("%s\n", dlerror());
        exit(1);
    }
    // void* (*p2)() = (void*(*)()) dlsym (dl_h, "get_buf");
    // buffer = (*p2)();
    buffer = valloc (4096);
    *((int*) (buffer)) = 0;

    p1 = dlsym (dl_h, "entry");
    // printf ("p1: %p\n", p1);
    // printf ("buffer: %p\n", buffer);
    // printf ("pid: %d\n", getpid());

    // preparing args
    // dl_iterate_phdr (phdr_iterator, NULL);

    args.entry = (unsigned long) p1;
    args.code_host = args.entry & ~0xFFFU;
    args.data_host = buffer;
    args.num_x_page = 1;
    args.num_w_page = 1;

    mlockall(MCL_CURRENT);

    // make introspection run on another CPU
    void* stk = valloc (4096);
    * (int*) stk = 0; // touch the page, so linux allocates it.
    clone (go, (char*)stk + 4092, CLONE_VM | CLONE_FS | CLONE_FILES, NULL);

    setup_imee (&args, p1);

    int r = run_imee ();


    // unsigned long long t2 = *(((unsigned long long*) queue) + 510);
    // printf("%lld\n", t);
    // printf("t1: %llX t2: %llX t0: %llX t1 - t2: %lld\n", t1, t2, t0, t1 - t2);
    // printf("t2 - t0: %lld\n", t2 - t0);

    // for (i = 0; i < 40; i ++)
    // {
	// 	printf("%d %s\n", *(queue + 7 + i * 5), (queue +7 + i * 5 + 1));
    // }

    // sleep (9999);

    printf("Parent thread quitting: %d\n", getpid());

    return 0;
}

static int go ()
{
    cpu_set_t cpuset;

    CPU_ZERO (&cpuset);
    CPU_SET (2, &cpuset);
    sched_setaffinity (0, sizeof (cpuset), &cpuset);

    printf("This is child thread: %d\n", getpid());

    sleep(1);

    printf("Background thread started.\n");
    // printf("queue[0]: %d\n", *(int*)buffer);

    volatile int* queue = (int*) buffer;

    int i = 0;
    for (i = 0; i < 40; i ++)
    {
        queue[0] = 0;
        queue[1] = sys_call_table + i * 4;
        queue[2] = 4; // 4bytes
        queue[0] = 1; // lock
        while(queue[0]);
        printf ("%X\n", queue[3]);
    }


    queue[0] = 2; // put a 2, so the ImEE thread exits

    printf("Child thread quitting: %d\n", getpid());

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

