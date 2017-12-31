#pragma once
#include <kern/kernlib.h>
#include <kern/fs.h>

#define EI_NIDENT 16
#define	ELFMAG		"\177ELF"

#define EI_CLASS	4		/* File class byte index */
#define ELFCLASSNONE	0		/* Invalid class */
#define ELFCLASS32	1		/* 32-bit objects */
#define ELFCLASS64	2		/* 64-bit objects */
#define ELFCLASSNUM	3

#define EI_DATA		5		/* Data encoding byte index */
#define ELFDATANONE	0		/* Invalid data encoding */
#define ELFDATA2LSB	1		/* 2's complement, little endian */
#define ELFDATA2MSB	2		/* 2's complement, big endian */
#define ELFDATANUM	3

enum et {
  ET_NONE		= 0,		/* No file type */
  ET_REL		= 1,		/* Relocatable file */
  ET_EXEC		= 2,		/* Executable file */
  ET_DYN		= 3,		/* Shared object file */
  ET_CORE		= 4,		/* Core file */
  ET_NUM		= 5,		/* Number of defined types */
  ET_LOOS		= 0xfe00,		/* OS-specific range start */
  ET_HIOS		= 0xfeff,		/* OS-specific range end */
  ET_LOPROC	= 0xff00,		/* Processor-specific range start */
  ET_HIPROC	= 0xffff,		/* Processor-specific range end */
};

#define EM_386		 3	/* Intel 80386 */

struct elf32_hdr {
  char e_ident[EI_NIDENT];
  u16 e_type;
  u16 e_machine;
  u32 e_version;
  u32 e_entry;
  u32 e_phoff;
  u32 e_shoff;
  u32 e_flags;
  u16 e_ehsize;
  u16 e_phentsize;
  u16 e_phnum;
  u16 e_shentsize;
  u16 e_shnum;
  u16 e_shstrndx;
};


enum sht {
  SHT_NULL		  = 0,		/* Section header table entry unused */
  SHT_PROGBITS  = 1,		/* Program data */
  SHT_SYMTAB	  = 2,		/* Symbol table */
  SHT_STRTAB	  = 3,		/* String table */
  SHT_RELA	  	= 4,		/* Relocation entries with addends */
  SHT_HASH	  	= 5,		/* Symbol hash table */
  SHT_DYNAMIC	  = 6,		/* Dynamic linking information */
  SHT_NOTE	  	= 7,		/* Notes */
  SHT_NOBITS	  = 8,		/* Program space with no data (bss) */
  SHT_REL			  = 9,		/* Relocation entries, no addends */
};

#define SHF_WRITE	     (1 << 0)	/* Writable */
#define SHF_ALLOC	     (1 << 1)	/* Occupies memory during execution */
#define SHF_EXECINSTR	     (1 << 2)	/* Executable */

struct elf32_shdr {
  u32 sh_name;
  u32 sh_type;
  u32 sh_flags;
  u32 sh_addr;
  u32 sh_offset;
  u32 sh_size;
  u32 sh_link;
  u32 sh_info;
  u32 sh_addralign;
  u32 sh_entsize;
};

enum pt {
  PT_NULL			= 0,		/* Program header table entry unused */
  PT_LOAD			= 1,		/* Loadable program segment */
  PT_DYNAMIC	= 2,		/* Dynamic linking information */
  PT_INTERP		= 3,		/* Program interpreter */
  PT_NOTE			= 4,		/* Auxiliary information */
  PT_SHLIB		= 5,		/* Reserved */
  PT_PHDR			= 6,		/* Entry for header table itself */
  PT_TLS			= 7,		/* Thread-local storage segment */
  PT_NUM			= 8,		/* Number of defined types */
};

#define PF_X		(1 << 0)	/* Segment is executable */
#define PF_W		(1 << 1)	/* Segment is writable */
#define PF_R		(1 << 2)	/* Segment is readable */

struct elf32_phdr {
  u32 p_type;
  u32 p_offset;
  u32 p_vaddr;
  u32 p_paddr;
  u32 p_filesz;
  u32 p_memsz;
  u32 p_flags;
  u32 p_align;
};


int elf32_is_valid_exec(struct elf32_hdr *hdr);
void *elf32_load(struct inode *ino);
 
