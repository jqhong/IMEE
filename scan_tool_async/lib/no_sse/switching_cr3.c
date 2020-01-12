/* gcc hyp.c -nostdlib -fPIC -shared -o vmi.so */

void entry ()
{
    asm (
         "movl $0, %eax; \n\t"
         "movl (%eax), %eax; \n\t"
         "movl %ebx, %cr3; \n\t"

         "movl $0xC183d419, %ebx; \n\t"
         "movl (%ebx), %ebx; \n\t"
         "movl $0, %eax; \n\t"
         "movl %ebx, (%eax); \n\t"



         "vmcall; \n\t"
            );

}
