#ifndef _HEADER_GDIFF
#define _HEADER_GDIFF 1
#define GDIFF_MAGIC 0xd1ffd1ff
#define GDIFF_MAGIC_LEN 4
#define GDIFF_VER4 4
#define GDIFF_VER_LEN 1

/* gdiff format
    EOF 0
    data 1 => 1 ubyte follows
    ...
    data 246 => 246 bytes
    data 247 => followed by ushort specifying byte_len
    data 248 => followed by uint specifying byte_len
    copy 249 => ushort, ubyte
    copy 250 => ushort, ushort
    copy 251 => ushort, int
    copy 252 => int, ubyte
    copy 253 => int, ushort
    copy 254 => int, int
    copy 255 => long, int
    
    note, specification seemed a bit off by the whole switching between signed and unsigned.
    soo. unsigned for all position indications based off of file start (crappy scheme).
    soo. version 4=original rfc spec.
    v5= signed based off of last dc offset position.
    v6= signed based off of current (versioned) position during reconstruction.
    magic=0xd1ffd1ff
    version is one byte.
    */

signed int gdiffEncodeDCBuffer(struct CommandBuffer *buffer, 
    unsigned int offset_type, unsigned char *ver, int fh);
#endif
