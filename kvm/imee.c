#include <linux/kvm_host.h>
#include <linux/kvm.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/percpu.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>

#include <asm/desc.h>
#include <asm/apic.h>
#include "imee.h"

LIST_HEAD (introspection_contexts);

DECLARE_PER_CPU(unsigned long, old_rsp);

intro_ctx_t* current_target;
EXPORT_SYMBOL_GPL (current_target);

int imee_scan_mode = SCAN_ALL;
EXPORT_SYMBOL_GPL (imee_scan_mode);

volatile int exit_flg;
EXPORT_SYMBOL_GPL (exit_flg);

volatile struct kvm_vcpu* imee_vcpu;
EXPORT_SYMBOL_GPL (imee_vcpu);

spinlock_t sync_lock;
EXPORT_SYMBOL_GPL(sync_lock);

volatile unsigned char go_flg;
EXPORT_SYMBOL_GPL(go_flg);

volatile int imee_pid;
EXPORT_SYMBOL_GPL(imee_pid);

volatile u32 last_cr3;
EXPORT_SYMBOL_GPL(last_cr3);

volatile u32 last_rip;
EXPORT_SYMBOL_GPL(last_rip);

volatile u32 last_rsp;
EXPORT_SYMBOL_GPL(last_rsp);

volatile u32 switched_cr3;
EXPORT_SYMBOL_GPL(switched_cr3);

struct desc_ptr imee_idt, imee_gdt;
EXPORT_SYMBOL_GPL(imee_idt);
EXPORT_SYMBOL_GPL(imee_gdt);

struct kvm_segment imee_tr;
EXPORT_SYMBOL_GPL(imee_tr);

volatile int demand_switch;
EXPORT_SYMBOL_GPL(demand_switch);

int imee_up;
u32* temp;
int trial_run;
EXPORT_SYMBOL_GPL(trial_run);

int enable_notifier;

volatile int do_switch;
EXPORT_SYMBOL_GPL(do_switch);

#define NBASE 4
void* p_bases[NBASE];
void* p_base;
int p_base_idx;
int p_idx;
#define PAGE_ORDER 10

ulong code_hpa, data_hpa;
EXPORT_SYMBOL_GPL(code_hpa);
EXPORT_SYMBOL_GPL(data_hpa);
ulong code_entry;
EXPORT_SYMBOL_GPL(code_entry);

ulong fake_cr3_pd_hpa;
ulong fake_cr3_pt_hpa_exec;
ulong fake_cr3_pt_hpa_data;

struct arg_blk imee_arg;

struct region 
{
    u32 start;
    u32 end;
    int type;
};

// 64bit guest, 64bit host
// #define GPA_MASK (0xFFFUL | (1UL << 63))
// 32bit guest, 64bit host
#define GPA_MASK (0xFFFU)
// 32bit guest, 32bit host
// #define GPA_MASK (0xFFFU)
// 32bit PAE guest...
// ...

// 64bit
#define HPA_MASK (0xFFFUL | (1UL << 63))
// 32bit
// #define HPA_MASK (0xFFFU)
// 32bit PAE
// #define HPA_MASK (0xFFFULL | (1ULL << 63))

// 64bit
#define EPT_MASK (0xFFFUL | (1UL << 63))
// 32bit
// #define EPT_MASK (0xFFFULL | (1ULL << 63))

static __attribute__((always_inline)) unsigned long long rdtsc(void)
{
    unsigned long long x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}
unsigned long long t0, t1;
unsigned long long total_cycle;
unsigned long long setup_cycle;

unsigned long long imee_t;
EXPORT_SYMBOL_GPL(imee_t);

unsigned long long t[100];
int cycle_idx = 0;

unsigned long long* ts_buffer1;
EXPORT_SYMBOL_GPL(ts_buffer1);
volatile int ts_buffer_idx1;
EXPORT_SYMBOL_GPL(ts_buffer_idx1);
volatile int ts_buffer_idx1_limit;
EXPORT_SYMBOL_GPL(ts_buffer_idx1_limit);

spinlock_t sync_lock1;
EXPORT_SYMBOL_GPL(sync_lock1);

int __tmp_counter1;
int __tmp_counter2;
int __tmp_counter4;
int __tmp_counter5;

int __tmp_counter3;
EXPORT_SYMBOL_GPL(__tmp_counter3);
int __tmp_counter;
EXPORT_SYMBOL_GPL(__tmp_counter);


// runs on the IMEE core
void imee_trace_cr3 (void)
{
    apic->write (APIC_EOI, 0);
    // if (imee_up) do_switch = 1;
    __tmp_counter1 ++;
}

asmlinkage void imee_int_handler (void);
asm ("  .text");
asm ("  .type   imee_int_handler, @function");
asm ("imee_int_handler: \n");
asm ("cli \n");
asm ("pushq %rax \n");
asm ("pushq %rbx \n");
asm ("pushq %rcx \n");
asm ("pushq %rdx \n");
asm ("pushq %rsi \n");
asm ("pushq %rdi \n");
asm ("pushq %rbp \n");
asm ("pushq %r8 \n");
asm ("pushq %r9 \n");
asm ("pushq %r10 \n");
asm ("pushq %r11 \n");
asm ("pushq %r12 \n");
asm ("pushq %r13 \n");
asm ("pushq %r14 \n");
asm ("pushq %r15 \n");
asm ("callq imee_trace_cr3 \n");
asm ("popq %r15 \n");
asm ("popq %r14 \n");
asm ("popq %r13 \n");
asm ("popq %r12 \n");
asm ("popq %r11 \n");
asm ("popq %r10 \n");
asm ("popq %r9 \n");
asm ("popq %r8 \n");
asm ("popq %rbp \n");
asm ("popq %rdi \n");
asm ("popq %rsi \n");
asm ("popq %rdx \n");
asm ("popq %rcx \n");
asm ("popq %rbx \n");
asm ("popq %rax \n");
asm ("sti \n");
asm ("iretq");

// runs on the guest core
unsigned long long intt;
void imee_write_eoi (void)
{
    u32 this_cr3 = 0;

    apic->write (APIC_EOI, 0);
    intt = rdtsc ();
    kvm_x86_ops->read_cr3 (&this_cr3);

    kvm_x86_ops->read_rsp_rip ((u32*) &last_rsp, (u32*) &last_rip);

    /* this is still ugly */
    if (!last_cr3)
    {
        kvm_x86_ops->get_seg_sec (current_target->target_vcpu, &imee_tr, VCPU_SREG_TR);
        kvm_x86_ops->get_idt (current_target->target_vcpu, &imee_idt);
        kvm_x86_ops->get_gdt (current_target->target_vcpu, &imee_gdt);
    }

    DBG ("CPU: %d this_cr3: %X last_cr3: %X\n", smp_processor_id (), this_cr3, last_cr3);

    if (last_cr3 != this_cr3)
    {
        last_cr3 = this_cr3;
        exit_flg ++;
    }

    // kvm_x86_ops->intercept_cr3 (target_kvm->bsp_vcpu);
    
    // we don't need to acquire the spinlock because this function runs in the 
    // interrupt context with interrupt disabled.
    if (ACCESS_ONCE (go_flg) == 0 || ACCESS_ONCE (go_flg) == 3)
    {
        // To temporarily disable CR3 interception, comment out this line:
        // ACCESS_ONCE(go_flg) = 1;
    }

    smp_wmb ();
}

