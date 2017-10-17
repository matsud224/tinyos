#pragma once

#include <stdint.h>
#include <stddef.h>
#include "envdep.h"
#include "protohdr.h"

#ifdef BIG_ENDIAN
#define hton16(val) val
#define ntoh16(val) val
#define hton32(val) val
#define ntoh32(val) val

#endif // BIG_ENDIAN
#ifdef LITTLE_ENDIAN
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
#endif // LITTLE_ENDIAN

#define IPADDR_TO_UINT32(a) ((a)[0]|((a)[1]<<8)|((a)[2]<<16)|((a)[3]<<24))


char *macaddr2str(u8 ma[]);
char *ipaddr2str(u8 ia[]);

/*
u64 macaddr2uint64(const u8 mac[]);
u32 ipaddr2uint32(const u8 ip[]);
*/

u16 checksum(u16 *data, int len);
u16 checksum2(u16 *data1, u16 *data2, int len1, int len2);
//u16 checksum_hdrstack(hdrstack *hs);

void ipaddr_hostpart(u8 *dst, u8 *addr, u8 *mask);
void ipaddr_networkpart(u8 *dst, u8 *addr, u8 *mask);


