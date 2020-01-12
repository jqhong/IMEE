#ifndef IMEE
#define IMEE
#include <linux/list.h>

#define DBG(fmt, ...) \
    do {printk ("%s(): " fmt, __func__, ##__VA_ARGS__); } while (0)
    
/*
#define DBG(fmt, ...) 
*/

#define ERR(fmt, ...) \
    do {printk ("%s(): " fmt, __func__, ##__VA_ARGS__); } while (0)

struct arg_blk 
{
    ulong cr3; 
    ulong num_x_page;
    ulong num_w_page;
    ulong offset;
    ulong code_host;
    ulong data_host;
    ulong stack;
    ulong entry;
    ulong int_handler;
    ulong thunk;
    ulong got;
    ulong got_len;
    ulong gotplt;
    ulong gotplt_len;
};

struct pt_mapping
{
    int lv;  // the level which the entry exits
    ulong e; // the paging structure entry
};

typedef struct introspection_context
{
    struct kvm* kvm;
    struct kvm_vcpu* target_vcpu;
    struct task_struct* task;

    ulong eptptr;
    u32 cr3;

    ulong visited;

    struct list_head leaf_page; // leaf pages of EPT
    struct list_head non_leaf_page; // non-leaf pages of EPT

    struct list_head node; // linked to global list

    u64* code_ept_pte_p;
    u64 code_ept_pte;
    u64* data_ept_pte_p;
    u64 data_ept_pte;

} intro_ctx_t;

extern intro_ctx_t* current_target;

#define PD_GPA (0xF0000000U)
#define PT_GPA_EXEC (0xF0001000U)
#define PT_GPA_DATA (0xF0002000U)
#define CODE_GPA (0xF0003000U)
#define DATA_GPA (0xF0004000U)


#define SCAN_ALL 1
#define SCAN_ONE 2
extern int imee_scan_mode;

extern volatile struct kvm_vcpu* imee_vcpu;
extern int enable_notifier;
extern volatile int imee_pid;
extern spinlock_t sync_lock;
extern volatile unsigned char go_flg;
extern ulong code_hpa, data_hpa;
extern volatile u32 last_cr3;
extern volatile u32 last_rip, last_rsp;

volatile extern int exit_flg;
extern int __tmp_counter;
extern int __tmp_counter3;
volatile extern u32 switched_cr3;
volatile extern int do_switch;

extern unsigned long long imee_t;
extern unsigned long long* ts_buffer1;
extern volatile int ts_buffer_idx1;
extern volatile int ts_buffer_idx1_limit;
extern spinlock_t sync_lock1;

ulong get_ptr_guest_page (struct task_struct* target_proc, struct kvm* target_kvm, gpa_t gpa);
u64* get_epte (intro_ctx_t* ctx, gpa_t gpa);
int remap_gpa (intro_ctx_t* ctx, ulong gpa);

void copy_leaf_ept (struct list_head* leaf_page, struct kvm_arch* arch);
intro_ctx_t* kvm_to_ctx (struct kvm* target);
void switch_intro_ctx (intro_ctx_t* next, struct kvm_vcpu* vcpu);
u64 make_imee_ept (struct list_head* leaf_page, struct list_head* non_leaf_page);
int start_guest_intercept (struct kvm_vcpu *vcpu);
struct kvm_vcpu* pick_cpu (struct kvm* target_kvm);

extern struct desc_ptr imee_idt, imee_gdt;
extern struct kvm_segment imee_tr;
extern ulong code_entry;
extern int trial_run;

extern intro_ctx_t* cur_ctx;

// void change_imee_ept (ulong hva, pte_t pte);
// void invalidate_imee_ept (ulong gpa);
int kvm_imee_stop (struct kvm_vcpu* vcpu);
long kvm_imee_get_guest_context (struct kvm_vcpu *vcpu, void* argp);

#endif
