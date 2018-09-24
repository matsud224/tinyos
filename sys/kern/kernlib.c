#include <kern/kernlib.h>
#include <stdint.h>
#include <stddef.h>

int abs(int n) {
  return n<0?-n:n;
}

char *strncpy(char *dest, const char *src, size_t n) {
  size_t i;
  for(i=0; i<n && src[i]!='\0'; i++) {
    dest[i] = src[i];
  }
  for(; i<n; i++)
    dest[i] = '\0';
  return dest;
}

size_t strlen(const char *s) {
  size_t i;
  for(i=0; *s!='\0'; i++, s++);
  return i;
}

size_t strnlen(const char *s, size_t n) {
  size_t i;
  for(i=0; i<n && *s!='\0'; i++, s++);
  return i;
}

int strcmp(const char *s1, const char *s2) {
  while(*s1 && *s1 == *s2) {
    s1++; s2++;
  }
  return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  for(size_t i=0; i<n && *s1 && (*s1==*s2); i++, s1++, s2++);
  return *s1 - *s2;
}

char *strdup(const char *str) {
  size_t size = strlen(str) + 1;
  char *copied = malloc(size);
  strncpy(copied, str, size);
  return copied;
}

void *memcpy(void *dest, const void *src, size_t n) {
  for(size_t i=0; i<n; i++)
    *(u8 *)dest++ = *(u8 *)src++;
  return dest;
}

void bzero(void *s, size_t n) {
  u8 *ptr = s;
  while(n--)
    *ptr++ = 0;
}

void *memset(void *s, int c, size_t n) {
  for(size_t i=0; i<n; i++)
    *(u8 *)s++ = (u8)c;
  return s;
}

void show_line() {
  puts("-------------------");
}

void show_number(u32 num) {
  printf("%x", num);
}
