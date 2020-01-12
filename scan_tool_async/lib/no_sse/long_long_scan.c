/* gcc hyp.c -nostdlib -fPIC -shared -o vmi.so */

void entry ()
{
    volatile int i = 0x8FFFFFFF;

    while (i > 0)
        i --;

    // watermark in ECX, buffer start address in EDX
    asm (// "movl 0xFFC(%edx), %eax;\n\t"
         // "cmpl %ecx, %eax;\n\t"
         // "jne 1f;\n\t"

         "vmcall; \n\t"
            );

}
