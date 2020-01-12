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

            "3: \n\t"
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

            // benchmarking
            "rdtsc; \n\t"
            "movl $0xFE0, %esi; \n\t"
            "movl %eax, (%esi); \n\t"
            "movl %edx, 4(%esi); \n\t"

            "2: \n\t"
            "vmcall; \n\t"
            );
}
