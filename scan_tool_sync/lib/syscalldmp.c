/* gcc hyp.c -nostdlib -fPIC -shared -o vmi.so */
void entry ()
{
    asm (
            // benchmarking
            "rdtsc; \n\t"
            "movl $0xFF0, %esi; \n\t"
            "movl %eax, (%esi); \n\t"
            "movl %edx, 4(%esi); \n\t"

            // real work starts here
            "movl $0x0, %edx; \n\t"
            "movl 8(%edx), %eax; \n\t" // size in byte

            "movl $0, %ebp; \n\t" // counter to record how many time we copied

            "3: \n\t"
            "movl $0x0, %edx; \n\t"
            "movl 4(%edx), %edx; \n\t" // source address
            "movl $128, %eax; \n\t"
            "imull %ebp, %eax; \n\t"
            "addl %eax, %edx; \n\t"

            "movl %cr3, %ecx; \n\t"
            "movl %ebx, %cr3; \n\t"

            "movups (%edx), %xmm0; \n\t"
            "movups 16(%edx), %xmm1; \n\t"
            "movups 32(%edx), %xmm2; \n\t"
            "movups 48(%edx), %xmm3; \n\t"
            "movups 64(%edx), %xmm4; \n\t"
            "movups 80(%edx), %xmm5; \n\t"
            "movups 96(%edx), %xmm6; \n\t"
            "movups 112(%edx), %xmm7; \n\t"

            "1: \n\t"
            "movl %ecx, %cr3; \n\t"

            "movl $0, %edi; \n\t"
            "addl $12, %edi;\n\t"
            "addl %eax, %edi; \n\t"

            "movups %xmm0, (%edi); \n\t"
            "movups %xmm1, 16(%edi); \n\t"
            "movups %xmm2, 32(%edi); \n\t"
            "movups %xmm3, 48(%edi); \n\t"
            "movups %xmm4, 64(%edi); \n\t"
            "movups %xmm5, 80(%edi); \n\t"
            "movups %xmm6, 96(%edi); \n\t"
            "movups %xmm7, 112(%edi); \n\t"

            "incl %ebp; \n\t"
            "cmpl $11, %ebp; \n\t"
            "jl 3b; \n\t"

            "2: \n\t"
            "vmcall; \n\t"
            );
}