asmlinkage void imee_guest_int (void);
asm ("  .text");
asm ("  .type   imee_guest_int, @function");
asm ("imee_guest_int: \n");
asm ("cli \n");
asm ("pushq %rax \n");
asm ("pushq %rbx \n");
asm ("pushq %rcx \n");
asm ("pushq %rdx \n");
asm ("pushq %rsi \n");
asm ("pushq %rdi \n");
asm ("pushq %rbp \n");
asm ("pushq %r8 \n");
asm ("pushq %r9 \n");
asm ("pushq %r10 \n");
asm ("pushq %r11 \n");
asm ("pushq %r12 \n");
asm ("pushq %r13 \n");
asm ("pushq %r14 \n");
asm ("pushq %r15 \n");
asm ("call imee_write_eoi \n");
asm ("popq %r15 \n");
asm ("popq %r14 \n");
asm ("popq %r13 \n");
asm ("popq %r12 \n");
asm ("popq %r11 \n");
asm ("popq %r10 \n");
asm ("popq %r9 \n");
asm ("popq %r8 \n");
asm ("popq %rbp \n");
asm ("popq %rdi \n");
asm ("popq %rsi \n");
asm ("popq %rdx \n");
asm ("popq %rcx \n");
asm ("popq %rbx \n");
asm ("popq %rax \n");
asm ("sti \n");
asm ("iretq");

struct kvm_vcpu* pick_cpu (struct kvm* target_kvm)
{
    // TODO: randomly pick a cpu?
    return target_kvm->vcpus[0];
}
EXPORT_SYMBOL_GPL(pick_cpu);

#define T_CODE 1
#define T_DATA 2
#define T_NORM 3
#define PAGESIZE 0x1000
#define PTE_P_BIT           0x1
#define PTE_RW_BIT          0x2
#define PDE_PG_BIT          (0x1 << 7)

static pte_t* get_pte (struct task_struct *tsk, unsigned long addr)
{
    pgd_t* pgd;
    pud_t* pud;
    pmd_t* pmd;
    pte_t* pte;

    struct mm_struct* mm = tsk->mm;

    pgd = pgd_offset (mm, addr);
    if (pgd_none (*pgd) || pgd_bad (*pgd)) return 0;

    pud = pud_offset (pgd,addr);
    if (pud_none (*pud) || pud_bad (*pud)) return 0;

    pmd = pmd_offset (pud, addr);
    if (pmd_none (*pmd) || pmd_bad (*pmd)) return 0;

    pte = pte_offset_map (pmd, addr);
    if (pte_none(*pte))
    {
        pte_unmap (pte);
        return 0;
    }

    return pte;
}

// takes a guest physical address and return a pointer to that page
ulong get_ptr_guest_page (struct task_struct* target_proc, struct kvm* target_kvm, gpa_t gpa)
{
    struct kvm_arch *arch = &target_kvm->arch;
    struct kvm_mmu_page *page;

    list_for_each_entry(page, &arch->active_mmu_pages, link)
    {
        if (page->gfn == ((gpa >> 12) & ~0x1FFUL) && page->role.level == 1)
        {
            u64* p = page->spt;
            int idx = (gpa >> 12) & 0x1FFUL;
            ulong r = (ulong) (p[idx] & ~EPT_MASK);
            DBG ("r: %lX gpa: %lX\n", r, gpa);
            return r;
        }
    }
    // DBG ("mapping not found for gpa: %lX\n", gpa);
    return 0;

    /*
    unsigned long hva = gfn_to_hva (target_kvm, gpa_to_gfn(gpa));
    pte_t* ptep = get_pte (target_proc, hva); //TODO: what if that page is paged out / not mapped?
    if (!ptep)
    {
        // DBG ("qemu pte not found, gpa: %lX hva: %lX\n", gpa, hva);
        return NULL;
    }
    ulong pte = pte_val(*ptep);
    pte_unmap (ptep);

    DBG ("got pte: %lX for gpa: %lX hva: %lX\n", pte, gpa, hva);

    if (pte & PTE_P_BIT)
    {
        ulong hpa = pte & ~GPA_MASK;
        return hpa;
    }
    else
    {
        DBG ("qemu pte p_bit not set, pte: %lX\n", pte);
        return NULL;
    }
    */
}
EXPORT_SYMBOL_GPL(get_ptr_guest_page);

// TODO: assuming 32bit guest, change it to be more generic
static struct pt_mapping* get_guest_pte (struct task_struct* target_proc, struct kvm* target_kvm, u32 cr3, gva_t gva)
{
    int idx[4] = {
        (gva >> 22) & 0x3FF,
        (gva >> 12) & 0x3FF
    };
    int page_level = 2;

    struct pt_mapping* pm = 0; 

    int lv = 0;
    u32 next, next_addr;
    next = cr3;
    next_addr = cr3 & ~0xFFFU;

    // DBG ("gva: %lX\n", gva);

    for ( ; lv < page_level; lv++)
    {
        ulong hpa = get_ptr_guest_page (target_proc, target_kvm, next_addr);
        if (hpa)
        {
            ulong pfn = hpa >> 12;
            struct page* pg = pfn_to_page (pfn);
            u32* pp = (u32*) kmap_atomic (pg);
            // DBG ("ptr to guest page: %p\n", p);
            next = pp[idx[lv]];
            DBG ("lv: %d next: %lX\n", lv, next);
            kunmap_atomic (pp);

            if (!next || !(next & PTE_P_BIT)) 
                break;

            if (next && (next & PDE_PG_BIT) && (next & PTE_P_BIT)) // this is a huge page
            {
                pm = kmalloc (sizeof (struct pt_mapping), GFP_KERNEL);
                pm->lv = page_level - lv;
                pm->e = next;
                return pm;
            }
            next_addr = next & ~GPA_MASK;
            // DBG ("lv: %d, next_addr: %lX\n", lv, next_addr);
        }
        else
        {
            break;
        }
    }
    
    if (lv == page_level)
    {
        pm = kmalloc (sizeof (struct pt_mapping), GFP_KERNEL);
        pm->lv = page_level - lv + 1;
        pm->e = next;
        return pm;
    }
    else
    {
        return 0;
    }
}

u64* get_epte (intro_ctx_t* ctx, gpa_t gpa)
{
    struct kvm_mmu_page* cur;
    gpa_t needle = (gpa >> 12);
    int idx = (gpa >> 12) & 0x1FFUL;
    list_for_each_entry (cur, &ctx->leaf_page, link)
    {
        if (cur->gfn == (needle & ~0x1FFUL))
        {
            // DBG ("Found epte: %lX\n", cur->spt[idx]);
            return &cur->spt[idx];
        }
    }

    // DBG ("epte not found: %lX\n", gpa);
    return 0;
}
EXPORT_SYMBOL_GPL(get_epte);

void* alloc_non_leaf_page (struct list_head* non_leaf_page, int lv);
void* alloc_leaf_page (struct list_head* leaf_page, gpa_t gpa);

u64* map_epte (intro_ctx_t* ctx, gpa_t gpa, ulong new_hpa, ulong perm)
{
    u64* r = 0;
    u64* root = __va (ctx->eptptr);
    int idx[4] = {
        (gpa >> 39) & 0x1FF,
        (gpa >> 30) & 0x1FF,
        (gpa >> 21) & 0x1FF,
        (gpa >> 12) & 0x1FF
    };

    int page_level = 4;
    int i = 0;
    u64* table = root;
    u64 entry;
    for ( ; i < page_level; i ++)
    {
        DBG ("lv: %d table: %p\n", i, table);
        entry = table[idx[i]];
        if (!entry)
        {
            u64* tbl;
            if (i < page_level - 2)
            {
                DBG ("allocating new non-leaf page for gpa: %lX\n", gpa);
                tbl = (u64*) alloc_non_leaf_page (&ctx->non_leaf_page, page_level - i);
            }
            else if (i < page_level - 1) // i == page_level - 2
            {
                DBG ("allocating new leaf page for gpa: %lX\n", gpa);
                tbl = (u64*) alloc_leaf_page (&ctx->leaf_page, gpa);
            }
            else // i == page_level - 1
            {
                // we are at leaf now
                // set the PTE, at last!
                u64 e = (new_hpa & ~EPT_MASK) | (perm & 0xFFFU) | 0x270; // 0x270: hard-coded memory type and stuff
                DBG ("new EPTE: %llX\n", e);
                table[idx[i]] = e;
                r = &table[idx[i]];
                break;
            }
            table[idx[i]] = __pa (tbl) | 0x7;
            table = tbl;
        }
        else
        {
            if (i < page_level - 1)
            {
                table = __va (entry & ~EPT_MASK);
            }
            else
            {
                table[idx[i]] = (new_hpa & ~EPT_MASK) | (perm & 0xFFFU);
                r = &table[idx[i]];
                break;
            }
        }
    }

    return r;
}

