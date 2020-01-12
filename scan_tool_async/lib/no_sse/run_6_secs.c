/* gcc hyp.c -nostdlib -fPIC -shared -o vmi.so */

void entry ()
{
    volatile unsigned int i = 0x8FFFFFFF;
    volatile unsigned int j = 0xFFF;
    volatile unsigned int k = 0x8FFFF;

    while (i > 0)
    {
        i --;
        while (j > 0)
        {
            j --;
            // while (k > 0)
            //     k--;
        }
    }

    // watermark in ECX, buffer start address in EDX
    asm (// "movl 0xFFC(%edx), %eax;\n\t"
         // "cmpl %ecx, %eax;\n\t"
         // "jne 1f;\n\t"

         "vmcall; \n\t"
            );

}
