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
         // "movl (%edx), %eax;\n\t" // (%ebx) : lock
         // "cmpl $1, %eax; \n\t"
         // "jne 2b;\n\t"

         "movl 4(%edx), %esi;\n\t" // 12(%edx) : source addr
         "movl 8(%edx), %ecx;\n\t" // 16(%edx) : size
         "leal 12(%edx), %edi;\n\t"
         "cld; \n\t"
         "rep movsd; \n\t"
            
         "vmcall; \n\t"
            );

}
