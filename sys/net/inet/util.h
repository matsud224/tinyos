#pragma once
#include <net/inet/params.h> 
#include <net/inet/protohdr.h>

char *macaddr2str(u8 ma[]);
char *ipaddr2str(u8 ia[]);

u16 checksum(u16 *data, int len);
u16 checksum2(u16 *data1, u16 *data2, int len1, int len2);

void ipaddr_hostpart(u8 *dst, u8 *addr, u8 *mask);
void ipaddr_networkpart(u8 *dst, u8 *addr, u8 *mask);