/*
gfn_t hva_to_gfn (hva_t hva)
{
    int i;
	struct kvm_memslots *slots;
    gfn_t gfn = 0;

	slots = kvm_memslots((struct kvm*) target_kvm);

	for (i = 0; i < slots->nmemslots; i++) {
		struct kvm_memory_slot *memslot = &slots->memslots[i];
		unsigned long start = memslot->userspace_addr;
		unsigned long end;

		end = start + (memslot->npages << PAGE_SHIFT);

		if (hva >= start && hva < end) {
			gfn_t gfn_offset = (hva - start) >> PAGE_SHIFT;
			gfn = memslot->base_gfn + gfn_offset;
        }
    }

    return gfn;
    // return NULL;
}

void invalidate_imee_ept (hva_t hva)
{
    u64* epte;
    gpa_t gpa = (hva_to_gfn (hva)) << 12;
    if (gpa == code_hpa || gpa == data_hpa)
        return;
    if (gpa && (epte = get_epte (gpa)))
    {
        // DBG ("gpa: %lX\n", gpa);
        *epte = 0;
    }
}

void change_imee_ept (ulong hva, pte_t pte)
{
    u32 ptev = pte_val(pte);
    // DBG ("ptev: %X\n", ptev);
    ulong new_hpa = ptev & ~0xFFF;
    gpa_t gpa = (hva_to_gfn (hva)) << 12;
    DBG ("gpa: %llX new_hpa: %lX\n", gpa, new_hpa);
    if (gpa == code_hpa || gpa == data_hpa)
        return;
    if (gpa)
        map_epte (gpa, new_hpa);
}
*/

/*
static void patch_table (ulong dest, ulong len, int patch)
{
    ulong* buf = (ulong*) kmalloc (len * sizeof (ulong*), GFP_KERNEL);
    int i;

    copy_from_user ((void*) buf, dest, len * sizeof (ulong*));
    for (i = 0; i < len; i ++)
    {
        DBG ("before patch: %lX\n", buf[i]);
        buf[i] += patch;
        DBG ("after patch: %lX\n", buf[i]);
    }
    copy_to_user (dest, (void*) buf, len * sizeof (ulong*));

    kfree (buf);
}

void patch_got (ulong got, ulong got_len, int got_patch, ulong plt, ulong plt_len, int plt_patch)
{
    DBG ("got_patch: %X\n", got_patch);
    DBG ("gotplt_patch: %X\n", plt_patch);
    DBG ("got: %lX\n", got);
    DBG ("gotplt: %lX\n", plt);

    patch_table (got, got_len, got_patch);
    patch_table (plt, plt_len, plt_patch);
}
*/

/*
static int fix_lib_normal (struct task_struct* target_proc, struct kvm* target_kvm, ulong cr3, ulong code, ulong num_x_page, ulong code_host, ulong data, ulong num_w_page, ulong data_host, ulong got, ulong got_len, ulong gotplt, ulong gotplt_len)
{
    int i;
    // printk ("code: %lX data: %lX code_host: %lX data_host: %lX\n", code, data, code_host, data_host);
    for (i = 0; i < num_x_page; i ++)
    {
        ulong addr = code + i * PAGE_SIZE;
        ulong pte = // get_guest_pte (target_proc, target_kvm, cr3, addr);
        if (!pte)
        {
            ERR ("Cannot find guest code mapping: %lX cr3: %lX.\n", addr, cr3);
            return -1;
        }
        // DBG ("pte: %lX\n", pte);

        ulong h_addr = code_host + i * PAGE_SIZE;
        pte_t* ptep = get_pte (current, h_addr);
        if (!ptep)
        {
            ERR ("Cannot find host mapping, but host memory should already be pinned: %lX\n", h_addr);
            return -1;
        }
        ulong ptev = pte_val(*ptep);
        pte_unmap (ptep);
        ulong code_phys = ptev & ~HPA_MASK;

        // there is only one page, so this HACK works
        code_hpa = code_phys;

        u64* epte = get_epte (pte & ~GPA_MASK);
        if (epte)
        {
            code_ept_pte_p = epte;
            code_ept_pte = *epte;
            *epte = ((*epte) & EPT_MASK) | ((u64) code_phys) | 0x4; // 0x4: Exec bit
            // printk ("replaced code epte: %llX at: %p with: %llX\n", code_ept_pte, code_ept_pte_p, *epte);
        }
        else
        {
            code_ept_pte_p = map_epte (pte & ~GPA_MASK, code_phys | 0x4); // hack, setting Exec bit here
            code_ept_pte = 0;
            // printk ("remapped code epte at: %p\n", code_ept_pte_p);
        }
    }

    for (i = 0; i < num_w_page; i ++)
    {
        ulong addr = data + i * PAGE_SIZE;
        ulong pte = // get_guest_pte (target_proc, target_kvm, cr3, addr);
        if (!pte)
        {
            ERR ("Cannot find guest data mapping: %lX cr3: %lX\n", addr, cr3);
            return -1;
        }
        // DBG ("pte: %lX\n", pte);

        ulong h_addr = data_host + i * PAGE_SIZE;
        pte_t* ptep = get_pte (current, h_addr);
        if (!ptep)
        {
            ERR ("Cannot find host data mapping, but host memory should already be pinned: %lX\n", h_addr);
            return -1;
        }
        ulong ptev = pte_val(*ptep);
        pte_unmap (ptep);
        ulong data_phys = ptev & ~HPA_MASK;

        // there is only one page, so this HACK works
        data_hpa = data_phys;

        u64* epte = get_epte (pte & ~GPA_MASK);
        if (epte)
        {
            data_ept_pte_p = epte;
            data_ept_pte = *epte;
            *epte = ((*epte) & EPT_MASK) | ((u64) data_phys) | 0x3;
            // printk ("replaced data epte: %llX at: %p with: %llX\n", data_ept_pte, data_ept_pte_p, *epte);
        }
        else
        {
            data_ept_pte_p = map_epte (pte & ~GPA_MASK, data_phys | 0x3); //hack
            data_ept_pte = 0;
            // printk ("remapped data epte at: %p\n", data_ept_pte_p);
        }
    }

    //
    // int got_patch = data - data_host;
    // int gotplt_patch = code - code_host;

    // patch_got (got, got_len, got_patch, gotplt, gotplt_len, gotplt_patch);
    //

    return 0;
}
*/

