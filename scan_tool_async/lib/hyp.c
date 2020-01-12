/* gcc hyp.c -nostdlib -fPIC -shared -o vmi.so */

/* %edx: buffer page start address. Mapped at 0
 * 0(%edx): lock 0: not ready 1: ready 2: time to quit
 * 4(%edx): start addresss
 * 8(%edx): length to read
 */

void entry ()
{
    asm (
            // benchmarking
            "rdtsc; \n\t"
            "movl $0xFF0, %esi; \n\t"
            "movl %eax, (%esi); \n\t"
            "movl %edx, 4(%esi); \n\t"

            "3: \n\t"

            "movl $0x0, %edx; \n\t"
            "movl (%edx), %eax; \n\t" // load lock value
            "cmpl $0, %eax; \n\t"     // 0: not ready, retry
            "je 3b; \n\t"
            "cmpl $2, %eax; \n\t"     // 2: it's time to exit
            "je 2f; \n\t"


            // real work starts here
            "movl 8(%edx), %eax; \n\t" // size in byte

            "movl $0x0, %edx; \n\t"
            "movl 4(%edx), %edx; \n\t" // source address

            "movl %cr3, %ecx; \n\t"
            "movl %ebx, %cr3; \n\t"

            "movups (%edx), %xmm0; \n\t"
            "movups 16(%edx), %xmm1; \n\t"
            "movups 32(%edx), %xmm2; \n\t"
            "movups 48(%edx), %xmm3; \n\t"

            "1: \n\t"
            "movl %ecx, %cr3; \n\t"

            "movl $0, %edi; \n\t"
            "addl $12, %edi;\n\t"

            "movups %xmm0, (%edi); \n\t"
            "movups %xmm1, 16(%edi); \n\t"
            "movups %xmm2, 32(%edi); \n\t"
            "movups %xmm3, 48(%edi); \n\t"

            "xorl %eax, %eax; \n\t"
            "movl $0x0, %edx; \n\t" 
            "movl %eax, (%edx); \n\t" // release the lock
            "jmp 3b; \n\t"

            // benchmarking
            "rdtsc; \n\t"
            "movl $0xFE0, %esi; \n\t"
            "movl %eax, (%esi); \n\t"
            "movl %edx, 4(%esi); \n\t"

            "2: \n\t"
            "vmcall; \n\t"
            );
}
