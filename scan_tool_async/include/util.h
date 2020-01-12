#define init_task 0xc17d3fe0
#define sys_call_table 0xc1586160
#define modules 0xc17e6b18
#define nf_hooks 0xc183b0e0
#define slab_caches 0xc17ee0b4

#define ts_tasks 0x1bc
#define ts_pid 0x20c
#define ts_comm 0x2ec
#define ts_files 0x390
#define ts_cred 0x2e4
#define ts_mm 0x1d8

#define drv_list 0x4
#define drv_name 0xc

#define files_fdt 0x4
#define fdt_fd 0x4

#define nfhook_list 0x0
#define nfhook_hook 0x8
#define nfhook_size 0x1c

#define kmem_size 0xc
#define kmem_name 0x40
#define kmem_list 0x44

#define mm_vma 0x0

#define vma_vm_start 0x4
#define vma_vm_end 0x8
#define vma_vm_next 0xc

static __attribute__((always_inline)) unsigned long long rdtsc(void)
{
    unsigned long long x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}
unsigned long t0, t1;
