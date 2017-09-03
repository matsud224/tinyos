#pragma once

#define INLINE __inline__
#define KERNENTRY __attribute__ ((section (".entry")))
#define PACKED __attribute__ ((packed))
#define ASM __asm__ __volatile__
#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))
