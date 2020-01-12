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
    // watermark in ECX, buffer start address in EDX
    asm (// "movl 0xFFC(%edx), %eax;\n\t"
         // "cmpl %ecx, %eax;\n\t"
         // "jne 1f;\n\t"
         "2: \n\t"
         "movl (%edx), %eax;\n\t" // (%ebx) : lock
         "cmpl $1, %eax; \n\t"
         "jne 2b;\n\t"

         "movl 4(%edx), %esi; \n\t" // 4(%edx) : list header addr
         "movl %esi, %ebp; \n\t" // 4(%edx) : list header addr
         "leal 28(%edx), %edi;\n\t"

         "4: \n\t"
         // "movl 12(%edx), %eax; \n\t" 
         "movl %esi, %ebx; \n\t" 
         // "addl %eax, %esi; \n\t"          // 12(%edx) : offset1
         "addl $0x20c, %esi; \n\t"          // 12(%edx) : offset1
         "movl $1, %ecx;\n\t"               // 16(%edx) : size
         "cld; \n\t"
         "rep movsd; \n\t"

         "movl %ebx, %esi; \n\t"
         "addl $0x2ec, %esi; \n\t"
         "movl $4, %ecx;\n\t"              
         "cld; \n\t"
         "rep movsd; \n\t"

         // "movl 8(%edx), %eax; \n\t" // 8(%edx) : next_offset
         "movl %ebx, %esi; \n\t"
         // "addl %eax, %esi; \n\t"
         "addl $0x1bc, %esi; \n\t"
         "movl (%esi), %esi; \n\t" // ->next node address in %esi
         "subl $0x1bc, %esi; \n\t"
         "cmpl %esi, %ebp; \n\t"
         "jne 4b; \n\t"

         "xorl %eax, %eax; \n\t"
         "movl %eax, (%edx); \n\t"
         "jmp 2b; \n\t"
            
         "1: \n\t"
         "jmp 3f;\n\t"
         "3: \n\t"
            
            );

}
