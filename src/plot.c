// Draw some plots.
// Time trace line from a ring buffer Buffer Object
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/plot

#include "common.h"

// --------------------------------------
// GLSL
// --------------------------------------
static const char * const s_plot_vert_src = "                                  \
#version 410 core                                                              \
layout(location = 0) in float v_y;                                             \
                                                                               \
uniform float u_y;                                                             \
uniform int u_offset;                                                          \
uniform int u_points_size_minus_one; /* points_size is power of 2 */           \
                                                                               \
out vec3 col;                                                                  \
                                                                               \
const uint n_plots = 10;                                                       \
const float y_scale = 1.0 / n_plots;                                           \
                                                                               \
void main() {                                                                  \
  int v_id = (gl_VertexID + u_offset) & u_points_size_minus_one;               \
  float x = float(v_id) / u_points_size_minus_one;                             \
  float y = (u_y + v_y) * y_scale;                                             \
  gl_Position = vec4(vec2(x, y) * 2.0 - 1.0, 0.0, 1.0);                        \
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
  PLOT_POINTS_COUNT = 1024,       // Power of 2
};

// Plot with ring buffer of points
struct plot {
  f32 points[PLOT_POINTS_COUNT];  // time
  u32 end;                        // one after last index in points
};

struct gl_queries {
  enum {QUERIES_COUNT = 16};      // Power of 2
  GLuint queries[QUERIES_COUNT];
  u64 results[QUERIES_COUNT];     // Ring buffer of query results
  u32 frame_nums[QUERIES_COUNT];  // Frame number of a query in a ring buffer
  u32 current;                    // Current query, advanced by query_begin/end
};

void gl_queries_init(struct gl_queries *qs) {
  glGenQueries(QUERIES_COUNT, qs->queries);
}

void gl_queries_shutdown(struct gl_queries *qs) {
  glDeleteQueries(QUERIES_COUNT, qs->queries);
}

void gl_queries_query_begin(struct gl_queries *qs, u64 frame_num) {
  glBeginQuery(GL_TIME_ELAPSED, qs->queries[qs->current]);
  qs->frame_nums[qs->current] = frame_num;
}
void gl_queries_query_end(struct gl_queries *qs) {
  glEndQuery(GL_TIME_ELAPSED);
  qs->current = (qs->current + 1) % QUERIES_COUNT;
}

u64 gl_queries_result(const struct gl_queries *qs, u32 idx) {
  GLuint64 ret = -1;
  GLuint available = 0;
  glGetQueryObjectuiv(qs->queries[idx], GL_QUERY_RESULT_AVAILABLE, &available);
  if (available) {
    glGetQueryObjectui64v(qs->queries[idx], GL_QUERY_RESULT, &ret);
  }
  return ret;
}

void gl_queries_poll(struct gl_queries *qs) {
  for (i32 i = 0; i < QUERIES_COUNT; ++i) {
    i64 idx = (qs->current + i) % QUERIES_COUNT;
    u64 res = gl_queries_result(qs, idx);
    qs->results[idx] = res;
  }
}

void gl_queries_debug_print(const struct gl_queries *qs, u64 frame_num) {
  print_cstr(STDOUT, "Frame #");
  print_u64(STDOUT, frame_num);
  print_cstr(STDOUT, ". Queries:\n");
  for (i32 i = 0; i < QUERIES_COUNT; ++i) {
    print_cstr(STDOUT, "#");
    print_u64(STDOUT, i);
    print_cstr(STDOUT, "\tframe#:");
    print_u64(STDOUT, qs->frame_nums[i]);
    print_cstr(STDOUT, "\tresult: ");
    print_u64(STDOUT, qs->results[i]);
    print_cstr(STDOUT, " ns,\t");
    print_f32(STDOUT, qs->results[i] / 1000.0);
    print_cstr(STDOUT, " us\n");
  }
}

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

  u32 frame_num = 0;

  struct plot plot_total = {0};

  GLuint vao;
  GLuint points_bo;
  GLuint points_ebo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &points_bo);
  glGenBuffers(1, &points_ebo);
  GLint plot_loc_y           = glGetUniformLocation(plot_prog, "u_y");
  GLint plot_loc_offset      = glGetUniformLocation(plot_prog, "u_offset");
  GLint plot_loc_col         = glGetUniformLocation(plot_prog, "u_col");
  GLint plot_loc_points_size_minus_one =
    glGetUniformLocation(plot_prog, "u_points_size_minus_one");

  glUseProgram(plot_prog);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, points_bo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(plot_total.points), plot_total.points,
      GL_STATIC_DRAW);
  glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, points_ebo);
  GLuint indices[] = { PLOT_POINTS_COUNT - 1, 0 };
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
      GL_STATIC_DRAW);

  struct gl_queries qs[1] = {0};
  gl_queries_init(&qs[0]);

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
#define DT_MAX (1.0f / 100.0f)
    u32 inserted_idx = plot_total.end;
    plot_total.points[plot_total.end] = dt / DT_MAX;
    plot_total.end = (plot_total.end + 1) % PLOT_POINTS_COUNT;

    // Renderer
    glUniform1i(plot_loc_points_size_minus_one, PLOT_POINTS_COUNT - 1);

    // Draw plots
    glBindBuffer(GL_ARRAY_BUFFER, points_bo);
    i32 subdata_size = sizeof(plot_total.points[0]);
    glBufferSubData(GL_ARRAY_BUFFER, inserted_idx * subdata_size, subdata_size,
        &plot_total.points[inserted_idx]);

    // Draw
    glClearColor(0.8f, 0.8f, 0.8f, 0.8f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Issue 3 draw calls, for 2 segments and 1 for connection in between
    u64 size0 = PLOT_POINTS_COUNT - plot_total.end;
    u64 size1 = plot_total.end;

    gl_queries_query_begin(&qs[0], frame_num);

    // Draw 10 plots
    for (int i = 0; i < 10; ++i) {
        glUniform1f(plot_loc_y, (f32)i);

        glUniform1i(plot_loc_offset, size0);

        // First segment
        glUniform3f(plot_loc_col, 0.2f, 0.6f * i * 0.1f, 0.2f);
        glDrawArrays(GL_LINE_STRIP, plot_total.end, size0);

        // Connect 2 segments
        if (size1 > 0) {
          glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, 0);
        }

        // Second segment
        if (size1 > 1) {
          glDrawArrays(GL_LINE_STRIP, 0, size1);
        }

    #if 0
        glPointSize(2.0f);
        glUniform3f(plot_loc_col, 0.2f, 0.2f, 0.2f);
        glDrawArrays(GL_POINTS, 0, PLOT_POINTS_COUNT);
    #endif
    }

    gl_queries_query_end(&qs[0]);


    window_flush(&w);

    gl_queries_poll(&qs[0]);
#if 1

    // Debug print qeury results
    if (frame_num % 16 == 0) {
      gl_queries_debug_print(&qs[0], frame_num);
    }
#endif
    frame_num += 1;
  }

shutdown:
  print_avg_dt_fps(loop_s / loop_count);

  // Shutdown
  gl_queries_shutdown(&qs[0]);

  glDeleteShader(plot_prog);

  glDeleteBuffers(1, &points_bo);
  glDeleteVertexArrays(1, &vao);

  window_shutdown(&w);
  event_loop_shutdown(&loop);

  log_shutdown(&g_log);

  exit(0);
}
