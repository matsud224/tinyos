#include "syscall.h"
#include "tinyos.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#define BUFSIZE 256

struct command {
  const char *name;
  void (*func)(void);
};

void cmd_ls(void);
void cmd_ps(void);
void cmd_netstat(void);
void cmd_rm(void);
void cmd_ln(void);
void cmd_cat(void);
void cmd_cd(void);
void cmd_mkdir(void);
void cmd_kill(void);
void cmd_exit(void);

struct command cmd_table[] = {
  {"ls", cmd_ls},
  {"ps", cmd_ps},
  {"netstat", cmd_netstat},
  {"rm", cmd_rm},
  {"ln", cmd_ln},
  {"cat", cmd_cat},
  {"cd", cmd_cd},
  {"mkdir", cmd_mkdir},
  {"kill", cmd_kill},
  {"exit", cmd_exit},
  {NULL, NULL}
};

int main(int argc, char *argv[], char *envp[]) {
  char buf[BUFSIZE];
  char *tp;

  while(1) {
    printf("$ ");
    fflush(stdout);
    fgets(buf, BUFSIZE, stdin);

    char *p = buf;
    while(*p != '\0') {
      if(*p == '\n')
        *p = '\0';
      p++;
    }

    tp = strtok(buf, " ");
    if(tp == NULL)
      continue;

    struct command *cmd = cmd_table;
    while(cmd->name) {
      if(strcmp(cmd->name, tp) == 0)
        break;
      cmd++;
    }

    if(cmd->name == NULL)
      cmd_exec(tp);
    else
      cmd->func();
  }
}

char *next_arg() {
  return strtok(NULL, " ");
}

void cmd_ls() {
  char *dirname = next_arg();
  if(dirname == NULL)
    dirname = ".";

  int dir = open(dirname, O_RDWR | O_DIRECTORY);
  if(dir < 0) {
    puts("failed to open");
    return;
  }

  struct dirent dirents[8];
  int bytes;
  while(bytes = getdents(dir, dirents, sizeof(dirents))) {
    for(int i=0; i<bytes/sizeof(struct dirent); i++) {
      printf("%u %s\n", (uint32_t)dirents[i].d_vno, dirents[i].d_name);
    }
  }

  close(dir);
}

const char *thread_state_name[] = {
  "RUNNING",
  "WAITING",
  "EXITED ",
  "ZOMBIE ",
};

void cmd_ps() {
  struct threadent threadents[64];
  int bytes = gettents(threadents, sizeof(threadents));
  for(int i=0; i<bytes/sizeof(struct threadent); i++) {
    if(threadents[i].brk == 0)
      printf("%3u %3u %s (kernel thread)               \"%s\" \n", threadents[i].pid, threadents[i].ppid, thread_state_name[threadents[i].state], threadents[i].name);
    else
      printf("%3u %3u %s %2d %8x %8x %3d %4d \"%s\" \n", threadents[i].pid, threadents[i].ppid, thread_state_name[threadents[i].state], threadents[i].priority, threadents[i].brk, threadents[i].user_stack_size, threadents[i].num_files, threadents[i].num_pfs, threadents[i].name);
  }
}

const char *sock_domain_name[] = {
  "LINK",
  "INET",
};

const char *sock_type_name[] = {
  "STREAM",
  "DGRAM ",
};

const char *tcp_state_name[] = {
  "CLOSED",
  "LISTEN",
  "SYN_RCVD",
  "SYN_SENT",
  "ESTABLISHED",
  "FIN_WAIT_1",
  "FIN_WAIT_2",
  "CLOSING",
  "TIME_WAIT",
  "CLOSE_WAIT",
  "LAST_ACK",
};
void cmd_netstat() {
  struct sockent sockents[64];
  int bytes = getsents(sockents, sizeof(sockents));
  for(int i=0; i<bytes/sizeof(struct sockent); i++) {
    printf("%s %s", sock_domain_name[sockents[i].domain], sock_type_name[sockents[i].type]);
    if(sockents[i].type == 0)
      printf(" %s\n", tcp_state_name[sockents[i].state]);
    else
      printf("\n");
  }
}

#define MAX_ARGS 128
char *argv[MAX_ARGS];

void cmd_exec(const char *path) {
  int pid = fork();
  if(pid == 0) {
    int i = 0;
    const char *arg = path;
    do {
      argv[i++] = arg;
    }while((arg = next_arg()) && i < MAX_ARGS);

    execve(path, argv, NULL);
    exit(-1);
  } else {
    wait(NULL);
  }
}

void cmd_rm() {
  char *name = next_arg();
  if(name == NULL) {
    puts("invalid argument");
    return;
  }

  if(unlink(name) != 0)
    puts("failed to remove");
}

void cmd_ln() {
  char *target = next_arg();
  char *name = next_arg();
  if(target == NULL || name == NULL) {
    puts("invalid argument");
    return;
  }

  if(link(target, name) != 0)
    puts("failed to link");
}

char buf[1024];

void cmd_cat() {
  while(1) {
    char *name = next_arg();
    if(name == NULL) {
      return;
    }
    int fd = open(name, O_RDWR);
    if(fd < 0) {
      puts("failed to open");
      return;
    }

    int len;
    while((len = read(fd, buf, 1024-1)) > 0) {
      buf[len] = '\0';
      printf("%s", buf);
    }

    close(fd);
  }
}

void cmd_cd() {
  char *dirname = next_arg();
  if(dirname == NULL)
    dirname = "/";

  if(chdir(dirname))
    puts("invalid pathname");
}

void cmd_mkdir() {
  char *dirname = next_arg();
  if(dirname == NULL) {
    puts("invalid pathname");
  }

  if(mkdir(dirname, 0))
    puts("failed to mkdir");
}

void cmd_kill() {
  char *arg1 = next_arg();
  if(arg1 == NULL)
    puts("invalid argument");
  else
    kill(atoi(arg1), SIGKILL);
}

void cmd_exit() {
  exit(0);
}
