// Draw timeline graphs in pixel shader
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/timelines

#include "common.h"

void start(void) {
  // Init
  u64 start_tsc = read_cpu_timer();
  f32 tsc_ifreq = 1.0f / read_cpu_timer_freq();
  log_init(&g_log, "timeline.log", start_tsc, tsc_ifreq);

  struct event_loop loop;
  struct window w;

  event_loop_init(&loop);
  window_init(&w, 0 /* vsync */, 0 /*is_full_screen*/);

  const GLubyte* version_cstr = glGetString(GL_VERSION);
  print_cstr(STDOUT, "OpenGL version: \n");
  print_cstr(STDOUT, (const char *)version_cstr);
  print_cstr(STDOUT, "\n\n"
      "Press <ESC> to exit.\n"
      "Press <SPACE> to pause/resume at current frame.\n"
      "When paused, press <RIGHT> to advance to the next frame.\n");

  u64 frame_num = 0;

  // Game loop
  f32 cpu_timer_freq  = read_cpu_timer_freq();
  f32 icpu_timer_freq = 1.0f / cpu_timer_freq;
  u64 tsc             = read_cpu_timer();

  f32 loop_s          = 0.0f;
  u64 loop_count      = 0;
  f32 print_dt_tsc    = tsc + 5.0f * cpu_timer_freq;

  b32 debug_frame_mode = 0;

  struct timelines_state timelines_state = {
    .resolution = { w.rect[2], w.rect[3] }
  };

  struct timelines_gpu timelines = {0};
  struct queries_gpu qs  = {0};

  timelines_gpu_init(&timelines);
  queries_gpu_init(&qs);

  while (1) {
    // Keyboard Input
    // Step through event loop once, updating input events
    struct keycodes old_kcs = loop.keycodes;
    event_loop_step(&loop);
    struct keycodes kcs = loop.keycodes;

    // ESC Down to exit
    if (kcs.e[KC_ESC]) {
      goto shutdown;
    }

    // dt bookkeeping
#if 1
    if (tsc > print_dt_tsc) {
      print_avg_dt_fps(loop_s / loop_count);

      loop_s        = 0.0f;
      loop_count    = 0;
      print_dt_tsc  = tsc + 5.0f * cpu_timer_freq;
    }
#endif
    u64 new_tsc     = read_cpu_timer();
    f32 dt          = (new_tsc - tsc) * icpu_timer_freq;
    tsc             = new_tsc;
    loop_s          += dt;
    loop_count      += 1;

    b32 is_up_space   = keycode_changed_to_up(  KC_SPACE, &old_kcs, &kcs);
    b32 is_up_right   = keycode_changed_to_up(  KC_RIGHT, &old_kcs, &kcs);
    b32 is_down_plus  = keycode_changed_to_down(KC_EQUAL, &old_kcs, &kcs);
    b32 is_down_minus = keycode_changed_to_down(KC_MINUS, &old_kcs, &kcs);
    b32 is_down_shift = keycode_changed_to_down(KC_SHIFT, &old_kcs, &kcs);

    // Zoom and scroll
    const f32 dzoom   = 0.1f;
    const f32 dscroll = 0.05f;
    if (is_down_shift) {
      // Control zoom with + and -
      if (is_down_minus) {
        timelines_state.dzoom -= dzoom;
      }
      if (is_down_plus) {
        timelines_state.dzoom += dzoom;

      }
    } else {
      // Control scroll with - and =
      if (is_down_minus) {
        timelines_state.scroll += dscroll;
      }
      if (is_down_plus) {
        timelines_state.scroll -= dscroll;
      }
    }
    timelines_state.current_frame_num = frame_num;
    timelines_state_bound(&timelines_state);

#if 1 // Stop at current frame. Advance 1 frame on Space press
    if (is_up_space) {
      debug_frame_mode = !debug_frame_mode;
      print_cstr(STDOUT, "Debug frame mode: ");
      print_cstr(STDOUT, debug_frame_mode ? "On\n" : "Off\n");
    }

    if (debug_frame_mode && !is_up_right) {
      continue;
    }
#endif

#define DT_MAX (1.0f / 100.0f)
    f32 sdt = dt / DT_MAX;
    struct timelines_update_data timelines_upd = {
      // Fake data
      .data       = {
        sdt,                                              // Scaled dt
        0,                                                // Draw time (query)
        loop_s / loop_count * 100,                        // Avg fps
        0,                                                // Zero
        (frame_num % 200) * 0.001,                        // Frame number
        0.5f + 0.5f * sinf32((frame_num % 1000) * 0.01),  // Sin
        (timelines_state.scroll + 1.0f),                  // Scroll
        (1.0f + timelines_state.dzoom) * 0.25f,           // Zoom
      },
      .frame_num  = frame_num,
    };

    // Draw
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.8f, 0.8f, 0.8f, 0.8f);
    glClear(GL_COLOR_BUFFER_BIT);

    queries_gpu_query_begin(&qs, frame_num);

    // Stream data to timelines
    timelines_gpu_batch_update(&timelines, &timelines_upd);
    timelines_gpu_partial_update_queries_gpu(&timelines, &qs, 1);

    timelines_gpu_draw(&timelines, &timelines_state);

    queries_gpu_query_end(&qs);

    window_flush(&w);

    queries_gpu_poll(&qs);

#if 1
    // Debug print qeury results
    if (frame_num % 16 == 0) {
      queries_gpu_debug_print(&qs, frame_num);
    }
#endif
    frame_num += 1;
  }

shutdown:
  print_avg_dt_fps(loop_s / loop_count);

  // Shutdown
  queries_gpu_shutdown(&qs);
  timelines_gpu_shutdown(&timelines);

  window_shutdown(&w);
  event_loop_shutdown(&loop);

  log_shutdown(&g_log);

  exit(0);
}
