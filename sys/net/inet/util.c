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

u16 checksum(u16 *data, int len){
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

u16 checksum2(u16 *data1, u16 *data2, int len1, int len2){
  u32 sum = 0;
  int len = len1 + len2;
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
/*
//もし途中に長さが奇数の部分があっても、次の領域にまたがって計算するため問題ない
u16 checksum_hdrstack(hdrstack *hs){
  u32 len = hdrstack_totallen(hs);
  u32 sum = 0;
  u32 thisstack_len = 0;
  u16 *data = (u16*)hs->buf;
  for(; len>1;len-=2){
    if(thisstack_len == hs->size-1){
      //のこり1byte
      sum+= ((u8)(*data)) | ((hs->next->buf[0])<<8);
      hs = hs->next;
      data = (u16*)(&(hs->buf[1]));
      thisstack_len = 1;
    }else{
      sum+=*data++;
      thisstack_len+=2;
    }

    if(thisstack_len == hs->size){
      hs = hs->next;
      data = (u16*)hs->buf;
      thisstack_len = 0;
    }
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
*/
void ipaddr_hostpart(u8 *dst, u8 *addr, u8 *mask){
  for(int i=0; i<IP_ADDR_LEN; i++)
    dst[i] = addr[i] & ~mask[i];
}

void ipaddr_networkpart(u8 *dst, u8 *addr, u8 *mask){
  for(int i=0; i<IP_ADDR_LEN; i++)
    dst[i] = addr[i] & mask[i];
}

