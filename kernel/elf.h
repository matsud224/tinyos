#pragma once

#include <stddef.h>
#include <stdint.h>


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

#define ET_NONE		0		/* No file type */
#define ET_REL		1		/* Relocatable file */
#define ET_EXEC		2		/* Executable file */
#define ET_DYN		3		/* Shared object file */
#define ET_CORE		4		/* Core file */
#define	ET_NUM		5		/* Number of defined types */
#define ET_LOOS		0xfe00		/* OS-specific range start */
#define ET_HIOS		0xfeff		/* OS-specific range end */
#define ET_LOPROC	0xff00		/* Processor-specific range start */
#define ET_HIPROC	0xffff		/* Processor-specific range end */

#define EM_386		 3	/* Intel 80386 */

struct elf32_hdr {
  char e_ident[EI_NIDENT];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
};


#define SHT_NULL	  0		/* Section header table entry unused */
#define SHT_PROGBITS	  1		/* Program data */
#define SHT_SYMTAB	  2		/* Symbol table */
#define SHT_STRTAB	  3		/* String table */
#define SHT_RELA	  4		/* Relocation entries with addends */
#define SHT_HASH	  5		/* Symbol hash table */
#define SHT_DYNAMIC	  6		/* Dynamic linking information */
#define SHT_NOTE	  7		/* Notes */
#define SHT_NOBITS	  8		/* Program space with no data (bss) */
#define SHT_REL		  9		/* Relocation entries, no addends */

#define SHF_WRITE	     (1 << 0)	/* Writable */
#define SHF_ALLOC	     (1 << 1)	/* Occupies memory during execution */
#define SHF_EXECINSTR	     (1 << 2)	/* Executable */

struct elf32_shdr {
  uint32_t sh_name;
  uint32_t sh_type;
  uint32_t sh_flags;
  uint32_t sh_addr;
  uint32_t sh_offset;
  uint32_t sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  uint32_t sh_addralign;
  uint32_t sh_entsize;
};


#define	PT_NULL		0		/* Program header table entry unused */
#define PT_LOAD		1		/* Loadable program segment */
#define PT_DYNAMIC	2		/* Dynamic linking information */
#define PT_INTERP	3		/* Program interpreter */
#define PT_NOTE		4		/* Auxiliary information */
#define PT_SHLIB	5		/* Reserved */
#define PT_PHDR		6		/* Entry for header table itself */
#define PT_TLS		7		/* Thread-local storage segment */
#define	PT_NUM		8		/* Number of defined types */

#define PF_X		(1 << 0)	/* Segment is executable */
#define PF_W		(1 << 1)	/* Segment is writable */
#define PF_R		(1 << 2)	/* Segment is readable */

struct elf32_phdr {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
};

