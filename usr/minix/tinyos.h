#include <sys/types.h>

#define MAX_FILENAME_LEN 255 //null is not contained

struct dirent {
  uint32_t d_vno;
  char d_name[MAX_FILENAME_LEN+1];
};


int getdents(int fd, struct dirent *dirp, size_t count);