/*
void fix_lib_scattered_page ()
{
}

static int fix_lib_regions (struct task_struct* target_proc, struct kvm* target_kvm, ulong cr3, ulong code, ulong num_x_page, ulong code_host, ulong data, ulong num_w_page, ulong data_host, ulong offset, ulong thunk, ulong got, ulong got_len, ulong gotplt, ulong gotplt_len)
{
    DBG ("code: %lX data: %lX code_host: %lX data_host: %lX offset: %lX thunk: %lX\n", 
            code, data, code_host, data_host, offset, thunk);
    u32 new_offset = data - code - num_x_page * 0x1000;
    int got_base_patch = new_offset - offset;
    unsigned char add_ebx_ret[10] = {
        0x8b, 0x1c, 0x24,               // mov (%esp), %ebx
        0x81, 0xc3, 0x0, 0x0, 0x0, 0x0, // add $0, %ebx
        0xc3                            // ret
    };
    unsigned char* p = &got_base_patch;
    DBG ("patch: %X\n", got_base_patch);

    // intel uses little endian
    int i;
    for (i = 3; i >= 0; i--)
    {
        add_ebx_ret[i + 5] = p[i];
    }

    // patch __i686.get_pc_thunk.bx
   
    copy_to_user ((void*) thunk, add_ebx_ret, 10);

    for (i = 0; i < 10; i ++)
    {
        printk ("%X ", add_ebx_ret[i]);
    }
    DBG ("\n");

    return fix_lib_normal (target_proc, target_kvm, cr3, code, num_x_page, code_host, data, num_w_page, data_host, got, got_len, gotplt, gotplt_len);
}
*/

static void adjust_imee_vcpu (ulong rip, ulong data)
{
    DBG ("rip: %lX\n", rip);
    imee_vcpu->arch.regs[VCPU_REGS_RIP] = rip; 
    // imee_vcpu->arch.regs_dirty |= 0xFFFFFFFF; // VCPU_REGS_RIP bit
    __set_bit (VCPU_REGS_RIP, (unsigned long*)&imee_vcpu->arch.regs_dirty); // VCPU_REGS_RIP bit
    imee_vcpu->arch.regs[VCPU_REGS_RCX] = 0xDEADBEEF;
    // imee_vcpu->arch.regs[VCPU_REGS_RDX] = data; 
    imee_vcpu->arch.regs[VCPU_REGS_RSP] = data + 0xFF0;
}

/*
static int walk_gpt (struct task_struct* tsk, struct kvm* target_kvm, struct arg_blk* args) 
{
    ulong hpa = get_ptr_guest_page (target_proc, target_kvm, last_cr3);
    int code_flg, data_flg;
    struct region code_region = {0, 0, T_CODE};
    struct region data_region = {0, 0, T_DATA};

    code_flg = data_flg = 0;

    ulong num_x_page = args->num_x_page;
    ulong num_w_page = args->num_w_page;
    ulong offset = args->offset;
    ulong code_host = args->code_host;
    ulong data_host = args->data_host;
    ulong tgt = args->thunk;
    ulong got = args->got;
    ulong got_len = args->got_len;
    ulong gotplt = args->gotplt;
    ulong gotplt_len = args->gotplt_len;
    code_entry = args->entry;

    if (hpa)
    {
        ulong pfn = hpa >> 12;
        struct page* pg = pfn_to_page (pfn);
        u32* pp = (u32*) kmap_atomic (pg);
        int i = 0;
        for (; i < 768; i ++)
        {
            if (pp[i])
            {
                ulong pt_hpa = get_ptr_guest_page (target_proc, target_kvm, pp[i] & ~GPA_MASK);
                struct page* pt_pg = pfn_to_page (pt_hpa >> 12);
                u32* pt_pp = (u32*) kmap_atomic (pt_pg);

                int j = 0;
                for (; j < 1024; j ++)
                {
                    if (pt_pp[j])
                    {
                        // DBG ("pte: %X\n", pt_pp[j]);
                        if (!code_flg && (pt_pp[j] & PTE_P_BIT) && !(pt_pp[j] & PTE_RW_BIT))
                        {
                            code_region.start = (i << 22 | j << 12);
                            code_region.end = code_region.start;
                            // DBG ("added code region\n");
                            code_flg = 1;
                        }
                        if (!data_flg && (pt_pp[j] & PTE_P_BIT) && (pt_pp[j] & PTE_RW_BIT))
                        {

                            data_region.start = (i << 22 | j << 12);
                            data_region.end = data_region.start;
                            // DBG ("added data region\n");
                            data_flg = 1;
                        }

                        if (code_flg && data_flg)
                        {
                            kunmap_atomic (pt_pp);
                            kunmap_atomic (pp);
                            goto found;
                        }
                    }
                }
                kunmap_atomic (pt_pp);
            }
        }
        kunmap_atomic (pp);
    }

found:

    // DBG ("code region %lX\n", code_region.start);
    // DBG ("data region %lX\n", data_region.start);
    adjust_imee_vcpu (args->entry + code_region.start - code_host, data_region.start);
    return fix_lib_normal (target_proc, target_kvm, last_cr3, code_region.start, num_x_page, code_host, data_region.start, num_w_page, data_host, got, got_len, gotplt, gotplt_len);
}
*/

/*
static ulong make_gpa (struct pt_mapping* pm, u32 gva)
{
    switch (pm->lv)
    {
        case 1:
            return pm->e & ~(0xFFFU);
        case 2:
            return (pm->e & (0xFFC00000U)) | (gva & 0x3FF000);
        default:
            ERR ("paging structure level at %d not implemented", pm->lv);
            return 0;
    }

}
*/

static int remap_gpa_code (intro_ctx_t* ctx, ulong gpa)
{
    ulong code_phys = code_hpa;

    // ulong gpa = make_gpa(pm, last_rip);
    u64* epte = get_epte (ctx, gpa);

    // restore what we changed last time
    if (ctx->code_ept_pte_p)
    {
        *ctx->code_ept_pte_p = ctx->code_ept_pte;
    }

    if (epte)
    {
        ctx->code_ept_pte_p = epte;
        ctx->code_ept_pte = *epte;
        *epte = ((*epte) & EPT_MASK) | ((u64) code_phys) | 0x4; // 0x4: Exec bit
        DBG ("replaced code epte: %llX at: %p with: %llX\n", ctx->code_ept_pte, ctx->code_ept_pte_p, *epte);
    }
    else
    {
        ctx->code_ept_pte_p = map_epte (ctx, gpa, code_phys, 0x4);
        ctx->code_ept_pte = 0;
        DBG ("remapped code epte at: %p\n", ctx->code_ept_pte_p);
    }

    // kfree (pm);
    return 0;
}

/*
static int remap_gpa_data ()
{
    ulong addr = last_rsp;
    struct pt_mapping* pm = get_guest_pte (target_proc, (struct kvm*) target_kvm, last_cr3, addr);
    if (!pm)
    {
        ERR ("Cannot find guest data mapping: %lX cr3: %lX\n", addr, last_cr3);
        return 1;
    }

    ulong h_addr = imee_arg.data_host;
    pte_t* ptep = get_pte (current, h_addr);
    if (!ptep)
    {
        ERR ("Cannot find host data mapping, but host memory should already be pinned: %lX\n", h_addr);
        return 1;
    }
    ulong ptev = pte_val(*ptep);
    pte_unmap (ptep);
    ulong data_phys = ptev & ~HPA_MASK;

    // there is only one page, so this HACK works
    data_hpa = data_phys;

    ulong gpa = make_gpa (pm, last_rsp);
    u64* epte = get_epte (gpa);
    if (epte)
    {
        data_ept_pte_p = epte;
        data_ept_pte = *epte;
        *epte = ((*epte) & EPT_MASK) | ((u64) data_phys) | 0x3;
        DBG ("replaced data epte: %llX at: %p with: %llX\n", data_ept_pte, data_ept_pte_p, *epte);
    }
    else
    {
        data_ept_pte_p = map_epte (gpa, data_phys, 0x3);
        data_ept_pte = 0;
        DBG ("remapped data epte at: %p\n", data_ept_pte_p);
    }
    
    kfree(pm);
    return 0;
}
*/

