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


void _init(void);


KERNENTRY void kernel_main(void) {
  a20_enable();
	vga_init();
	puts("Starting kernel...");
  malloc_init();
  page_init();
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

  _init();
  //switch_to_chardev();

  ip_set_defaultgw(IPADDR(192,168,4,1));
  pit_init();

  dispatcher_run();

  while(1);
}

struct thread *timer_thread;

void thread_timer(void *arg UNUSED) {
  while(1) {
    thread_set_alarm(timer_thread, msecs_to_ticks(10000));
    thread_sleep(timer_thread);
    puts("--- 10sec timer---");
  }
}

void thread_main(void *arg UNUSED) {
  if(fs_mountroot(ROOTFS_TYPE, ROOTFS_DEV))
    puts("fs: failed to mount");
  else
    puts("fs: mount succeeded");

  //thread_run(timer_thread = kthread_new(thread_timer, NULL, "timer_10sec"));

  struct file *f = open("/dev/tty1", O_RDWR);
  if(!f) {
    puts("tty1 open failed.");
  }
  current->files[0] = f;
  current->files[1] = dup(f);
  current->files[2] = dup(f);

  thread_exec("/bin/init");
  puts("exec failed");
}
