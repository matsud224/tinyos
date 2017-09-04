#include <stdint.h>
#include <stddef.h>

void malloc_init(void);
void *malloc(size_t request);
void free(void *addr);