int remap_gpa (intro_ctx_t* ctx, ulong gpa)
{
    if (remap_gpa_code (ctx, gpa))
        return -1;
    // if (remap_gpa_data ())
    //     return -1;

    // lastly
    // adjust_imee_vcpu ((last_rip & ~(0xFFFU)) | (imee_arg.entry & 0xFFFU), last_rsp & ~(0xFFFU));
    // adjust_imee_vcpu (0x40000000 + 0x1000 | (imee_arg.entry & 0xFFFU), 0x40000000);
    adjust_imee_vcpu ((last_rip & ~0xFFFU) | (imee_arg.entry & 0xFFFU), 0);

    DBG ("set! last_cr3: %lX last_rip: %lX\n", last_cr3, last_rip);
    return 0;
}
EXPORT_SYMBOL_GPL(remap_gpa);

static void* do_alloc_ept_frames (void* base)
{
    base = (void*) __get_free_pages (GFP_KERNEL, PAGE_ORDER);
    return base;
}

void init_ept_frames (void)
{
    if (!p_base)
    {
        p_idx = 0;
        p_base_idx = 0;
        p_base = do_alloc_ept_frames (p_bases[p_base_idx]);
    }
}
EXPORT_SYMBOL_GPL(init_ept_frames);

static void release_ept_frames (void)
{
    int i = 0;
    for (; i <= p_base_idx; i ++)
    {
        free_pages ((ulong) p_bases[i], PAGE_ORDER);
        p_bases[i] = 0;
    }

    p_base_idx = 0;
    p_base = 0;
    p_idx = 0;
}

static ulong* get_ept_page (void)
{
    if (p_base)
    {
        p_idx ++;
        if (p_idx < (1 << PAGE_ORDER))
        {
            int i;
            ulong* p = (ulong*) (((ulong) p_base) + p_idx * PAGE_SIZE);
            for (i = 0; i < PAGE_SIZE / sizeof (ulong); i ++)
            {
                p[i] = 0;
            }
            return p;
        }
        else
        {
            p_base_idx ++;
            if (p_base_idx < NBASE)
            {
                p_base = do_alloc_ept_frames (p_bases[p_base_idx]);
                p_idx = 0;
                return (ulong*) p_base;
            }
            else
            {
                printk (KERN_ERR "EPT frames have been used up, p_base_idx: %d p_idx: %d\n", p_base_idx, p_idx);
                return 0;
            }
        }
    }
    else
    {
        printk (KERN_ERR "EPT frames have not been allocated.");
        return 0;
    }
}

void* alloc_non_leaf_page (struct list_head* non_leaf_page, int lv)
{
    struct kvm_mmu_page* temp_page = kmalloc (sizeof (struct kvm_mmu_page), GFP_KERNEL);
    INIT_LIST_HEAD(&temp_page->link);
    void* page = get_ept_page();
    temp_page->spt = page;
    temp_page->role.level = lv;
    list_add (&temp_page->link, non_leaf_page);
    return page;
}

void* alloc_leaf_page (struct list_head* leaf_page, gpa_t gpa)
{
    struct kvm_mmu_page* temp_page = kmalloc (sizeof (struct kvm_mmu_page), GFP_KERNEL);
    INIT_LIST_HEAD(&temp_page->link);
    void* page = get_ept_page();
    temp_page->spt = page;
    temp_page->role.level = 1;
    temp_page->gfn = (gpa >> 12) & ~0x1FFUL; 
    list_add (&temp_page->link, leaf_page);
    return page;
}


u64 make_imee_ept (struct list_head* leaf_page, struct list_head* non_leaf_page)
{
    struct kvm_mmu_page* root_page = kmalloc (sizeof (struct kvm_mmu_page), GFP_KERNEL);
    root_page->spt = (u64*) get_ept_page ();
    root_page->role.level = 4;
    INIT_LIST_HEAD (&root_page->link);
    list_add (&root_page->link, non_leaf_page);

    u64* root = root_page->spt;
    struct kvm_mmu_page* cur;
    list_for_each_entry (cur, leaf_page, link)
    {
        // DBG ("building higher level pages for GFN: %llX\n", cur->gfn);
        int pml4_ind = ((cur->gfn) >> 27) & 0x1FF;
        int pdpt_ind = ((cur->gfn) >> 18) & 0x1FF;
        int pd_ind = ((cur->gfn) >> 9) & 0x1FF;
        // DBG ("pml4_ind: %X\n", pml4_ind);
        // DBG ("pdpt_ind: %X\n", pdpt_ind);
        // DBG ("pd_ind: %X\n", pd_ind);

        u64 *pdpt, *pd;
        if (root[pml4_ind] == 0)
        {
            pdpt = (u64*) alloc_non_leaf_page (non_leaf_page, 3);
            root[pml4_ind] = __pa (pdpt) | 0x7;
            // DBG ("added root[pml4_ind]: %llX\n", root[pml4_ind]);
        }
        else
        {
            pdpt = __va (root[pml4_ind] & ~EPT_MASK);
            // DBG ("found pdpt: %llX\n", pdpt);
        }

        if (pdpt[pdpt_ind] == 0)
        {
            pd = (u64*) alloc_non_leaf_page (non_leaf_page, 2);
            pdpt[pdpt_ind] = __pa (pd) | 0x7;
            // DBG ("added pdpt[pdpt_ind]: %llX\n", pdpt[pdpt_ind]);
        }
        else
        {
            pd = __va (pdpt[pdpt_ind] & ~EPT_MASK);
            // DBG ("found pd: %llX\n", pd);
        }

        if (pd[pd_ind] == 0)
        {
            pd[pd_ind] = __pa (cur->spt) | 0x7;
        }
    }

    list_for_each_entry (cur, non_leaf_page, link)
    {
        // DBG ("new non-leaf page at: %p\n", cur->spt);
    }

    return (u64) __pa (root);
}
EXPORT_SYMBOL_GPL(make_imee_ept);

static void cr0_wp_off (void)
{
    u64 cr0;
    asm ("movq %%cr0, %0;":"=r"(cr0)::);
    // printk ("%llX\n", cr0);
    cr0 &= ~0x10000;
    // printk ("%llX\n", cr0);
    asm ("movq %0, %%cr0;"::"r"(cr0):);

}

static void cr0_wp_on (void)
{
    u64 cr0;
    asm ("movq %%cr0, %0;":"=r"(cr0)::);
    // printk ("%llX\n", cr0);
    cr0 |= 0x10000;
    // printk ("%llX\n", cr0);
    asm ("movq %0, %%cr0;"::"r"(cr0):);

}

static void install_int_handlers (void)
{
    unsigned char idtr[10];
    u64* idt;
    gate_desc s;

    asm ("sidt %0":"=m"(idtr)::);

    idt = (u64*)(*(u64*)(idtr + 2));
    DBG ("idt: %p\n", idt);

    cr0_wp_off ();
    pack_gate(&s, GATE_INTERRUPT, (unsigned long) imee_int_handler, 0, 0, __KERNEL_CS);

    idt[0x55 * 2] = * ((u64*) (&s));
    idt[0x55 * 2 + 1] = 0x00000000FFFFFFFFULL;

    pack_gate(&s, GATE_INTERRUPT, (unsigned long) imee_guest_int, 0, 0, __KERNEL_CS);

    idt[0x56 * 2] = * ((u64*) (&s));
    idt[0x56 * 2 + 1] = 0x00000000FFFFFFFFULL;

    cr0_wp_on ();

    DBG ("imee_int_handler: %p\n", imee_int_handler);
}

static void remove_int_handlers (void)
{
}

int get_next_ctx (intro_ctx_t** next)
{
    intro_ctx_t* cur = 0;

    list_for_each_entry (cur, &introspection_contexts, node)
    {
        if (cur->visited == 0)
        {
            cur->visited ++;
            *next = cur;

            current_target = cur;

            DBG ("picked VM: target_vm_pid: %d process: %s\n", 
                    cur->task->pid, cur->task->comm);

            return 0;
        }
    }

    // no more to scan
    return -1;
}
EXPORT_SYMBOL_GPL(get_next_ctx);

