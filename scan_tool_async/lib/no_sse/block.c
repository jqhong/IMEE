/* gcc hyp.c -nostdlib -fPIC -shared -o vmi.so */
/*
[ 2175.692663] task is f6b4e580
[ 2175.692665] offsets: ts_state=0x0, ts_pid=0x20c, ts_next=0x1bc, ts_comm=0x2ec, ts_prev=0x1c0
[ 2175.692668] task struct size: 0xca8

sys_call_table size; 349
sys_open 5
*/

#define M 10
#define TS_SIZE 0xca8
#define TS_STATE 0x0
#define TS_PID 0x20c
#define TS_NEXT 0x1bc
#define TS_COMM 0x2ec
#define TS_PREV 0x1c0

#define SYS_SIZE 349
#define SYS_OPEN 5

/*
struct symbol {
	char name[32];
	unsigned long addr;	
};

static const struct symbol symbols[M] = {
	{
		.name = "modules",
		.addr = 0xc1738e98
	},

	{
		.name = "runqueues",
		.addr = 0xc182f080
	},

	{
		.name = "init_mm",
		.addr = 0xc173f900
	},

	{
		.name = "init_task",
		.addr = 0xc1727020
	},

	{	
		.name = "sys_call_table",
		.addr = 0xc1507160
	},
};
*/

void entry ()
{
    // watermark in ECX, buffer start address in EDX
    asm (// "movl 0xFFC(%edx), %eax;\n\t"
         // "cmpl %ecx, %eax;\n\t"
         // "jne 1f;\n\t"
         "2: \n\t"
         "movl (%edx), %eax;\n\t" // (%ebx) : lock
         "cmpl $1, %eax; \n\t"
         "jne 2b;\n\t"
         "movl 12(%edx), %eax;\n\t" // 12(%edx) : source addr
         // "movl 16(%edx), %ebx;\n\t" // 16(%edx) : size in dword
         // "xorl %esi, %esi; \n\t"
         "4: \n\t"
         "movl (%eax), %ecx;\n\t"
         "movl %ecx, 20(%edx);\n\t"
         // "inc $1, %esi;\n\t"
         // "cmpl %esi, %ebx;\n\t"
         // "jl 4b; \n\t"
         "xorl %eax, %eax; \n\t"
         "movl %eax, (%edx); \n\t"
         "jmp 2b; \n\t"
            
         "1: \n\t"
         "jmp 3f;\n\t"
         "3: \n\t"
            
            );

}
