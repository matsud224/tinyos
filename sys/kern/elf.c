#include <kern/kernlib.h>
#include <kern/elf.h>
#include <kern/thread.h>
#include <kern/vmem.h>
#include <kern/fs.h>


int elf32_is_valid_exec(struct elf32_hdr *hdr) {
  if(strncmp(hdr->e_ident, ELFMAG, 4) != 0)
    return 0;
  if(hdr->e_ident[EI_CLASS] != ELFCLASS32)
    return 0;
  if(hdr->e_ident[EI_DATA] != ELFDATA2LSB)
    return 0;
  if(hdr->e_type != ET_EXEC)
    return 0;

  return 1;
}

void *elf32_load(struct inode *ino) {
  struct elf32_hdr *ehdr = malloc(sizeof(struct elf32_hdr));
  fs_read(ino, ehdr, 0, sizeof(struct elf32_hdr));
  if(elf32_is_valid_exec(ehdr)) {
    puts("invalid elf32 executable.");
    free(ehdr);
    return NULL;
  }

  u32 phdr_table_size = ehdr->e_phentsize * ehdr->e_phnum;
  struct elf32_phdr *phdr_table = malloc(phdr_table_size);
  fs_read(ino, phdr_table, ehdr->e_phoff, phdr_table_size);
  printf("e_phnum = %d\n", ehdr->e_phnum);

  for(int i=0; i < ehdr->e_phnum; i++) {
    struct elf32_phdr *phdr = (struct elf32_phdr *)((u8 *)phdr_table + ehdr->e_phentsize*i);
    switch(phdr->p_type) {
    case PT_LOAD:
      vm_add_area(current->vmmap, phdr->p_vaddr, phdr->p_filesz, inode_mapper_new(ino, phdr->p_offset), 0);
      printf("loaded: %x - %x (file mapping)\n", phdr->p_vaddr, phdr->p_vaddr + phdr->p_filesz);
      if(phdr->p_filesz < phdr->p_memsz) {
        vm_add_area(current->vmmap, phdr->p_vaddr+phdr->p_filesz, phdr->p_memsz - phdr->p_filesz, anon_mapper_new(phdr->p_memsz - phdr->p_filesz), 0);
        printf("loaded: %x - %x (file mapping)\n", phdr->p_vaddr, phdr->p_vaddr + phdr->p_filesz);
      }
      break;
    }
  }

  free(phdr_table);
  free(ehdr);
  printf("entry point: %x\n", ehdr->e_entry);
  return ehdr->e_entry;
}

