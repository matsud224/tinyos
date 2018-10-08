#include "syscall.h"
#include "tinyos.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

int main(int argc, char *argv[], char *envp[]) {
  return execve("/bin/sh", NULL, NULL);
}
