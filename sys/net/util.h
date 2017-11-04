#pragma once
#include <kern/types.h>
#include <kern/machine.h>

#ifdef ENDIAN_BE
#define hton16(val) val
#define ntoh16(val) val
#define hton32(val) val
#define ntoh32(val) val

#endif // ENDIAN_BE
#ifdef ENDIAN_LE
#define hton16(val) ((u16) ( \
    ((val) << 8) | ((val) >> 8) ))

#define ntoh16(val) ((u16) ( \
    ((val) << 8) | ((val) >> 8) ))

#define hton32(val) ((u32) ( \
    (((val) & 0x000000ff) << 24) | \
    (((val) & 0x0000ff00) <<  8) | \
    (((val) & 0x00ff0000) >>  8) | \
    (((val) & 0xff000000) >> 24) ))

#define ntoh32(val) ((u32) ( \
    (((val) & 0x000000ff) << 24) | \
    (((val) & 0x0000ff00) <<  8) | \
    (((val) & 0x00ff0000) >>  8) | \
    (((val) & 0xff000000) >> 24) ))
#endif // ENDIAN_LE
