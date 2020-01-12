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

int n_ptrs = 134;
unsigned long pointers[] = 
{
    0xC17E47C0U,
    0xF5386A80U,
    0xF5805D80U,
    0xF5805E00U,
    0xF585B080U,
    0xF585B200U,
    0xF585B280U,
    0xF585B300U,
    0xF585B400U,
    0xF585BD80U,
    0xF5871500U,
    0xF5871580U,
    0xF5871600U,
    0xF5871A80U,
    0xF5903000U,
    0xF5903080U,
    0xF5903100U,
    0xF598CC00U,
    0xF598CE80U,
    0xF598CF00U,
    0xF598CF80U,
    0xF5BBB180U,
    0xF5BBB200U,
    0xF5BBBA80U,
    0xF5BBBC80U,
    0xF5BFF100U,
    0xF50AD000U,
    0xF50AD100U,
    0xF50AD200U,
    0xF520A300U,
    0xF5221280U,
    0xF5221200U,
    0xF356E480U,
    0xF356EC00U,
    0xF358E180U,
    0xF356E600U,
    0xF358CA00U,
    0xF358DA00U,
    0xF356E180U,
    0xEDA6B280U,
    0xF520AD80U,
    0xF520A200U,
    0xF5221300U,
    0xF358DB80U,
    0xF358B400U,
    0xF358BD00U,
    0xF358DC00U,
    0xF358EB00U,
    0xF358AE80U,
    0xF5386280U,
    0xF358A900U,
    0xF356E200U,
    0xF356EB00U,
    0xF5209D00U,
    0xF5226300U,
    0xF5209600U,
    0xF3589D80U,
    0xF5209480U,
    0xF3589200U,
    0xF5209280U,
    0xF356E680U,
    0xF5378A00U,
    0xF5378280U,
    0xF5206C00U,
    0xF3589E80U,
    0xF5226980U,
    0xF520A500U,
    0xF358AC80U,
    0xF5386480U,
    0xF358AC00U,
    0xF358AB80U,
    0xF5386000U,
    0xF5221780U,
    0xF358A600U,
    0xF358A980U,
    0xF5386F80U,
    0xF5226380U,
    0xF5221900U,
    0xF5206480U,
    0xF5226D80U,
    0xF5341700U,
    0xF5378E00U,
    0xF5206800U,
    0xF5386B80U,
    0xF5341680U,
    0xF5341A80U,
    0xF3588F00U,
    0xF5206380U,
    0xF5209F80U,
    0xF5206B00U,
    0xF358A480U,
    0xF5341980U,
    0xF5206180U,
    0xF5341D80U,
    0xF5341080U,
    0xF5341900U,
    0xF5206780U,
    0xF5341100U,
    0xF5341600U,
    0xF5206580U,
    0xF356E580U,
    0xF5206300U,
    0xF539EE00U,
    0xF358AE00U,
    0xF539EA00U,
    0xF5206C80U,
    0xF539E480U,
    0xF5206500U,
    0xF5221580U,
    0xF5378680U,
    0xF358E300U,
    0xF358DF80U,
    0xF520A900U,
    0xF358C800U,
    0xF356FF80U,
    0xF358C100U,
    0xF3588E80U,
    0xF3588A00U,
    0xF5209580U,
    0xF5341D00U,
    0xF377E400U,
    0xF377E780U,
    0xF5341200U,
    0xF377E880U,
    0xEDA3AB00U,
    0xF377E000U,
    0xEDA3AE80U,
    0xEDA6BB80U,
    0xEDA6BA00U,
    0xF377E600U,
    0xEDA3A300U,
    0xEDA3AC80U,
    0xEDA3A100U,
    0xEDA3A400U,
};

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

    unsigned long long t = 0;
    for (; c < 100; c ++)
    {
        int j = 0;
        for (; j < n_ptrs - 1; j ++)
        {
            queue[1] = pointers[j];
            queue[2] = 116 / 4; 
            queue[0] = 1; // lock
            t0 = rdtsc();
            while(queue[0]);
            t1 = rdtsc();
            t += t1 - t0;
        }
    }

    printf("%lld\n", t / 100);
    printf("%X\n", queue[3]);

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