static void free_ept (struct list_head* leaf_page, struct list_head* non_leaf_page)
{
    struct kvm_mmu_page *cur, *n;

    list_for_each_entry_safe (cur, n, leaf_page, link)
    {
        // DBG ("releasing leaf page: %llX lv: %d\n", cur->gfn, cur->role.level);
        if (cur->gfn == ((last_cr3 >> 12) & ~0x1FF))
        {
            int i;
            for (i = 0; i < 512; i++)
            {
                // u64* p = cur->spt;
                // if (p[i])
                //     DBG ("\t i:%d -> %llX\n", i, p[i]);
            }
        }

        list_del (&cur->link);
        // free_page (cur->spt);
        kfree (cur);
    }

    list_for_each_entry_safe (cur, n, non_leaf_page, link)
    {
        // DBG ("releasing non-leaf page: %llX lv: %d\n", cur->gfn, cur->role.level);
        int i;
        for (i = 0; i < 512; i++)
        {
            // u64* p = cur->spt;
            // if (p[i])
            //     DBG ("\t i:%d -> %llX\n", i, p[i]);
        }

        list_del (&cur->link);
        // free_page (cur->spt);
        kfree (cur);
    }
    
}

void copy_leaf_ept (struct list_head* leaf_page, struct kvm_arch* arch)
{
    struct kvm_mmu_page *page;

    list_for_each_entry(page, &arch->active_mmu_pages, link)
    {
        // DBG ("level: %d gfn: %lX\n", page->role.level, page->gfn);
        // copy all leaf page
        if (page->role.level == 1)
        {
            void *newpage = (void*) get_ept_page ();
            struct kvm_mmu_page* new_pt_page = kmalloc (sizeof (struct kvm_mmu_page), GFP_KERNEL);
            new_pt_page->spt = newpage;
            new_pt_page->role.level = 1;
            new_pt_page->gfn = page->gfn;
            INIT_LIST_HEAD (&new_pt_page->link);
            list_add (&new_pt_page->link, leaf_page);
            u64 *pte = (u64*) newpage;
            int i = 0;
            for (; i < 512; i ++)
            {
                pte[i] = page->spt[i] & ~0x6;
            }
        }
    }
}
EXPORT_SYMBOL_GPL(copy_leaf_ept);

/* 32-bit guest only */
static int init_local_page_tables (void)
{
    pte_t* ptep = get_pte (current, imee_arg.data_host);
    if (!ptep)
    {
        ERR ("Cannot find data mapping, but host memory should already be pinned: %lX\n", imee_arg.data_host);
        return -1;
    }
    ulong ptev = pte_val(*ptep);
    pte_unmap (ptep);

    data_hpa = ptev & ~HPA_MASK;

    ptep = get_pte (current, imee_arg.code_host);
    if (!ptep)
    {
        ERR ("Cannot find data mapping, but host memory should already be pinned: %lX\n", imee_arg.code_host);
        return -1;
    }
    ptev = pte_val(*ptep);
    pte_unmap (ptep);
    code_hpa = ptev & ~HPA_MASK;

    code_entry = imee_arg.entry;

    u32* fake_cr3_pd = (u32*) get_zeroed_page(GFP_KERNEL);
    fake_cr3_pd_hpa = __pa (fake_cr3_pd);
    u32* fake_cr3_pt_exec = (u32*) get_zeroed_page(GFP_KERNEL);
    fake_cr3_pt_hpa_exec = __pa (fake_cr3_pt_exec);
    u32* fake_cr3_pt_data = (u32*) get_zeroed_page(GFP_KERNEL);
    fake_cr3_pt_hpa_data = __pa (fake_cr3_pt_data);

    // temp = fake_cr3_pt;

    // u32* fake_cr3_pd2 = (u32*) get_zeroed_page(GFP_KERNEL);
    // ulong fake_cr3_pd2_hpa = __pa (fake_cr3_pd2);
    // // fake_cr3_pd2 [256] = 0xF0001060U | 0x7;
    // map_epte (0xF0007000U, fake_cr3_pd2_hpa, 0x3);

    // fake_cr3_pd [772] = 0xF0001060U | 0x7;

    fake_cr3_pd [0] = PT_GPA_DATA | 0x7 | 0x60;
    int i = 0;
    for (i = 1; i < 1024; i ++)
    {
        fake_cr3_pd [i] = PT_GPA_EXEC | 0x7 | 0x60;
    }

    fake_cr3_pt_data [0] = DATA_GPA | 0x7 | 0x100 | 0x60; // 0x100 G bit, 0x20 A bit, 0x40 D bit
    fake_cr3_pt_exec [0] = CODE_GPA | 0x7 | 0x100 | 0x60; // 0x100 G bit, 0x20 A bit, 0x40 D bit
    for (i = 1; i < 1024; i ++)
    {
        fake_cr3_pt_data [i] = CODE_GPA | 0x7 | 0x100 | 0x60; // 0x100 G bit, 0x20 A bit, 0x40 D bit
        fake_cr3_pt_exec [i] = CODE_GPA | 0x7 | 0x100 | 0x60; // 0x100 G bit, 0x20 A bit, 0x40 D bit
    }
    
    return 0;
}

static void map_local_page_tables (intro_ctx_t* ctx)
{
    map_epte (ctx, PD_GPA, fake_cr3_pd_hpa, 0x3);
    map_epte (ctx, PT_GPA_EXEC, fake_cr3_pt_hpa_exec, 0x3);
    map_epte (ctx, PT_GPA_DATA, fake_cr3_pt_hpa_data, 0x3);

    map_epte (ctx, DATA_GPA, data_hpa, 0x3);
    map_epte (ctx, CODE_GPA, code_hpa, 0x4);
}

