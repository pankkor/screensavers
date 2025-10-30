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
// mmapped file io. We write fixed size lines only to the first half of the file.
// Line size -> 128 (cache line size)
enum {
  LOG_BYTES        = 65536, // power of 2, multiple of page size
  LOG_LINE_BYTES   = 128,
  LOG_LINES        = LOG_BYTES / LOG_LINE_BYTES,
  LOG_LINE_A_OFF   = 65536,                  // Line number atomic
  LOG_RELOAD_A_OFF = 65536 + LOG_LINE_BYTES, // Reload count atomic
};

u8  *s_log_buf;
u16 *s_log_line_a;      // 0xllll - log line number
u16 *s_log_reload_a;    // 0xrrrr - restart count
u16 s_log_this_reload;  // reload number of current run
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
  s_log_line_a = (u16 *)(s_log_buf + LOG_LINE_A_OFF);
  s_log_reload_a = (u16 *)(s_log_buf + LOG_RELOAD_A_OFF);
  s_log_this_reload = (++*s_log_reload_a) & 0xF;
}

static void log_shutdown(void) {
  EXPECT(sys_munmap(s_log_buf, LOG_BYTES) > 0, "Log: shutdown failed");
}

// Log i64 `v` and message `m` to a memory mapped file
// r|sss.uuuuuu|ffffffffffffffff:llll|hhhhhhhh|iiiiiiiiiii|mmm..mmm\n
// r - reload count in hex
// s - seconds
// u - microseconds
// f - file
// l - line
// h - v in hex form
// i - v in i64 form
// m - message
#define LOG_M(v, m) log_m(__FILE_NAME__, __LINE__, (v), (m))

static void log_m(const char* filename, i32 file_line, i32 v, const char* m) {
  i32 log_line = (++*s_log_line_a) & 0x1FF;

  u64 tsc = read_cpu_timer() - s_log_start_tsc;
  f32 sec_f32 = tsc * s_log_tsc_ifreq;
  i32 sec = sec_f32;
  i32 usec = (sec_f32 - sec) * 1000000;

  char file[16] = {0};
  {
    i32 file_i = cstr_n_copy(file, filename, 16);
    buf_fill((u8 *)file + file_i, 16 - file_i, ' ');
  }

  // IO
  i32 i = 0;
  u8 *dst = s_log_buf + log_line * LOG_LINE_BYTES;

  // r| - reload count in hex
  dst[i + 0] = s_hex[s_log_this_reload];
  dst[i + 1] = '|';
  i += 2;

  // sss.uuuuuu| - time since start
  // sss. - seconds since start
  u32 s = sec % 1000; // warp to 999 sec
  s = u32_to_a1d_(dst + i + 2, s);
  s = u32_to_a1d_(dst + i + 1, s);
  s = u32_to_a1d_(dst + i + 0, s);
  dst[i + 3] = '.';
  i += 4;

  // uuuuuu| - microseconds since start
  u32 u = usec;
  u = u32_to_a1d_(dst + i + 5, u);
  u = u32_to_a1d_(dst + i + 4, u);
  u = u32_to_a1d_(dst + i + 3, u);
  u = u32_to_a1d_(dst + i + 2, u);
  u = u32_to_a1d_(dst + i + 1, u);
  u = u32_to_a1d_(dst + i + 0, u);
  dst[i + 6] = '|';
  i += 7;

  // ffffffffffffffff: - file
  // TODO: memcpy16
  dst[i + 0] = file[ 0]; dst[i + 1] = file[ 1]; dst[i + 2] = file[ 2]; dst[i + 3] = file[ 3];
  dst[i + 4] = file[ 4]; dst[i + 5] = file[ 5]; dst[i + 6] = file[ 6]; dst[i + 7] = file[ 7];
  dst[i + 8] = file[ 8]; dst[i + 9] = file[ 9]; dst[i +10] = file[10]; dst[i +11] = file[11];
  dst[i +12] = file[12]; dst[i +13] = file[13]; dst[i +14] = file[14]; dst[i +15] = file[15];
  dst[i +16] = ':';
  i += 17;

  // llll| - line in file
  u32 l = file_line % 10000;
  l = u32_to_a1d_(dst + i + 3, l);
  l = u32_to_a1d_(dst + i + 2, l);
  l = u32_to_a1d_(dst + i + 1, l);
  l = u32_to_a1d_(dst + i + 0, l);
  dst[i + 4] = '|';
  i += 5;

  // hhhhhhhh| - hex
  u32_to_a8x(dst + i, v);
  dst[i + 8] = '|';
  i += 9;

  // iiiiiiiiiii| - i32
  i32_to_a11(dst + i, v);
  dst[i + 11] = '|';
  i += 12;

  i += cstr_n_copy((char *)dst + i, m, LOG_LINE_BYTES - i - 1);
  buf_fill(dst + i, LOG_LINE_BYTES - i - 1, ' ');
  dst[LOG_LINE_BYTES - 1] = '\n';
}

void start(void) {
  u64 start_tsc = read_cpu_timer();
  f32 tsc_ifreq = 1.0f / read_cpu_timer_freq();

  print_cstr(STDOUT, "\n\n");
  print_cstr(STDOUT, "<Press Ctrl+C to exit>\n");

  log_init(".log.log", start_tsc, tsc_ifreq);
  while (1) {
    LOG_M(0xdeadbeef, "hello! How are you? I'm fine and you? What about emoji? 1234567890 12345678790 1234567890 1234567890");
    LOG_M(0x0, "");
    LOG_M(-1234567, "Number: -123456");
  }

  log_shutdown();
  exit(0);
}
