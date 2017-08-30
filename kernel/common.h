#pragma once

#define PACKED __attribute__ ((packed))
#define KERNENTRY __attribute__ ((section (".entry")))

#define ALIGN(a, m) (((a)+(m)-1)/(m)*(m))
