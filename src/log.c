// mmaped log file
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/log

#include "common.h"

// Mmaped log file. Inspired by Timothy Lottes
// mmaped file io. We write fixed size lines only to the first half of the file.
// Line size -> 128 (cache line size)
enum {
  L_BYTES        = 65536, // power of 2, multiple of page size
  L_LINE_BYTES   = 128,
  L_LINES        = L_BYTES / L_LINE_BYTES,
  L_ATOMIC_OFF   = 65536, // Beginning of the second half of the file
};

u8 *s_l_buf;
u16 s_atomic_line;

#define O_CREAT         0x00000200      /* create if nonexistant */
#define O_RDONLY        0x0000          /* open for reading only */
#define O_RDWR          0x0002          /* open for reading and writing */

void l_init(const char *filepath) {
  int fd = sys_open(filepath, O_RDWR | O_CREAT, 0644);
  EXPECT(sys_ftruncate(fd, L_BYTES) >= 0, "Log: ftruncate failed");
  if (fd >= 0 ) {
    void *p = sys_mmap(0, L_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    sys_close(fd);
    EXPECT(p >= (void *)p, "Log: mmap file failed");
    s_l_buf = p;
  }
}

void l_shutdown(void) {
  EXPECT(sys_munmap(s_l_buf, L_BYTES) > 0, "Log: shutdown failed");
}

void l_log(const char* m) {
  // TODO atomic
  s_atomic_line = (s_atomic_line + 1) & 0x1FF;
  // TODO ineficcient
  i32 len = MIN(cstr_len(m), L_LINE_BYTES);
  u8 *line = s_l_buf + s_atomic_line * L_LINE_BYTES;

  i32 i = 0;
  for (; i < len; ++i) {
    line[i] = m[i];
  }
  for (; i < L_LINE_BYTES - 1; ++i) {
    line[i] = ' ';
  }
  line[L_LINE_BYTES - 1] = '\n';
}

// --------------------------------------
// Entry point (aka main)
// --------------------------------------
void start(void) {
  print_cstr(STDOUT, "\n\n");
  print_cstr(STDOUT, "<Press Ctrl+C to exit>\n");

  l_init("log.log");
  while (1) {
    l_log("hello!");
  }

  l_shutdown();
  exit(0);
}
