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
u16 s_l_atomic_line;
f32 s_l_tsc_ifreq;
u64 s_l_start_tsc;

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

  s_l_tsc_ifreq = 1.0f / read_cpu_timer_freq();
  s_l_start_tsc = read_cpu_timer();
}

void l_shutdown(void) {
  EXPECT(sys_munmap(s_l_buf, L_BYTES) > 0, "Log: shutdown failed");
}

/* void l_log(const char* m) { */
/*   s_l_atomic_line = (s_l_atomic_line + 1) & 0x1FF; */
/*  */
/*   // sss.uuuuuu| */
/*   i32 reload_count = 1; */
/*   i32 time_since_start=123456789; */
/*   // const char *file = __FILE__; */
/*   char file[16] = __FILE__; */
/*   i32 ln_number = 3; */
/*   i32 v=0xd3adbeef; */
/*  */
/*   u64 tsc = read_cpu_timer() - s_l_start_tsc; */
/*   f32 sec = tsc * s_l_tsc_ifreq; */
/*  */
/*   // IO */
/*   u8 *dst = s_l_buf + s_l_atomic_line * L_LINE_BYTES; */
/*   // sec.us */
/*   u32_to_a10(dst, v); */
/*   u32_to_a8x(dst, v); */
/*   u32_to_a8x(dst, v); */
/*  */
/*   u32_to_a8x(dst, v); */
/*   dst[8] = '|'; */
/*  */
/*   // TODO ineficcient */
/*   i32 i = 9; */
/*  */
/*   i32 r = MIN(i + cstr_len(m), L_LINE_BYTES); */
/*  */
/*   for (; i < r; ++i) { */
/*     dst[i] = m[i]; */
/*   } */
/*   for (; i < L_LINE_BYTES - 1; ++i) { */
/*     dst[i] = '_'; */
/*   } */
/*   dst[L_LINE_BYTES - 1] = '\n'; */
/*  */
/*   // Grab the line */
/*   // TODO atomic */
/*   // s_l_atomic_line = (s_l_atomic_line + 1) & 0x1FF; */
/*  */
/*   [> for (i32 i = 0; i < L_LINE_BYTES / 8; i += 8) { <] */
/*   [>   dst[i] = line[i]; <] */
/*   [> } <] */
/* } */

void start(void) {
  {
  u8 buf[12]; buf[11] = '\n';
  i32_to_a11(buf, 2147483647);
  print_buf(STDOUT, (const char *)buf, ARRAY_COUNT(buf));

  /* i32_to_a11(buf, abs32(-2147483647)); */
  u32_to_a10(buf, absi32(-2147483648));
  print_buf(STDOUT, (const char *)buf, ARRAY_COUNT(buf));
  }

  {
  u8 buf[21]; buf[20] = '\n';
  i64_to_a20_fmt_right(buf, 9223372036854775807, '_');
  print_buf(STDOUT, (const char *)buf, ARRAY_COUNT(buf));

  i32 x3 = -2147483648;
  (void)x3;

  i64_to_a20_fmt_right(buf, I64_MIN, '_');
  /* i64_to_a20_fmt_right(buf, -1234567, '_'); */
  /* u64_to_a20_fmt_right(buf, 0xFFFFFFFFFFFFFFFF, '_'); */
  /* print_buf(STDOUT, (const char *)buf, ARRAY_COUNT(buf)); */
  }

  print_cstr(STDOUT, "\n\n");
  exit(1);
  print_cstr(STDOUT, "<Press Ctrl+C to exit>\n");

  l_init("log.log");
  while (1) {
  /*   l_log("hello!"); */
  }
  l_shutdown();
  exit(0);
}
