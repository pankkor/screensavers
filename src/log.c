// mmapped log file
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/log

#include "common.h"

// Mmapped log file. Inspired by Timothy Lottes
// mmapped file io. Writes fixed size lines in a ring buffer fashion.
// At the end of a file there is a 32-bit atomic that marks current
// run number and currently written line.
// Line size -> 128 (cache line size)
enum {
  LOG_BYTES        = 65536, // power of 2, multiple of page size
  LOG_LINE_BYTES   = 128,   // cache size to avoid false sharing
  LOG_LINES        = LOG_BYTES / LOG_LINE_BYTES,  // power of 2
  LOG_ATOMIC_OFF   = 65536,                       // atomic offset
  // TODO:
  LOG_LINE_A_OFF   = 65536,                  // Line number atomic
  LOG_RELOAD_A_OFF = 65536 + LOG_LINE_BYTES, // Reload count atomic
};

u8  *s_log_buf;
u32 *s_log_atomic; // 0xrrrrllll: 0xrrrr - restart count, 0xllll - line number
// TODO:
u32 *s_log_line_a;      // 0xllll - log line number
u32 *s_log_reload_a;    // 0xrrrr - restart count
u16 s_log_this_reload;  // restart number of current run
f32 s_log_tsc_ifreq;
u64 s_log_start_tsc;

static void log_init(const char *filepath, u64 start_tsc, f32 tsc_ifreq) {
  i32 fd = sys_open(filepath, O_RDWR | O_CREAT, 0644);
  i32 log_size = LOG_BYTES + 4;

  EXPECT(fd >= 0, "Log: mmap failed");
  EXPECT(sys_ftruncate(fd, log_size) >= 0, "Log: ftruncate failed");
  if (fd >= 0 ) {
    void *p = sys_mmap(0, log_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    sys_close(fd);
    EXPECT(p >= (void *)p, "Log: mmap file failed");
    s_log_buf = p;
  }

  s_log_tsc_ifreq = tsc_ifreq;
  s_log_start_tsc = start_tsc;
  // TODO: atomic
  s_log_line_a = (u32 *)(s_log_buf + LOG_LINE_A_OFF);
  s_log_reload_a = (u32 *)(s_log_buf + LOG_RELOAD_A_OFF);
  s_log_this_reload = fetch_add_i32((i32 *)s_log_reload_a, 1) & 0xF;
}

static void log_shutdown(void) {
  EXPECT(sys_munmap(s_log_buf, LOG_BYTES) > 0, "Log: shutdown failed");
}

// Log i64 `v` and message `m` to a 128 byte wide line in a memory mapped file
// r|sss.uuuuuu|ffffffffffffffff:llll|hhhhhhhh|iiiiiiiiiii|mmm..mmm\n
// r - restart count in hex
// s - seconds
// u - microseconds
// f - file
// l - line
// h - v in hex form
// i - v in i64 form
// m - message (71 chars)
#define LOG_M(v, m) log_m(__FILE_NAME__, __LINE__, (v), (m))

static void log_m(const char* filename, i32 file_line, i32 v, const char* m) {
  enum { FILL_C = '.' };  // fill empty space with this char
  u8 tmp[LOG_LINE_BYTES]; // cache line

  u64 tsc = read_cpu_timer() - s_log_start_tsc;
  f32 sec_f32 = tsc * s_log_tsc_ifreq;
  i32 sec = sec_f32;
  i32 usec = (sec_f32 - sec) * 1000000;

  // Fill in cache line buffer
  i32 i = 0;


  // r| - restart count in hex
  tmp[i + 0] = s_hex[s_log_this_reload];
  tmp[i + 1] = '|';
  i += 2;

  // sss.uuuuuu| - time since start
  // sss. - seconds since start
  u32 s = sec % 1000; // warp to 999 sec
  s = u32_to_a1d_(tmp + i + 2, s);
  s = u32_to_a1d_(tmp + i + 1, s);
  s = u32_to_a1d_(tmp + i + 0, s);
  tmp[i + 3] = '.';
  i += 4;

  // uuuuuu| - microseconds since start
  u32 u = usec;
  u = u32_to_a1d_(tmp + i + 5, u);
  u = u32_to_a1d_(tmp + i + 4, u);
  u = u32_to_a1d_(tmp + i + 3, u);
  u = u32_to_a1d_(tmp + i + 2, u);
  u = u32_to_a1d_(tmp + i + 1, u);
  u = u32_to_a1d_(tmp + i + 0, u);
  tmp[i + 6] = '|';
  i += 7;

  // ffffffffffffffff: - filename
  i32 filename_size = cstr_n_copy(tmp + i, filename, 16);
  buf_fill((u8 *)tmp + i + filename_size, 16 - filename_size , FILL_C);
  tmp[i + 16] = ':';
  i += 17;

  // llll| - line in file
  u32 l = file_line % 10000;
  l = u32_to_a1d_(tmp + i + 3, l);
  l = u32_to_a1d_(tmp + i + 2, l);
  l = u32_to_a1d_(tmp + i + 1, l);
  l = u32_to_a1d_(tmp + i + 0, l);
  fmt_subs_leading_zeroes(tmp + i, 4, FILL_C);
  tmp[i + 4] = '|';
  i += 5;

  // hhhhhhhh| - hex
  u32_to_a8x(tmp + i, v);
  tmp[i + 8] = '|';
  i += 9;

  // iiiiiiiiiii| - i32
  i32_to_a11_fmt_right(tmp + i, v, FILL_C);
  tmp[i + 11] = '|';
  i += 12;

  i += cstr_n_copy(tmp + i, m, LOG_LINE_BYTES - i - 1);
  buf_fill(tmp + i, LOG_LINE_BYTES - i - 1, FILL_C);
  tmp[LOG_LINE_BYTES - 1] = '\n';

  // atomic and IO
  i32 log_line = fetch_add_i32((i32 *)s_log_line_a, 1) & (LOG_LINES - 1);
  u8 *dst = s_log_buf + log_line * LOG_LINE_BYTES;
  mem_cp_aligned32(dst, tmp, 128);
}

void start(void) {
  u64 start_tsc = read_cpu_timer();
  f32 tsc_ifreq = 1.0f / read_cpu_timer_freq();

  print_cstr(STDOUT, "Writing log to 'log.log'\n");
  print_cstr(STDOUT, "\n");
  print_cstr(STDOUT, "<Press Ctrl+C to exit>\n");

  log_init("log.log", start_tsc, tsc_ifreq);
  while (1) {
    LOG_M(0xdeadbeef, "hello! How are you? I'm fine and you? What about emoji? 1234567890 12345678790 1234567890 1234567890");
    LOG_M(0x0, "");
    LOG_M(-1234567, "Number: -123456");
    LOG_M(-1234567, "Number: -123456");
    log_m("test_blablababa.abcdef", 9, 9, "9");
    log_m("test_blablababa.abcdef", 99, 19, "99");
    log_m("test_blablababa.abcdef", 999, 199, "999");
    log_m("test_blablababa.abcdef", 9999, 1999, "9999");
    log_m("test_1laba.def", 99, 19, "99");
    log_m("test_2lba.aef", 999, 199, "999");
    log_m("test_3la.aef", 9, 9, "9");
    log_m("test_4l.abf", 9999, 1999, "9999");
  }

  log_shutdown();
  exit(0);
}
