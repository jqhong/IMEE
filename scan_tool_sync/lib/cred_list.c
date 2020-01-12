/* gcc hyp.c -nostdlib -fPIC -shared -o vmi.so */
/*
block read
buffer[0] lock
buffer[1] type
buffer[2] base
buffer[3] size

batch read
buffer[0] lock
buffer[1] type
buffer[2] start
buffer[3] next_offset 
buffer[4] offset1
buffer[5] size1
buffer[6] offset2
buffer[7] size2

buffer[1023] watermark
*/

void entry ()
{
    asm (
         // "2: \n\t"
         // "movl (%edx), %eax;\n\t" // (%ebx) : lock
         // "cmpl $1, %eax; \n\t"
         // "jne 2b;\n\t"

         // benchmarking
         "rdtsc; \n\t"
         "movl $0xFF0, %esi; \n\t"
         "movl %eax, (%esi); \n\t"
         "movl %edx, 4(%esi); \n\t"

         "movl $0, %edx; \n\t"
         "leal 20(%edx), %edi;\n\t" // buffer[5]: start of reply
         "movl 4(%edx), %ebp; \n\t" 

         "movl 4(%edx), %esi; \n\t" // buffer[1]: start
         "movl 8(%edx), %eax; \n\t" // buffer[2]: next_offset
         "movl 12(%edx), %edx; \n\t" // buffer[3]: offset_to_read
         // "movl 16(%edx), %esp; \n\t" // buffer[4]: size_to_read


         "movl %cr3, %ecx; \n\t"
         "movl %ebx, %cr3; \n\t"

         "4: \n\t"
         "addl %edx, %esi; \n\t"          // %edx: offset_to_read
         "movl (%esi), %esi; \n\t"          // %edx: offset_to_read

         "movups (%esi), %xmm0; \n\t"
         "movups 16(%esi), %xmm1; \n\t"
         "movups 32(%esi), %xmm2; \n\t"
         "movups 48(%esi), %xmm3; \n\t"
         "movups 64(%esi), %xmm4; \n\t"
         "movups 80(%esi), %xmm5; \n\t"
         "movups 96(%esi), %xmm6; \n\t"
         "movups 112(%esi), %xmm7; \n\t"

         "movl %ecx, %cr3; \n\t"

         "movups %xmm0, (%edi); \n\t"
         "movups %xmm1, 16(%edi); \n\t"
         "movups %xmm2, 32(%edi); \n\t"
         "movups %xmm3, 48(%edi); \n\t"
         "movups %xmm4, 64(%edi); \n\t"
         "movups %xmm5, 80(%edi); \n\t"
         "movups %xmm6, 96(%edi); \n\t"
         "movups %xmm7, 112(%edi); \n\t"
         "addl $116, %edi; \n\t"
         "cmpl $3960, %edi; \n\t"
         "jl 2f; \n\t"
         "movl $20, %edi; \n\t"

         "2: \n\t"
         "movl %cr3, %ecx; \n\t"
         "movl %ebx, %cr3; \n\t"

         "subl %edx, %esi; \n\t"
         "addl %eax, %esi; \n\t"
         "movl (%esi), %esi; \n\t" // ->next node address in %esi
         "subl %eax, %esi; \n\t"
         "cmpl %esi, %ebp; \n\t"
         "jne 4b; \n\t"

         "1: \n\t"
         "vmcall;   \n\t"
            
          );

}
