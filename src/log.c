// mmapped circular log file
// Creates logfile 'log.log' and writes to it.
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/log

#include "common.h"

void start(void) {
  u64 start_tsc = read_cpu_timer();
  f32 tsc_ifreq = 1.0f / read_cpu_timer_freq();

  print_cstr(STDOUT, "Writing log to 'log.log...'\n");
  print_cstr(STDOUT, "\n");
  print_cstr(STDOUT, "<Press Ctrl+C to exit>\n");

  log_init(&g_log, "log.log", start_tsc, tsc_ifreq);

  // Create text that will be printed to the log. Fill it with a pattern.
  u8 luminance[12] = ".,-~:;=!*#$@"; // don't keep null terminator
  enum { TEXT_W = LOG_MESSAGE_SIZE, TEXT_H = 1024 };
  u8 msg[TEXT_W * TEXT_H];

  f32 bg_anim_t = 0.0f;
  for (i32 x = 0; x < TEXT_W; ++x) {
    for (i32 y = 0; y < TEXT_H; ++y) {
      f32 l = 0.25f * cosf32(bg_anim_t + 0.02f * x) + 0.25f;
      f32 k = 0.25f * sinf32(bg_anim_t + 0.02f * y) + 0.25f;
      i32 idx = (l + k) * (ARRAY_COUNT(luminance) - 1);
      msg[x + TEXT_W * y] = luminance[idx];
    }
  }

  i32 cur_text_line = 0;
  while (1) {
    cur_text_line = (cur_text_line + 1) % TEXT_H;
    LOG_M(cur_text_line, (const char*)(msg + TEXT_W * cur_text_line));
  }

  log_shutdown(&g_log);
  exit(0);
}