void setup_imee_vcpu_sregs (struct kvm_vcpu* guest_vcpu, struct kvm_vcpu* vcpu)
{
    struct kvm_sregs *imee_sregs = kmalloc (sizeof (struct kvm_sregs), GFP_KERNEL);
    imee_sregs->cr3 = 0xABABABAB;
    kvm_arch_vcpu_ioctl_get_sregs (guest_vcpu, imee_sregs);
    // kvm_arch_vcpu_ioctl_set_sregs (vcpu, imee_sregs);
    
    DBG ("guest_vcpu: %p imee_vcpu: %p\n", guest_vcpu, vcpu);

    // init CS register
    imee_sregs->cs.selector = 0x60;
    imee_sregs->cs.base = 0x0;
    imee_sregs->cs.limit = 0xFFFFFFFF;
    imee_sregs->cs.type = 0xB;
    imee_sregs->cs.s = 1;
    imee_sregs->cs.dpl = 0;
    imee_sregs->cs.present = 1;
    imee_sregs->cs.avl = 0;
    imee_sregs->cs.l = 0;
    imee_sregs->cs.db = 1;
    imee_sregs->cs.g = 1;

    // DS register
    imee_sregs->ds.selector = 0x7B;
    imee_sregs->ds.base = 0x0;
    imee_sregs->ds.limit = 0xFFFFFFFF;
    imee_sregs->ds.type = 0x3;
    imee_sregs->ds.s = 1;
    imee_sregs->ds.dpl = 3;
    imee_sregs->ds.present = 1;
    imee_sregs->ds.avl = 0;
    imee_sregs->ds.l = 0;
    imee_sregs->ds.db = 1;
    imee_sregs->ds.g = 1;

    // SS register
    imee_sregs->ss.selector = 0x68;
    imee_sregs->ss.base = 0x0;
    imee_sregs->ss.limit = 0xFFFFFFFF;
    imee_sregs->ss.type = 0x3;
    imee_sregs->ss.s = 1;
    imee_sregs->ss.dpl = 0;
    imee_sregs->ss.present = 1;
    imee_sregs->ss.avl = 0;
    imee_sregs->ss.l = 0;
    imee_sregs->ss.db = 1;
    imee_sregs->ss.g = 1;

    // GS register
    imee_sregs->gs.selector = 0x68;
    imee_sregs->gs.base = 0x0;
    imee_sregs->gs.limit = 0xFFFFFFFF;
    imee_sregs->gs.type = 0x3;
    imee_sregs->gs.s = 1;
    imee_sregs->gs.dpl = 0;
    imee_sregs->gs.present = 1;
    imee_sregs->gs.avl = 0;
    imee_sregs->gs.l = 0;
    imee_sregs->gs.db = 1;
    imee_sregs->gs.g = 1;

    DBG ("CS selector: %X base: %llX limit: %X\n", imee_sregs->cs.selector, imee_sregs->cs.base, imee_sregs->cs.limit);
    DBG ("DS selector: %X base: %llX limit: %X\n", imee_sregs->ds.selector, imee_sregs->ds.base, imee_sregs->ds.limit);
    DBG ("SS selector: %X base: %llX limit: %X\n", imee_sregs->ss.selector, imee_sregs->ss.base, imee_sregs->ss.limit);
    DBG ("ES selector: %X base: %llX limit: %X\n", imee_sregs->es.selector, imee_sregs->es.base, imee_sregs->es.limit);
    DBG ("FS selector: %X base: %llX limit: %X\n", imee_sregs->fs.selector, imee_sregs->fs.base, imee_sregs->fs.limit);
    DBG ("GS selector: %X base: %llX limit: %X\n", imee_sregs->gs.selector, imee_sregs->gs.base, imee_sregs->gs.limit);
    DBG ("IDT base: %llX limit: %X\n", imee_sregs->idt.base, imee_sregs->idt.limit);
    DBG ("GDT base: %llX limit: %X\n", imee_sregs->gdt.base, imee_sregs->gdt.limit);
    DBG ("TR  base: %llX limit: %X sel: %X\n", imee_sregs->tr.base, imee_sregs->tr.limit, imee_sregs->tr.selector);
    DBG ("CR0:%llX CR2: %llX, CR3: %llX, CR4: %llX\n", imee_sregs->cr0, imee_sregs->cr2, imee_sregs->cr3, imee_sregs->cr4);
    DBG ("CR8:%llX EFER: %X\n", imee_sregs->cr8, imee_sregs->efer);

    kvm_x86_ops->set_segment (vcpu, &imee_sregs->cs, VCPU_SREG_CS);
    kvm_x86_ops->set_segment (vcpu, &imee_sregs->ds, VCPU_SREG_DS);
    kvm_x86_ops->set_segment (vcpu, &imee_sregs->ss, VCPU_SREG_SS);
    kvm_x86_ops->set_segment (vcpu, &imee_sregs->ds, VCPU_SREG_ES);
    kvm_x86_ops->set_segment (vcpu, &imee_sregs->fs, VCPU_SREG_FS);
    kvm_x86_ops->set_segment (vcpu, &imee_sregs->gs, VCPU_SREG_GS);
    kvm_x86_ops->set_segment (vcpu, &imee_sregs->ldt, VCPU_SREG_LDTR);

    // kvm_x86_ops->set_rflags (vcpu, 0x2 | 0x100);
    kvm_x86_ops->set_rflags (vcpu, 0x2);

	// vcpu->arch.cr2 = imee_sregs->cr2;
	
	kvm_x86_ops->set_efer(vcpu, imee_sregs->efer);
	kvm_x86_ops->set_cr0(vcpu, (imee_sregs->cr0 | 0x2 ) & ~(0x4 | 0x8)); // set MP, clear TS and EM
	kvm_x86_ops->set_cr4(vcpu, imee_sregs->cr4 | 0x80 | 0x600); // set PGE bit, and OSFXSR, OSXMMEXCPT bits for SSE

    kfree (imee_sregs);
}

static void init_global_vars (struct kvm_vcpu* vcpu)
{
    __tmp_counter = 0;
    __tmp_counter1 = 0;
    __tmp_counter2 = 0;
    __tmp_counter3 = 0;
    __tmp_counter5 = 0;

    total_cycle = 0;
    cycle_idx = 0;
    imee_t = 0;

    // ts_buffer1 = (unsigned long long*) __get_free_pages (GFP_KERNEL, PAGE_ORDER);
    ts_buffer_idx1 = 0;
    ts_buffer_idx1_limit = (1 << PAGE_ORDER) * 0x1000 / sizeof (unsigned long long);

    imee_pid = current->pid;
    imee_vcpu = vcpu;
    exit_flg = 0;
}

intro_ctx_t* kvm_to_ctx (struct kvm* target)
{

    intro_ctx_t* cur;
    list_for_each_entry (cur, &introspection_contexts, node)
    {
        if (cur->kvm == target)
            return cur;
    }

    return NULL;
}
EXPORT_SYMBOL_GPL(kvm_to_ctx);

static void create_introspection_context (struct kvm* target)
{
    /* return if this VM is myself */
    if (target->mm->owner == current)
    {
        return;
    }

    if (kvm_to_ctx (target))
    {
        // already created
        return;
    }

    intro_ctx_t* ctx = (intro_ctx_t*) kmalloc (sizeof (intro_ctx_t), GFP_KERNEL);
    ctx->task = target->mm->owner;
    DBG ("pid: %d, process: %s, cpu: %d\n", 
            ctx->task->pid, ctx->task->comm, task_cpu (ctx->task));
    ctx->visited = 0;

    list_add (&ctx->node, &introspection_contexts);
    INIT_LIST_HEAD(&ctx->leaf_page);
    INIT_LIST_HEAD(&ctx->non_leaf_page);

    ctx->code_ept_pte_p = 0;
    ctx->code_ept_pte = 0;
    ctx->data_ept_pte_p = 0;
    ctx->data_ept_pte = 0;

    ctx->kvm = target;
    ctx->target_vcpu = pick_cpu ((struct kvm*) target);

    /* copy leaf EPTs */
    struct kvm_arch *arch = (struct kvm_arch*) &target->arch;

    spin_lock (&ctx->target_vcpu->kvm->mmu_lock);
    copy_leaf_ept (&ctx->leaf_page, arch);
    ctx->eptptr = make_imee_ept (&ctx->leaf_page, &ctx->non_leaf_page);
    spin_unlock (&ctx->target_vcpu->kvm->mmu_lock);

    /* prepare fixed mapping that's outside of the guest address space */
    map_local_page_tables (ctx);

    ctx->cr3 = PD_GPA;
}

static void free_contexts (void)
{
    intro_ctx_t* cur, *bck;
    list_for_each_entry_safe (cur, bck, &introspection_contexts, node)
    {
        free_ept (&cur->leaf_page, &cur->non_leaf_page);
        list_del (&cur->node);
        kfree (cur);
    }
}

int init_imee_vcpu (intro_ctx_t* next, struct kvm_vcpu* vcpu)
{
    DBG ("current->pid: %d parent->pid: %d imee_pid: %d\n", current->pid, current->parent->pid, imee_pid);

    /* setup rest of vCPU */
    setup_imee_vcpu_sregs (next->target_vcpu, vcpu);

    return 0;
}

void reset_general_regs (struct kvm_vcpu* vcpu)
{
    vcpu->arch.regs[VCPU_REGS_RAX] = 0;
    vcpu->arch.regs[VCPU_REGS_RBX] = 0;
    vcpu->arch.regs[VCPU_REGS_RCX] = 0;
    vcpu->arch.regs[VCPU_REGS_RDX] = 0;
    vcpu->arch.regs[VCPU_REGS_RSP] = 0;
    vcpu->arch.regs[VCPU_REGS_RBP] = 0;
    vcpu->arch.regs[VCPU_REGS_RSI] = 0;
    vcpu->arch.regs[VCPU_REGS_RDI] = 0;

    vcpu->arch.regs_dirty = 0xFFFFFFFFU;
    vcpu->arch.regs_avail = 0xFFFFFFFFU;
}

