#include <kern/kernlib.h>
#include <stdint.h>
#include <stddef.h>

int abs(int n) {
  return n<0?-n:n;
}

char *strncpy(char *dest, const char *src, size_t n) {
  for(size_t i=0; i<n; i++) {
    *dest++ = *src++;
    if(*src == '\0') break;
  }
  return dest;
}

size_t strnlen(const char *s, size_t n) {
  size_t i;
  for(i=0; i<n && *s!='\0'; i++, s++);
  return i;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  for(size_t i=0; i<n && *s1 && (*s1==*s2); i++, s1++, s2++);
  return *s1 - *s2;
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
