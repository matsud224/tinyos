#include <kern/chardev.h>
#include <kern/kernlib.h>
#include <kern/thread.h>

#define PTS_NUM         32
#define PTS_BUFSIZE     128
#define PTS_PAIR(minor) ((minor)&1?(minor)-1:(minor)+1)

static int pts_open(int minor);
static int pts_close(int minor);
static int pts_read(int minor, char *dest, size_t count);
static int pts_write(int minor, const char *src, size_t count);
static struct chardev_state *pts_getstate(int minor);

static const struct chardev_ops pts_chardev_ops = {
  .open = pts_open,
  .close = pts_close,
  .read = pts_read,
  .write = pts_write,
  .getstate = pts_getstate,
};

static int pts_MAJOR;
static mutex pts_mutex;

struct pts {
  int is_used;
  struct chardev_buf *rxbuf;
  struct chardev_state state;
} pts[PTS_NUM];
//pairs are 0&1, 2&3, 4&5, ...

DRIVER_INIT void pts_init() {
  mutex_init(&pts_mutex);

  pts_MAJOR = chardev_register(&pts_chardev_ops);
  if(pts_MAJOR < 0) {
    puts("pts: failed to register");
    return;
  }
  printf("pts: major number = 0x%x\n", pts_MAJOR);

  for(int i=0; i<PTS_NUM; i++) {
    pts[i].is_used = 0;
    pts[i].rxbuf = NULL;
    chardev_initstate(&pts[i].state, CDMODE_CANON | CDMODE_ECHO);
  }
}

static int pts_check_minor(int minor) {
  if(minor < 0 || minor >= PTS_NUM)
    return -1;
  else
    return 0;
}

static int pts_open(int minor) {
  if(pts_check_minor(minor) || !pts[minor].is_used)
    return -1;

  if(pts[minor].rxbuf == NULL)
    pts[minor].rxbuf = cdbuf_create(malloc(PTS_BUFSIZE), PTS_BUFSIZE);

  if(pts[PTS_PAIR(minor)].rxbuf == NULL)
    pts[PTS_PAIR(minor)].rxbuf = cdbuf_create(malloc(PTS_BUFSIZE), PTS_BUFSIZE);

  pts[minor].is_used = 1;
  pts[PTS_PAIR(minor)].is_used = 1;

  printf("pts: minor %d,%d opened.\n", minor, PTS_PAIR(minor));

  return 0;
}

static int pts_close(int minor) {
  if(pts_check_minor(minor))
    return -1;

  pts[minor].is_used = 0;
  pts[PTS_PAIR(minor)].is_used = 0;

  thread_wakeup(&pts_chardev_ops);

  return 0;
}

static int pts_read(int minor, char *dest, size_t count) {
  if(pts_check_minor(minor) || !pts[minor].is_used)
    return -1;

  int n = cdbuf_read(pts[minor].rxbuf, dest, count);
  thread_wakeup(&pts_chardev_ops);
  return n;
}

static int pts_write(int minor, const char *src, size_t count) {
  if(pts_check_minor(minor) || !pts[minor].is_used)
    return -1;

  int n = cdbuf_write(pts[PTS_PAIR(minor)].rxbuf, src, count);
  thread_wakeup(&pts_chardev_ops);
  return n;
}

static struct chardev_state *pts_getstate(int minor) {
  if(pts_check_minor(minor))
    return -1;

  return &pts[minor].state;
}

//minor number n(return value) and n+1 are pair terminal
int pts_get() {
  mutex_lock(&pts_mutex);
  for(int i=0; i<PTS_NUM; i+=2) {
    if(!pts[i].is_used) {
      mutex_unlock(&pts_mutex);
      return i;
    }
  }
  mutex_unlock(&pts_mutex);
  return -1;
}
