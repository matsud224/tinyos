# tinyos
An UNIX-like toy operating system runs on x86 CPU

Modified gcc and newlib for cross-compile of userland applications are [here](https://github.com/matsud224/tinyos-cross).

# Build
TODO

# Features
* Preemptive multitasking
* Paging
* Buddy memory allocation
* Interrupts(PIC)
* Timer(PIT)
* Application runs in usermode
* ELF loader
* Delayed execution(like a work queue in Linux)
* IDE disk driver
* Serial port driver
* RTL8139 NIC driver
* Virtual file system layer
* MINIX3 file system
* FAT32 file system(readonly)
* TCP/IP protocol stack(ported from my [tinyip](https://github.com/matsud224/tinyip) project)
* Socket
* Ported Newlib C library
  * (implemented exit, close, execve, fork, fstat, getpid, isatty, link, lseek, open, read, sbrk, stat, unlink, wait and write)
* mruby in the kernel space


![Screenshot](https://user-images.githubusercontent.com/20959559/77240903-c35a6c80-6c2e-11ea-81ed-fa77791dbb41.png)
Logging in via TCP/IP and running `mruby`, `lua`, `ls` and `objdump`.


# References(random order)
* OSDev.org(https://wiki.osdev.org/Main_Page)
* はじめて読む486(ASCII)
* 0から作るOS開発(http://softwaretechnique.jp/OS_Development/scratchbuild.html)
* はじめてのOSコードリーディング(技術評論社)
* Linuxカーネル2.6解読室(ソフトバンククリエイティブ)
* FreeBSD Architecture Handbook(https://www.freebsd.org/doc/en/books/arch-handbook/index.html)
* オペレーティングシステム II(2010年)筑波大学 情報科学類 講義資料(http://www.coins.tsukuba.ac.jp/~yas/coins/os2-2010/)
* The Newlib Homepage(https://sourceware.org/newlib/)
* Realtek RTL8139DL DataSheet1.2(http://realtek.info/pdf/rtl8139d.pdf)
* RTL8139(A/B) Programming guide(https://www.cs.usfca.edu/~cruse/cs326f04/RTL8139_ProgrammersGuide.pdf)
* minixのファイルシステムからデータを読む。φ(・・*)ゞ ｳｰﾝ　カーネルとか弄ったりのメモ(https://kernhack.hatenablog.com/entry/20100501/1272677537)
* FATファイルシステムのしくみと操作法(http://elm-chan.org/docs/fat.html)
* malloc動画（https://www.youtube.com/watch?v=0-vWT-t0UHg)
* Interrupt and Exception Handling on the x86(https://pdos.csail.mit.edu/6.828/2004/lec/lec8-slides.pdf)
* Linux Kernel Documents(https://ja.osdn.net/projects/linux-kernel-docs/)
* Understanding TCP/IP Network Stack & Writing Network Apps(https://www.cubrid.org/blog/understanding-tcp-ip-network-stack)
* ...

# License
* Unlicense(Public Domain)
