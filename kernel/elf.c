#include "kernlib.h"
#include "elf.h"
#include "task.h"
#include "vmem.h"

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

void elf32_load(struct inode *ino, u8 *head) {
  struct elf32_hdr *ehdr = (struct elf32_hdr *)head;
  for(int i=0; i < ehdr->e_phnum; i++) {
    struct elf32_phdr *phdr = (struct elf32_phdr *)(head + ehdr->e_phoff + ehdr->e_phentsize*i);
    switch(phdr->p_type) {
    case PT_LOAD:
      vm_add_area(current->vmmap, phdr->p_vaddr, phdr->p_filesz, inode_mapper_new(ino, (u32)head+phdr->p_offset), 0);
      if(phdr->p_filesz < phdr->p_memsz)
        vm_add_area(current->vmmap, phdr->p_vaddr+phdr->p_filesz, phdr->p_memsz - phdr->p_filesz, anon_mapper_new(phdr->p_memsz - phdr->p_filesz), 0);
      break;
    }

  }
}