void switch_intro_ctx (intro_ctx_t* next, struct kvm_vcpu* vcpu)
{
    vcpu->arch.mmu.root_hpa = next->eptptr;
    kvm_x86_ops->write_eptp (vcpu);
}
EXPORT_SYMBOL_GPL(switch_intro_ctx);

long kvm_imee_get_guest_context (struct kvm_vcpu *vcpu, void* argp)
{
    printk ("================start==================\n");

    t0 = rdtsc ();

    init_global_vars (vcpu);

    // install the handlers to IDT
    install_int_handlers ();

    /* allocate page frames for EPT from the kernel */
    init_ept_frames();

    // struct arg_blk* args = &imee_arg;
    copy_from_user (&imee_arg, argp, sizeof (struct arg_blk));

    /* prepare the local page tables */
    init_local_page_tables ();

    
    /* init contexts */
    struct kvm* cur;
    spin_lock (&kvm_lock);
    list_for_each_entry (cur, &vm_list, vm_list)
    {
        create_introspection_context (cur);
    }
    spin_unlock (&kvm_lock);

    t1 = rdtsc ();
    setup_cycle = t1 - t0;

    return 0; 
}

int start_guest_intercept (struct kvm_vcpu *vcpu)
{
    cycle_idx = 0;
    // int r = -1;
    int r = 0;
    // ulong flags;
    int ii = 0;

    if (ts_buffer1 && ts_buffer_idx1 < ts_buffer_idx1_limit)
    {
        ts_buffer1 [ts_buffer_idx1] = rdtsc ();
        ts_buffer1 [ts_buffer_idx1 + 1] = 1;
        spin_lock (&sync_lock1);
        ts_buffer_idx1 += 2;
        spin_unlock (&sync_lock1);
    }

    last_cr3 = 0;

    spin_lock (&sync_lock);
    if (ACCESS_ONCE (go_flg) == 2)
    {
        printk ("WARNING: last scan ended without resetting CR3 scanning, skipping now.\n");
    }
    else if (ACCESS_ONCE (go_flg) == 1)
    {
        ACCESS_ONCE (go_flg) = 0;
    }
    else if (ACCESS_ONCE (go_flg) == 3)
    {
        ACCESS_ONCE (go_flg) = 2;
    }
    spin_unlock (&sync_lock);

    intro_ctx_t* next = 0;
    if (get_next_ctx (&next) == -1)
        return -1;

    switch_intro_ctx (next, vcpu);
    init_imee_vcpu (next, vcpu);
    reset_general_regs (vcpu);

    exit_flg = 1;
    smp_mb ();

    int cpu = next->target_vcpu->cpu;
    DBG ("Firing IPI to cpu: %d\n", cpu);
    apic->send_IPI_mask (cpumask_of (cpu), 0x56);

    // if (ts_buffer1 && ts_buffer_idx1 < ts_buffer_idx1_limit)
    // {
    //     ts_buffer1 [ts_buffer_idx1] = rdtsc ();
    //     ts_buffer1 [ts_buffer_idx1 + 1] = 1;
    //     spin_lock (&sync_lock1);
    //     ts_buffer_idx1 += 2;
    //     spin_unlock (&sync_lock1);
    // }

    smp_mb ();
    int j = 0;
    while (ACCESS_ONCE(exit_flg) == 1)
    {
        j ++;
        if (j > 10000000) 
        {
            ERR ("Waited for too long for exit_flg, last_cr3: %lX\n", last_cr3);
            return -1;
        }
    }

    // ii = remap_gpa ();

    // Old way of walking the guest PT  to find VA to reuse
    // int ii = walk_gpt (target_proc, target_kvm, &imee_arg);

    // trial_run = 0; // this causes violations
    trial_run = 1; // this causes violations
    DBG ("Acquired last_cr3: %X\n", last_cr3);
    DBG ("Acquired last_rip: %X\n", last_rip);
    DBG ("Acquired last_rsp: %X\n", last_rsp);

    // vcpu->arch.regs[VCPU_REGS_RBX] = last_cr3;
    // vcpu->arch.regs[VCPU_REGS_RBX] = 0xF0007000U;

    // // use PF handler inside IMEE
    // gate_desc s;
    // pack_gate(&s, GATE_INTERRUPT, (unsigned long)imee_arg.int_handler, 0, 0, imee_sregs->cs.selector);
    // copy_to_user (args->data_host + 0xFF8, &s, sizeof (gate_desc));
    // imee_idt.address = args->data_host + (0xFF8 - 8 * 14); // magic
    // // use PF handler inside IMEE

    kvm_x86_ops->set_segment (vcpu, &imee_tr, VCPU_SREG_TR);
    kvm_x86_ops->set_idt (vcpu, &imee_idt);
    kvm_x86_ops->set_gdt (vcpu, &imee_gdt);


    // DBG ("exit_flg: %d\n", exit_flg);
    imee_up = 1;

    if (ts_buffer1 && ts_buffer_idx1 < ts_buffer_idx1_limit)
    {
        ts_buffer1 [ts_buffer_idx1] = rdtsc ();
        ts_buffer1 [ts_buffer_idx1 + 1] = 1;
        spin_lock (&sync_lock1);
        ts_buffer_idx1 += 2;
        spin_unlock (&sync_lock1);
    }

    // return -1;
    return 0;
}
EXPORT_SYMBOL_GPL(start_guest_intercept);

int kvm_imee_stop (struct kvm_vcpu* vcpu)
{
    current_target = 0;

    DBG ("releasing IMEE.\n");

    free_contexts ();

    release_ept_frames ();

    ACCESS_ONCE(exit_flg) = 0;

    remove_int_handlers ();

    // smp_mb ();
    spin_lock (&sync_lock);
    if (ACCESS_ONCE (go_flg) == 2)
    {
        ACCESS_ONCE(go_flg) = 3;
    }

    ACCESS_ONCE(imee_pid) = 0;
    ACCESS_ONCE(imee_vcpu) = 0;
    spin_unlock (&sync_lock);

    smp_mb ();

    vcpu->arch.mmu.root_hpa = INVALID_PAGE;
    enable_notifier = 0;
    code_hpa = 0;
    data_hpa = 0;
    last_cr3 = 0;

    while (cycle_idx >= 0)
    {
        printk ("cycles: %d - %lld\n", cycle_idx, t[cycle_idx]);
        cycle_idx--;
    }

    printk ("__tmp_counter: %d\n", __tmp_counter);
    printk ("__tmp_counter3: %d\n", __tmp_counter3);
    DBG ("__tmp_counter4: %d\n", __tmp_counter4);
    DBG ("__tmp_counter5: %d\n", __tmp_counter5);
    printk ("__tmp_counter2: %d\n", __tmp_counter2);
    DBG ("last_cr3: %lX\n", last_cr3);
    DBG ("go_flg: %lX\n", go_flg);
    printk ("total_cycle: %lld\n", total_cycle);
    printk ("setup_cycle: %lld\n", setup_cycle);

    printk ("imee_t: %lld\n", imee_t);
    printk ("__tmp_counter1: %d\n", __tmp_counter1);

    if (ts_buffer1)
    {
        int i;
        for (i = 0; i < ts_buffer_idx1; i += 2)
        {
            printk ("%lld %lld\n", ts_buffer1[i], ts_buffer1[i + 1]);
        }
        free_pages ((ulong) ts_buffer1, PAGE_ORDER);
        ts_buffer1 = 0;
    }

    imee_up = 0;

    printk ("=================end===================\n");
    return 0;
}
