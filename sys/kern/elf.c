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


void *elf32_load(struct file *f, void **brk) {
  struct elf32_hdr *ehdr = malloc(sizeof(struct elf32_hdr));
  read(f, ehdr, sizeof(struct elf32_hdr));
  if(elf32_is_valid_exec(ehdr)) {
    puts("invalid elf32 executable.");
    free(ehdr);
    return NULL;
  }
  u32 phdr_table_size = ehdr->e_phentsize * ehdr->e_phnum;
  struct elf32_phdr *phdr_table = malloc(phdr_table_size);
  lseek(f, ehdr->e_phoff, SEEK_SET);
  read(f, phdr_table, phdr_table_size);

  void *tail = 0;

  for(int i=0; i < ehdr->e_phnum; i++) {
    struct elf32_phdr *phdr = (struct elf32_phdr *)((u8 *)phdr_table + ehdr->e_phentsize*i);
    switch(phdr->p_type) {
    case PT_LOAD:
      vm_add_area(current->vmmap, phdr->p_vaddr, phdr->p_memsz, file_mapper_new(dup(f), phdr->p_offset, phdr->p_filesz), 0);

      if(phdr->p_vaddr + phdr->p_memsz > tail)
        tail = phdr->p_vaddr + phdr->p_memsz;

      //printf("loaded: %x - %x fileoff: %x (file mapping)\n", phdr->p_vaddr, phdr->p_vaddr + phdr->p_filesz, phdr->p_offset);
      break;
    }
  }

  free(phdr_table);
  free(ehdr);

  *brk = tail;
  return (void *)ehdr->e_entry;
}

