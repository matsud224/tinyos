#include <net/inet/util.h>
#include <kern/kernlib.h>

char *macaddr2str(u8 ma[]){
  static char str[18];
  //sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
    //ma[0], ma[1], ma[2], ma[3], ma[4], ma[5]);
  return str;
}

char *ipaddr2str(u8 ia[]){
  static char str[16];
  //sprintf(str, "%d.%d.%d.%d",
  //  ia[0], ia[1], ia[2], ia[3]);
  return str;
}

u16 checksum(u16 *data, size_t len){
  u32 sum = 0;
  for(; len>1;len-=2){
    sum+=*data++;
    if(sum &0x80000000)
      sum=(sum&0xffff)+(sum>>16);
  }
  if(len == 1){
    u16 i=0;
    *(u8*)(&i)= *(u8*)data;
    sum+=i;
  }
  while(sum>>16)
    sum=(sum&0xffff)+(sum>>16);

  return ~sum;
}

u16 checksum2(u16 *data1, u16 *data2, size_t len1, size_t len2){
  u32 sum = 0;
  size_t len = len1 + len2;
  u16 *data = data1;
  int complen = 0;
  for(; len>1;len-=2){
    if(complen == len1-1){
      //data1側がのこり1byte
      sum+= ((u8)*data) | ((*data2)<<8);
      complen = len1+1;
      data = &(data2[1]);
    }else{
      sum+=*data++;
    }
    complen+=2;
    if(sum &0x80000000)
      sum=(sum&0xffff)+(sum>>16);
    if(complen == len1)
      data = data2;
  }
  if(len == 1){
    u16 i=0;
    *(u8*)(&i)= *(u8*)data;
    sum+=i;
  }
  while(sum>>16)
    sum=(sum&0xffff)+(sum>>16);

  return ~sum;
}

