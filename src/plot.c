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
layout(location = 0) in float v_point;                                         \
                                                                               \
uniform int  u_offset;                                                         \
uniform uint u_points_size_minus_one; /* power of 2 */                         \
                                                                               \
out vec3 col;                                                                  \
                                                                               \
void main() {                                                                  \
  uint v_id = (uint(gl_VertexID) + u_offset) & u_points_size_minus_one;        \
  float dx = 2.0 / u_points_size_minus_one;                                    \
  float x = -1.0 + dx * v_id;                                                  \
  gl_Position = vec4(x, v_point * 2.0 - 1.0, 0.0, 1.0);                        \
}                                                                              \
";

static const char * const s_plot_frag_src = "                                  \
#version 410 core                                                              \
                                                                               \
uniform vec3 u_col;                                                            \
                                                                               \
in vec3 col;                                                                   \
out vec4 frag_col;                                                             \
                                                                               \
void main() {                                                                  \
  frag_col = vec4(u_col , 1.0);                                                \
}                                                                              \
";

enum {
  PLOT_POINTS_COUNT = 1024, // Power of 2
};

struct plot {
  // Ring buffer of points
  f32 points[PLOT_POINTS_COUNT]; // time
  u32 end;  // one after last index in points
};

void start(void) {
  // Init
  u64 start_tsc = read_cpu_timer();
  f32 tsc_ifreq = 1.0f / read_cpu_timer_freq();
  log_init(&g_log, "plot.log", start_tsc, tsc_ifreq);

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

  GLuint plot_prog = create_gl_shader_program(
    s_plot_vert_src,
    s_plot_frag_src
  );

  struct plot plot0 = {0};

  GLuint vao;
  GLuint points_bo;
  GLuint points_ebo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &points_bo);
  glGenBuffers(1, &points_ebo);
  GLint plot_loc_col         = glGetUniformLocation(plot_prog, "u_col");
  GLint plot_loc_offset      = glGetUniformLocation(plot_prog, "u_offset");
  GLint plot_loc_points_size_minus_one =
    glGetUniformLocation(plot_prog, "u_points_size_minus_one");

  glUseProgram(plot_prog);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, points_bo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(plot0.points), plot0.points,
      GL_STATIC_DRAW);
  glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, points_ebo);
  GLuint indices[] = { PLOT_POINTS_COUNT - 1, 0 };
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
      GL_STATIC_DRAW);

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

    // Update plots
    u32 inserted_idx = plot0.end;
    plot0.points[plot0.end] = clampf32(dt * 50.0f, 0.0f, 1.0f);
    plot0.end = (plot0.end + 1) % PLOT_POINTS_COUNT;

    // Renderer
    glUniform1ui(plot_loc_points_size_minus_one, PLOT_POINTS_COUNT - 1);

    // Draw plots
    glBindBuffer(GL_ARRAY_BUFFER, points_bo);
    i32 subdata_size = sizeof(plot0.points[0]);
    glBufferSubData(GL_ARRAY_BUFFER, inserted_idx * subdata_size, subdata_size,
        &plot0.points[inserted_idx]);

    // Draw
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.8f, 0.8f, 0.8f, 0.8f);

    // Issue 3 draw calls, for 2 segments and 1 for connection in between
    u64 size0 = PLOT_POINTS_COUNT - plot0.end;
    u64 size1 = plot0.end;

    glUniform1i(plot_loc_offset, size0);

    // First segment
    glUniform3f(plot_loc_col, 1.0f, 0.0f, 0.5f);
    glDrawArrays(GL_LINE_STRIP, plot0.end, size0);

    // Connect 2 segments
    if (size1 > 0) {
      glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, 0);
    }

    // Second segment
    if (size1 > 1) {
      glDrawArrays(GL_LINE_STRIP, 0, size1);
    }

#if 1
    glPointSize(2.0f);
    glUniform3f(plot_loc_col, 0.2f, 0.2f, 0.2f);
    glDrawArrays(GL_POINTS, 0, PLOT_POINTS_COUNT);
#endif

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
