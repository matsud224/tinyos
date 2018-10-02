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
void cmd_rm(void);
void cmd_ln(void);
void cmd_cat(void);
void cmd_cd(void);
void cmd_mkdir(void);
void cmd_exit(void);

struct command cmd_table[] = {
  {"ls", cmd_ls},
  {"rm", cmd_rm},
  {"ln", cmd_ln},
  {"cat", cmd_cat},
  {"cd", cmd_cd},
  {"mkdir", cmd_mkdir},
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

void cmd_exit() {
  exit(0);
}
