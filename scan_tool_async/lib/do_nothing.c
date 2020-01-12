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
         "vmcall; \n\t"
            );

}
