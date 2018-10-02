
#define	_FREAD		0x0001	/* read enabled */
#define	_FWRITE		0x0002	/* write enabled */
#define	_FAPPEND	0x0008	/* append (writes guaranteed at the end) */
#define	_FCREAT		0x0200	/* open with file create */
#define	_FTRUNC		0x0400	/* open with truncation */
#define	_FEXCL		0x0800	/* error on open if file exists */
#define _FDIRECTORY     0x200000

#define	O_RDONLY	0		/* +1 == FREAD */
#define	O_WRONLY	1		/* +1 == FWRITE */
#define	O_RDWR		2		/* +1 == FREAD|FWRITE */
#define	O_APPEND				_FAPPEND
#define	O_CREAT					_FCREAT
#define	O_TRUNC					_FTRUNC
#define	O_EXCL					_FEXCL
#define O_SYNC					_FSYNC
#define O_DIRECTORY     _FDIRECTORY

void do_cli(void);
int syscall_0(int a0);
int syscall_1(int a0, int a1);
int syscall_2(int a0, int a1, int a2);
int syscall_3(int a0, int a1, int a2, int a3);
int syscall_4(int a0, int a1, int a2, int a3, int a4);
int syscall_5(int a0, int a1, int a2, int a3, int a4, int a5);
