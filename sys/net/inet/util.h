#pragma once
#include <net/inet/params.h> 
#include <net/inet/protohdr.h>

char *macaddr2str(u8 ma[]);
char *ipaddr2str(u8 ia[]);

u16 checksum(u16 *data, size_t len);
u16 checksum2(u16 *data1, u16 *data2, size_t len1, size_t len2);

#define inaddr_hostpart(a, m) ((a) & (~m))
#define inaddr_networkpart(a, m) ((a) & (m))
