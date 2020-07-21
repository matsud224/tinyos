#include <kern/kernlib.h>
#include <kern/vga.h>
#include <kern/trap.h>
#include <kern/page.h>
#include <kern/idt.h>
#include <kern/pic.h>
#include <kern/pci.h>
#include <kern/pit.h>
#include <kern/malloc.h>
#include <kern/kernasm.h>
#include <kern/pagetbl.h>
#include <kern/vmem.h>
#include <kern/fs.h>
#include <kern/gdt.h>
#include <kern/thread.h>
#include <kern/blkdev.h>
#include <kern/chardev.h>
#include <kern/netdev.h>
#include <net/inet/inet.h>
#include <net/inet/ip.h>
#include <kern/timer.h>
#include <net/socket/socket.h>
#include <net/util.h>
#include <kern/multiboot.h>


void _init(void);

void kernel_main(struct multiboot_info *bootinfo) {
	vga_init();
	puts("Starting kernel...");
  malloc_init();
  page_init(bootinfo);

  idt_init();
  //for(int i=0; i<=0xff; i++)
    //idt_register(i, IDT_INTGATE, spurious_inthandler);
  idt_register(13, IDT_INTGATE, gpe_inthandler);
  idt_register(14, IDT_INTGATE, pf_inthandler);
  idt_register(0x80, IDT_INTGATE, syscall_inthandler);
  pic_init();
  pagetbl_init();
  dispatcher_init();
  vmem_init();
  pci_init();
  blkdev_init();
  chardev_init();
  netdev_init();
  fs_init();
  kernelmrb_init();

  kernelmrb_load_string("puts 'hello from mruby!'");

  _init();
  //switch_to_chardev();

	ip_set_defaultgw(IPADDR(192,168,4,1));
  pit_init();

  dispatcher_run();

  while(1);
}

void thread_main(void *arg UNUSED) {
  if(fs_mountroot(ROOTFS_TYPE, ROOTFS_DEV))
    puts("fs: failed to mount");
  else
    puts("fs: mount succeeded");

  struct file *f = open("/dev/tty1", O_RDWR);
  if(!f) {
    puts("tty1 open failed.");
  }
  current->files[0] = f;
  current->files[1] = dup(f);
  current->files[2] = dup(f);

  thread_chdir("/");

  thread_exec_in_usermode("/bin/init", NULL, NULL);
  puts("exec failed");
}
