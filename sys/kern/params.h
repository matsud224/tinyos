#pragma once
#include <kern/types.h>

#define KERN_VMEM_ADDR					((vaddr_t)0xc0000000u)
#define MEMORYMAP_ADDR					((paddr_t)0x500u)
#define KERN_CODE_ADDR					((paddr_t)0x7e00u)
#define KERN_STACK_ADDR					((paddr_t)0x7bffu)
#define PROTMEM_ADDR						((paddr_t)0x100000u)
#define KERN_STRAIGHT_MAP_SIZE	((size_t)0x38000000) //896MB

#define KERN_VMEM_TO_PHYS(v)		((paddr_t)((v) - KERN_VMEM_ADDR))
#define PHYS_TO_KERN_VMEM(p)		((vaddr_t)((p) + KERN_VMEM_ADDR))

#define PAGESIZE			4096

#define MAX_BLKDEV		64
#define MAX_CHARDEV		128
#define MAX_NETDEV		64 
#define MAX_FSTYPE		32
#define MAX_MOUNT			32

#define MAX_FILENAME_LEN 255 //null is not contained

#define NBLKBUF				64
#define NVCACHE				1024

#define CLASS_BLKDEV	1
#define CLASS_CHARDEV	2
#define CLASS_NETDEV	3

#define GDT_SEL_NULL			0*8
#define GDT_SEL_CODESEG_0	1*8
#define GDT_SEL_DATASEG_0	2*8
#define GDT_SEL_CODESEG_3	3*8
#define GDT_SEL_DATASEG_3	4*8
#define GDT_SEL_TSS				5*8


#define ROOTFS_TYPE "minix3"
#define ROOTFS_DEV DEVNO(1, 2)
