// Draw some plots.
// Time trace line with fetching Y from GL_TEXTURE_BUFFER
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/plot_v_fetch

#include "common.h"

// --------------------------------------
// GLSL
// --------------------------------------
static const char * const s_plot_vert_src = "                                  \
#version 410 core                                                              \
                                                                               \
uniform int u_offset;                                                          \
uniform int u_points_size_minus_one; /* points_size is power of 2 */           \
uniform samplerBuffer u_y_buf;                                                 \
                                                                               \
out vec3 col;                                                                  \
                                                                               \
void main() {                                                                  \
  int v_id = (gl_VertexID + u_offset) & u_points_size_minus_one;               \
  float dx = 2.0 / u_points_size_minus_one;                                    \
  float x = -1.0 + dx * gl_VertexID;                                           \
  float y = texelFetch(u_y_buf, v_id).r;                                       \
  gl_Position = vec4(x, y * 2.0 - 1.0, 0.0, 1.0);                              \
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
  GLuint y_bo;
  GLuint y_tx;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &y_bo);
  glGenTextures(1, &y_tx);
  GLint plot_loc_col         = glGetUniformLocation(plot_prog, "u_col");
  GLint plot_loc_y_buf       = glGetUniformLocation(plot_prog, "u_y_buf");
  GLint plot_loc_offset      = glGetUniformLocation(plot_prog, "u_offset");
  GLint plot_loc_points_size_minus_one =
    glGetUniformLocation(plot_prog, "u_points_size_minus_one");

  glBindVertexArray(vao);
  glBindBuffer(GL_TEXTURE_BUFFER, y_bo);
  glBufferData(GL_TEXTURE_BUFFER, sizeof(plot_total.points), plot_total.points,
      GL_STATIC_DRAW);

  glBindTexture(GL_TEXTURE_BUFFER, y_tx);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, y_bo);

  glUseProgram(plot_prog);
  glActiveTexture(GL_TEXTURE0);
  // TODO: check
  glBindTexture(GL_TEXTURE_BUFFER, y_tx);
  glUniform1ui(plot_loc_y_buf, 0);

  struct gl_queries qs[2] = {0};
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
    u32 inserted_idx = plot_total.end;
    plot_total.points[plot_total.end] = clampf32(dt * 50.0f, 0.0f, 1.0f);
    plot_total.end = (plot_total.end + 1) % PLOT_POINTS_COUNT;

    // Renderer
    glUniform1i(plot_loc_points_size_minus_one, PLOT_POINTS_COUNT - 1);

    // Draw plots
    glBindBuffer(GL_ARRAY_BUFFER, y_bo);
    i32 subdata_size = sizeof(plot_total.points[0]);
    glBufferSubData(GL_ARRAY_BUFFER, inserted_idx * subdata_size, subdata_size,
        &plot_total.points[inserted_idx]);

    // Draw
    glClearColor(0.8f, 0.8f, 0.8f, 0.8f);
    glClear(GL_COLOR_BUFFER_BIT);

    // TODO
    // u64 offset = PLOT_POINTS_COUNT - plot_total.end;
    u64 offset = plot_total.end;

    glUniform1i(plot_loc_offset, offset);

    gl_queries_query_begin(&qs[0], frame_num);

    glUniform3f(plot_loc_col, 1.0f, 0.0f, 0.5f);
    glDrawArrays(GL_LINE_STRIP, 0, PLOT_POINTS_COUNT);

#if 1
    glPointSize(2.0f);
    glUniform3f(plot_loc_col, 0.2f, 0.2f, 0.2f);
    glDrawArrays(GL_POINTS, 0, PLOT_POINTS_COUNT);
#endif

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

  glDeleteBuffers(1, &y_bo);
  glDeleteTextures(1, &y_tx);
  glDeleteVertexArrays(1, &vao);

  window_shutdown(&w);
  event_loop_shutdown(&loop);

  log_shutdown(&g_log);

  exit(0);
}
