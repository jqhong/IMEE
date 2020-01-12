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

/*
[ 2175.692663] task is f6b4e580
[ 2175.692665] offsets: ts_state=0x0, ts_pid=0x20c, ts_next=0x1bc, ts_comm=0x2ec, ts_prev=0x1c0
[ 2175.692668] task struct size: 0xca8

module
offsets: m_name=0xc, m_list.next=0x4, m_init_size=0xe0, m_core_size=0xe4

sys_call_table size; 349
sys_open 5
*/

static __attribute__((always_inline)) unsigned long long rdtsc(void)
{
    unsigned long long x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}
unsigned long long t0, t1;

#define SYS_SIZE 349

#define TS_SIZE 0xca8
#define TS_STATE 0x0
#define TS_PID 0x20c
#define TS_NEXT 0x1bc
#define TS_COMM 0x2ec
#define TS_PREV 0x1c0

#define M_NEXT 0x4
#define M_NAME 0xc
#define M_INIT 0xe0
#define M_CORE 0xe4
#define M_BASE 0xdc

#define SYS_SIZE 349
#define SYS_OPEN 5

#define MODULES 0xc17e8b18
#define RUNQUEUES 0xc18f2400 // runqueues
#define INIT_MM 0xc17ef700 // init_mm
#define INIT_TASK 0xc17d5fe0 // init_task
#define SYS_CALL_TABLE 0xc1587160  // sys_call_table

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
    int c = 0; 
    volatile int tmp;

    // unsigned long long t = 0;
    unsigned int last = 0;
    int flg = 0;
    while (1)
    {
        // queue[1] = 0;
        queue[1] = 0xc1586160U + 37 * 4;
        // printf ("put: %X\n", queue[3]);
        queue[2] = 4; // 4bytes
        queue[0] = 1; // lock
        // t0 = rdtsc();
        while(queue[0]);
        // if (flg && last != queue[3])
        // {
        //     break;
        // }
        // if (!flg) flg ++;
        // last = queue[3];
        // t1 = rdtsc();
        // t += t1 - t0;
    }

    // printf ("detected change: last: %X, now: %X\n", last, queue[3]);
    // printf("%lld\n", t / 10000);

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

