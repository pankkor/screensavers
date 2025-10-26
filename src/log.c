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
  LOG_BYTES        = 65536, // power of 2, multiple of page size
  LOG_LINE_BYTES   = 128,
  LOG_LINES        = LOG_BYTES / LOG_LINE_BYTES,
  LOG_ATOMIC_OFF   = 65536, // Beginning of the second half of the file
};

u8 *s_log_buf;
u16 s_log_atomic_line;
f32 s_log_tsc_ifreq;
u64 s_log_start_tsc;

static void log_init(const char *filepath) {
  int fd = sys_open(filepath, O_RDWR | O_CREAT, 0644);
  EXPECT(fd >= 0, "Log: mmap failed");
  EXPECT(sys_ftruncate(fd, LOG_BYTES) >= 0, "Log: ftruncate failed");
  if (fd >= 0 ) {
    void *p = sys_mmap(0, LOG_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    sys_close(fd);
    EXPECT(p >= (void *)p, "Log: mmap file failed");
    s_log_buf = p;
  }

  s_log_tsc_ifreq = 1.0f / read_cpu_timer_freq();
  s_log_start_tsc = read_cpu_timer();
}

static void log_shutdown(void) {
  EXPECT(sys_munmap(s_log_buf, LOG_BYTES) > 0, "Log: shutdown failed");
}

// Log integer `v` and message `m` to a memory maped file
// r|sss.uuuuuu|ffffffffffffffff:llll|hhhhhhhh|iiiiiiiiiii|mmm..mmm\n
// r - reload count
// s - seconds
// u - microseconds
// f - file
// l - line
// h - v in hex form
// i - v in int form
// m - message
static void log_m(i32 v, const char* m) {
  s_log_atomic_line = (s_log_atomic_line + 1) & 0x1FF;

  // sss.uuuuuu|
  u32 reload_count = 1556;
  u64 start_ns=1987123456789;
  /* i32 tid=33; // thread id */
  // const char *file = __FILE__;
  char file[16] = {0};
  {
    i32 file_i = cstr_n_copy(file, __FILE__, 16);
    buf_fill((u8 *)file + file_i, 16 - file_i, '_');
  }
  /* i32 ln_number = 3; */

  /* u64 tsc = read_cpu_timer() - s_log_start_tsc; */
  /* f32 sec = tsc * s_log_tsc_ifreq; */

  // IO
  i32 i = 0;
  u8 *dst = s_log_buf + s_log_atomic_line * LOG_LINE_BYTES;

  // r| - reoload count
  u32 r = reload_count % 10;
  r = u32_to_a1d_(dst + i + 0, r);
  dst[i + 1] = '|';
  i += 2;

  // sss.uuuuuu| - time since start
  start_ns = start_ns % 1000000000000; // wrap to 999 sec

  // sss. - seconds since start
  u32 s = start_ns / 1000 / 1000 / 1000 ; // sec
  s = u32_to_a1d_(dst + i + 2, s);
  s = u32_to_a1d_(dst + i + 1, s);
  s = u32_to_a1d_(dst + i + 0, s);
  dst[i + 3] = '.';
  i += 4;

  // uuuuuu| - microseconds since start
  u32 u = start_ns / 1000;                // usec
  u = u32_to_a1d_(dst + i + 5, u);
  u = u32_to_a1d_(dst + i + 4, u);
  u = u32_to_a1d_(dst + i + 3, u);
  u = u32_to_a1d_(dst + i + 2, u);
  u = u32_to_a1d_(dst + i + 1, u);
  u = u32_to_a1d_(dst + i + 0, u);
  dst[i + 6] = '|';
  i += 7;

  // ffffffffffffffff| - file
  // TODO: memcpy16
  dst[i + 0] = file[ 0]; dst[i + 1] = file[ 1]; dst[i + 2] = file[ 2]; dst[i + 3] = file[ 3];
  dst[i + 4] = file[ 4]; dst[i + 5] = file[ 5]; dst[i + 6] = file[ 6]; dst[i + 7] = file[ 7];
  dst[i + 8] = file[ 8]; dst[i + 9] = file[ 9]; dst[i +10] = file[10]; dst[i +11] = file[11];
  dst[i +12] = file[12]; dst[i +13] = file[13]; dst[i +14] = file[14]; dst[i +15] = file[15];
  dst[i +16] = ':';
  i += 17;

  u32 l = __LINE__ % 1000;
  l = u32_to_a1d_(dst + i + 2, l);
  l = u32_to_a1d_(dst + i + 1, l);
  l = u32_to_a1d_(dst + i + 0, l);
  dst[i + 3] = '|';
  i += 4;

  // hhhhhhhh| - hex
  u32_to_a8x(dst + i, v);
  dst[i + 8] = '|';
  i += 9;

  // iiiiiiiiiii| - i32
  i32_to_a11(dst + i, v);
  dst[i + 11] = '|';
  i += 12;

  i += cstr_n_copy((char *)dst + i, m, LOG_LINE_BYTES - 1);
  buf_fill(dst + i, LOG_LINE_BYTES - 1 - i, '_');
  dst[LOG_LINE_BYTES - 1] = '\n';
}

void start(void) {
  print_cstr(STDOUT, "\n\n");
  print_cstr(STDOUT, "<Press Ctrl+C to exit>\n");

  log_init(".log.log");
  while (1) {
    log_m(0xdeadbeef, "hello! how are you? I'm fine and you? What about emoji? 1234567890 12345678790 1234567890 1234567890");
  }

  log_shutdown();
  exit(0);
}
