#include "syscall.h"
#include "tinyos.h"
#include "socket.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

#define PORT_NUM       12345
#define SHELL_NAME     "/bin/sh"

int wait_incoming() {
  int sock0, sock;
  struct sockaddr_in addr;
  struct sockaddr_in client;
  sock0 = socket(PF_INET, SOCK_STREAM);
  addr.family = PF_INET;
  addr.port = hton16(PORT_NUM);
  addr.addr = INADDR_ANY;
  bind(sock0, (struct sockaddr *)&addr);
  listen(sock0, 4);

  while(1) {
    sock = accept(sock0, (struct sockaddr *)&client);
    if(sock < 0)
      return -1;
    if(fork() == 0) {
      if(dup2(sock, STDIN_FILENO) < 0 || dup2(sock, STDOUT_FILENO) < 0
          || dup2(sock, STDERR_FILENO) < 0)
        return -3;
      return execve(SHELL_NAME, NULL, NULL);
    } else {
      close(sock);
    }
  }
}

int main(int argc, char *argv[]) {
  int ret = fork();
  if(ret == 0) {
    return wait_incoming();
  }
  return 0;
}
