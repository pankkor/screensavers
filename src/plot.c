// Draw some plots
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/log

#include "common.h"

// --------------------------------------
// GLSL
// --------------------------------------
static const char * const s_plot_vert_src = "                                  \
#version 410 core                                                              \
layout(location = 0) in vec2 v_point;                                          \
                                                                               \
out float v_id;                                                                \
                                                                               \
void main() {                                                                  \
  v_id = gl_VertexID;                                                          \
  gl_Position = vec4(2.0 * v_point.x - 1.0, v_point.y * 2.0 - 1.0, 0.0, 1.0);  \
}                                                                              \
";

static const char * const s_plot_frag_src = "                                  \
#version 410 core                                                              \
                                                                               \
in float v_id;                                                                 \
out vec4 frag_col;                                                             \
                                                                               \
void main() {                                                                  \
  frag_col = vec4(0.0, v_id / 1024, 0.0 , 1.0);                                \
}                                                                              \
";

enum {
  PLOT_POINTS_COUNT = 1024,
};

struct plot {
  // Ring buffer of points
  f32 points[2 * PLOT_POINTS_COUNT]; // 0 (x-axis) - time; 1 (y-axis) - value
  u32 end;  // last index in points
};

void start(void) {
  // Init
  u64 start_tsc = read_cpu_timer();
  f32 tsc_ifreq = 1.0f / read_cpu_timer_freq();
  log_init(&g_log, "plot.log", start_tsc, tsc_ifreq);

  struct event_loop loop;
  struct window w;

  event_loop_init(&loop);
  window_init(&w, 0 /*is_full_screen*/);

  const GLubyte* version_cstr = glGetString(GL_VERSION);
  print_cstr(STDOUT, "OpenGL version: \n");
  print_cstr(STDOUT, (const char *)version_cstr);
  print_cstr(STDOUT, "\n\n"
      "Press <ESC> to exit.\n"
      "Press <SPACE> to pause/resume at current frame.\n"
      "When paused, press <RIGHT> to advance to the next frame.\n");

  GLuint plot_prog = create_gl_shader_program(
    s_plot_vert_src,
    s_plot_frag_src
  );

  f32 t_curve0 = 0.0f;
  struct plot plot0 = {0};

  for (i32 i = 0; i < PLOT_POINTS_COUNT; ++i) {
    plot0.points[i * 2 + 0] = (f32)i / (PLOT_POINTS_COUNT - 1);
    plot0.points[i * 2 + 1] = sinf32((f32)i / (PLOT_POINTS_COUNT - 1));
  }

  GLuint vao;
  GLuint points_bo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &points_bo);

  glUseProgram(plot_prog);
  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, points_bo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(plot0.points), plot0.points,
      GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  // TODO: instancing
  // glVertexAttribDivisor(1, 1);
  glEnableVertexAttribArray(0);

  // Game loop
  f32 cpu_timer_freq  = read_cpu_timer_freq();
  f32 icpu_timer_freq = 1.0f / cpu_timer_freq;
  u64 tsc             = read_cpu_timer();

  f32 loop_s          = 0.0f;
  u64 loop_count      = 0;
  f32 print_dt_tsc    = tsc + 5.0f * cpu_timer_freq;

  b32 debug_frame_mode = 0;

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
    u64 new_tsc     = read_cpu_timer();
    f32 dt          = (new_tsc - tsc) * icpu_timer_freq;
    tsc             = new_tsc;
    loop_s          += dt;
    loop_count      += 1;

#if 1 // Stop at current frame. Advance 1 frame on Space press
    b32 is_space_up = keycode_is_up(KC_SPACE, &old_kcs, &kcs);
    b32 is_right_up = keycode_is_up(KC_RIGHT, &old_kcs, &kcs);

    if (is_space_up) {
      debug_frame_mode = !debug_frame_mode;
      print_cstr(STDOUT, "Debug frame mode: ");
      print_cstr(STDOUT, debug_frame_mode ? "On\n" : "Off\n");
    }

    if (debug_frame_mode && !is_right_up) {
      continue;
    }
#endif

#if 1 // Print average tick time (print could block io)
    if (tsc > print_dt_tsc) {
      print_avg_dt_fps(loop_s / loop_count);

      loop_s        = 0.0f;
      loop_count    = 0;
      print_dt_tsc  = tsc + 5.0f * cpu_timer_freq;
    }
#else
    (void)loop_s;
    (void)loop_count;
    (void)print_dt_tsc;
#endif

    // Update plots
    t_curve0 = fmodf32(t_curve0 + dt, 1.0f);

    plot0.end = (plot0.end + 1) % PLOT_POINTS_COUNT;
    plot0.points[plot0.end * 2 + 0] = (f32)plot0.end / (PLOT_POINTS_COUNT - 1);
    plot0.points[plot0.end * 2 + 1] = clampf32(dt * 100.0f, 0.0f, 1.0f);

    // Draw
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.8f, 0.8f, 0.8f, 0.8f);

    // Draw plots
    glBindBuffer(GL_ARRAY_BUFFER, points_bo);
    // TODO, only update 1 point
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(plot0.points), plot0.points);
    glDrawArrays(GL_LINE_STRIP, 0, PLOT_POINTS_COUNT);

    // glDrawArrays(GL_LINE_STRIP, 0, plot0.end);
    // glDrawArraysInstanced(GL_LINE_STRIP, 0, 2, PLOT_POINTS_COUNT);

    window_flush(&w);
  }

shutdown:
  print_avg_dt_fps(loop_s / loop_count);

  // Shutdown
  glDeleteShader(plot_prog);

  glDeleteBuffers(1, &points_bo);
  glDeleteVertexArrays(1, &vao);

  window_shutdown(&w);
  event_loop_shutdown(&loop);

  log_shutdown(&g_log);

  exit(0);
}
