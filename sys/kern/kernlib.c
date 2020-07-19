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
  size_t i;
  for(i=0; i<n && *s1 && (*s1==*s2); i++, s1++, s2++);
  return (i!=n)?(*s1 - *s2):0;
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

int memcmp(const void *s1, const void *s2, size_t n) {
  u8 *p1 = (u8 *)s1;
  u8 *p2 = (u8 *)s2;

  while(n-- != 0 && *p1 == *p2) {
    p1++; p2++;
  }
  return *p1 - *p2;
}

void *memmove(void *dest, const void *src, size_t n) {
  if (src + n > dest) {
    src += n - 1;
    dest += n - 1;
    for(size_t i=0; i<n; i++)
      *(u8 *)dest-- = *(u8 *)src--;
  } else {
    memcpy(dest, src, n);
  }

  return dest;
}

void *memchr(const void *s, int c, size_t n) {
  u8 *p = (u8 *)s;
  for (size_t i=0; i<n; i++, p++) {
    if (*p == c)
      return p;
  }
  return NULL;
}

char *strchr(const char *s, int c) {
  char *p = (char *)s;

  while(*p != '\0' && *p != c)
    p++;

  if (*p == '\0')
    return NULL;
  else
    return p;
}

char *strcpy(char *dest, const char *src) {
  do {
    *dest++ = *src;
  } while (*src++ != '\0');
  return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
  size_t destlen = strlen(dest);
  size_t i;

  for (i = 0; i < n; i++) {
    dest[destlen + i] = src[i];
  }

  dest[destlen + i] = '\0';
  return dest;
}

char *strcat(char *dest, const char *src) {
  size_t destlen = strlen(dest);
  strcpy(dest + destlen, src);
  return dest;
}

int isspace(int c) {
  return (c == ' ' || c == '\f' || c == '\n' ||
          c == '\r' || c == '\t' || c == '\v');
}

int tolower(int c) {
  if (c >= 'A' && c <= 'Z')
    return c + ('a' - 'A');
  else
    return c;
}

static int isvalidnumchar(char c, int base) {
  c = tolower(c);

  if (c >= '0' && c <= '9') {
    return (c - '0') < base;
  } else if (c >= 'a' && c <= 'z') {
    return ((c - 'a') + 10) < base;
  }

  return 0;
}

#define ULONG_MAX (~(unsigned long)0)

unsigned long int strtoul(const char *nptr, char **endptr, int base) {
  const char *p = nptr;
  int is_minus = 0;
  unsigned long result = 0;

  while(isspace(*p)) p++;

  if (*p == '-') {
    is_minus = 1;
    p++;
  } else if (*p == '+') {
    p++;
  }

  if (*p == '0' && *(p + 1) == 'x') {
    if (base == 0 || base == 16)
      base = 16;
    else
      goto error;

    p += 2;
  } else if (*p == '0') {
    if (base == 0)
      base = 8;
    else if (base != 10)
      goto error;

    p += 1;
  } else {
    if (base == 0)
      base = 10;
  }

  while (isvalidnumchar(*p, base)) {
    unsigned long temp = (result * base) + *p;
    if (result > temp)
      result = ULONG_MAX;
    else
      result = temp;

    p++;
  }

  if (is_minus) {
    result = (unsigned long)(-(long)result);
  }

error:
  if (endptr != NULL)
    *endptr = p;

  return result;
}

void show_line() {
  puts("-------------------");
}

void show_number(u32 num) {
  printf("%x", num);
}

void abort_for_mrb(void);
void abort(void) {
  abort_for_mrb();
}

void exit_for_mrb(int);
void exit(int status) {
  exit_for_mrb(status);
}
