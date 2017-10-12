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
#define hton16(val) ((uint16_t) ( \
    ((val) << 8) | ((val) >> 8) ))

#define ntoh16(val) ((uint16_t) ( \
    ((val) << 8) | ((val) >> 8) ))

#define hton32(val) ((uint32_t) ( \
    (((val) & 0x000000ff) << 24) | \
    (((val) & 0x0000ff00) <<  8) | \
    (((val) & 0x00ff0000) >>  8) | \
    (((val) & 0xff000000) >> 24) ))

#define ntoh32(val) ((uint32_t) ( \
    (((val) & 0x000000ff) << 24) | \
    (((val) & 0x0000ff00) <<  8) | \
    (((val) & 0x00ff0000) >>  8) | \
    (((val) & 0xff000000) >> 24) ))
#endif // LITTLE_ENDIAN

#define IPADDR_TO_UINT32(a) ((a)[0]|((a)[1]<<8)|((a)[2]<<16)|((a)[3]<<24))


char *macaddr2str(uint8_t ma[]);
char *ipaddr2str(uint8_t ia[]);

/*
uint64_t macaddr2uint64(const uint8_t mac[]);
uint32_t ipaddr2uint32(const uint8_t ip[]);
*/

uint16_t checksum(uint16_t *data, int len);
uint16_t checksum2(uint16_t *data1, uint16_t *data2, int len1, int len2);
uint16_t checksum_hdrstack(hdrstack *hs);

void ipaddr_hostpart(uint8_t *dst, uint8_t *addr, uint8_t *mask);
void ipaddr_networkpart(uint8_t *dst, uint8_t *addr, uint8_t *mask);

uint32_t hdrstack_totallen(hdrstack *target);
void hdrstack_cpy(char *dst, hdrstack *src, uint32_t start, uint32_t len);

