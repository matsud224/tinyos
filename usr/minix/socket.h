#include <sys/types.h>
#include <inttypes.h>

#ifdef ENDIAN_BE
#define hton16(val) val
#define ntoh16(val) val
#define hton32(val) val
#define ntoh32(val) val

#else
#define hton16(val) ((uint16_t) ( \
    ((val) << 8) | ((val) >> 8) ))

#define ntoh16(val) ((uint16_t) ( \
    ((val) << 8) | ((val) >> 8) ))

#define hton32(val) ((uint32_t) ( \
    (((val) & 0x000000ff) << 24) | \
    (((val) & 0x0000ff00) <<  8) | \
    (((val) & 0x00ff0000) >>  8) | \
    (((val) & 0xff000000) >> 24) ))

#define ntoh32(val) ((uint32_t) ( \
    (((val) & 0x000000ff) << 24) | \
    (((val) & 0x0000ff00) <<  8) | \
    (((val) & 0x00ff0000) >>  8) | \
    (((val) & 0xff000000) >> 24) ))
#endif // ENDIAN_BE

#define PF_LINK			0
#define PF_INET			1
#define MAX_PF			2

#define SOCK_STREAM			0
#define SOCK_DGRAM			1
#define MAX_SOCKTYPE		2

struct sockaddr {
  uint8_t len;
  uint8_t family;
  uint8_t addr[];
};

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

#define INADDR_ANY 0

#ifdef ENDIAN_BE
#define IPADDR(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
#else
#define IPADDR(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#endif

struct sockaddr_in {
  uint8_t len;
  uint8_t family;
  in_port_t port;
  in_addr_t addr;
};


int socket(int domain, int type);
int bind(int fd, const struct sockaddr *addr);
int sendto(int fd, const char *msg, size_t len, int flags, const struct sockaddr *to_addr);
int recvfrom(int fd, char *buf, size_t len, int flags, struct sockaddr *from_addr);
int connect(int fd, const struct sockaddr *to_addr);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr *client_addr);
int send(int fd, const char *msg, size_t len, int flags);
int recv(int fd, char *buf, size_t len, int flags);
