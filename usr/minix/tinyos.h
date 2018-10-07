#include <sys/types.h>

#define MAX_FILENAME_LEN   255 //null is not contained
#define MAX_THREADNAME_LEN 64  //null is not contained

struct dirent {
  uint32_t d_vno;
  char d_name[MAX_FILENAME_LEN+1];
};

struct threadent {
  uint8_t state;
  uint32_t flags;
  pid_t pid;
  pid_t ppid;
  char name[MAX_THREADNAME_LEN];
  uint32_t brk;
  uint32_t user_stack_size;
};

struct sockent {
  int domain;
  int type;
  int state;
};

int getdents(int fd, struct dirent *dirp, size_t count);
int gettents(struct threadent *thp, size_t count);
int getsents(struct sockent *sockp, size_t count);
