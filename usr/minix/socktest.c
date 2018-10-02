#include "socket.h"
#include "syscall.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <string.h>

int main() {
  int sock0;
  struct sockaddr_in addr;
  struct sockaddr_in client;
  int sock;
  sock0 = socket(PF_INET, SOCK_STREAM);
  addr.family = PF_INET;
  addr.port = hton16(12345);
  addr.addr = INADDR_ANY;
  bind(sock0, (struct sockaddr *)&addr);
  listen(sock0, 1);
  puts("listening...");
  sock = accept(sock0, (struct sockaddr *)&client);
  puts("accepted");
  char buf[2048];
  int len;
  while((len = recv(sock, buf, sizeof(buf), 0)) > 0) {
    printf("tcp: received %d byte\n", len);
    buf[len] = '\0';
    puts(buf);
    send(sock, "ok. ", 4, 0);
  }
  puts("tcp connection closed.");
  close(sock);
  close(sock0);
  return 0;
}
