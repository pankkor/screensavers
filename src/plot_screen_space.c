// Draw some plots.
// Time trace lines in pixel shader
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/plot_screen_space

#include "common.h"

// --------------------------------------
// GLSL
// --------------------------------------
#define GLSL_V410 "#version 410 core\n#line " STR(__LINE__) "\n"

static const char * const s_plot_vert_src = GLSL_V410 "                      \r\
out vec2 f_uv;                                                               \r\
                                                                             \r\
const vec2 verts[3] = vec2[](                                                \r\
  vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0)                         \r\
);                                                                           \r\
const vec2 uvs[3] = vec2[](                                                  \r\
  vec2(0.0, 1.0), vec2(2.0, 1.0), vec2(0.0, -1.0)                            \r\
);                                                                           \r\
                                                                             \r\
void main() {                                                                \r\
  f_uv = vec2(uvs[gl_VertexID]);                                             \r\
  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);                          \r\
}                                                                            \r\
";

static const char * const s_plot_frag_src = GLSL_V410 "                      \r\
uniform int u_offset;                                                        \r\
uniform int u_plots_count;                                                   \r\
uniform int u_points_size_minus_one; /* points_size is power of 2 */         \r\
uniform float u_izoom; /* inverse zoom */                                    \r\
uniform float u_scroll;                                                      \r\
uniform samplerBuffer u_y_buf;                                               \r\
                                                                             \r\
in vec2 f_uv;                                                                \r\
out vec4 frag_col;                                                           \r\
                                                                             \r\
const int POINTS_MAX = 1024;                                                 \r\
                                                                             \r\
const vec3 palettes[] = vec3[](                                              \r\
  vec3(0.59, 0.18, 0.15),                                                    \r\
  vec3(0.16, 0.44, 0.34),                                                    \r\
  vec3(0.61, 0.46, 0.15),                                                    \r\
  vec3(0.13, 0.41, 0.58),                                                    \r\
  vec3(0.59, 0.30, 0.08),                                                    \r\
  vec3(0.41, 0.24, 0.50),                                                    \r\
  vec3(0.12, 0.56, 0.12),                                                    \r\
  vec3(0.57, 0.38, 0.49),                                                    \r\
  vec3(0.25, 0.25, 0.25),                                                    \r\
  vec3(0.00, 0.42, 0.42)                                                     \r\
);                                                                           \r\
                                                                             \r\
void main() {                                                                \r\
  int window = int(POINTS_MAX * u_izoom);                                    \r\
  int plot_idx = int(f_uv.t * u_plots_count);                                \r\
  int palette_idx = plot_idx % u_plots_count;                                \r\
  vec4 col = vec4(palettes[palette_idx], 1.0);                               \r\
  int point = int((f_uv.s + u_scroll) * window);                             \r\
  int point_in_buf = point * u_plots_count + plot_idx;                       \r\
  float plot = f_uv.t * u_plots_count;                                       \r\
  float t = 1.0 - fract(plot);                                               \r\
  float y = texelFetch(u_y_buf, point_in_buf).r;                             \r\
  if (y < t) {                                                               \r\
    col = vec4(0.0, 0.0, 0.0, 0.0);                                          \r\
  }                                                                          \r\
  int dpoint = (point - u_offset) & u_points_size_minus_one;                 \r\
  col.a *= mix(0.4, 1.0, float(dpoint) / POINTS_MAX);                        \r\
  col.rgb *= col.a;                                                          \r\
  if (t < 0.01) {                                                            \r\
    /* line */                                                               \r\
    col = vec4(0.3, 0.3, 0.3, 1.0);                                          \r\
  }                                                                          \r\
  frag_col = col;                                                            \r\
}                                                                            \r\
";

enum {
  PLOT_POINTS_COUNT = 1024, // Power of 2
  PLOTS_COUNT = 8,
};

struct plots_data_update {
  f32 data[PLOTS_COUNT];  // data to be updated for 1 frame
  u32 frame_num;
};

#define PLOTS_MIN_ZOOM 0.1f
#define PLOTS_MAX_ZOOM 3.0f
#define PLOTS_MAX_SCROLL 1.0f
#define PLOTS_MIN_SCROLL -1.0f

struct plots_state {
  u32 current_frame_num;
  f32 dzoom;   // delta zoom,   effective zoom   = 1.0 + dzoom
  f32 scroll;  //               effective scroll = 0.0 + dscroll
};

struct plots_gpu {
  struct plots_state state;
  // Data is streamed to GPU

  // OpenGL
  GLuint vao;
  GLuint y_bo;
  GLuint y_tx;

  GLuint prog;
  GLint loc_y_buf;
  GLint loc_plots_count;
  GLint loc_offset;
  GLint loc_izoom;
  GLint loc_scroll;
  GLint loc_points_size_minus_one;
};

void plots_gpu_init(struct plots_gpu *ps) {
  EXPECT(ps->vao == 0, "plots_gpu is already initialized?");
  EXPECT(ps->y_bo == 0, "plots_gpu is already initialized?");
  EXPECT(ps->y_tx == 0, "plots_gpu is already initialized?");
  glGenVertexArrays(1, &ps->vao);
  glGenTextures(1, &ps->y_tx);
  glGenBuffers(1, &ps->y_bo);

  glBindBuffer(GL_TEXTURE_BUFFER, ps->y_bo);
  glBufferData(GL_TEXTURE_BUFFER, GL_R32F * PLOTS_COUNT * PLOT_POINTS_COUNT, 0,
      GL_STATIC_DRAW);

  glBindTexture(GL_TEXTURE_BUFFER, ps->y_tx);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, ps->y_bo);

  ps->prog = create_gl_shader_program(
    s_plot_vert_src,
    s_plot_frag_src
  );

  ps->loc_y_buf       = glGetUniformLocation(ps->prog, "u_y_buf");
  ps->loc_plots_count = glGetUniformLocation(ps->prog, "u_plots_count");
  ps->loc_offset      = glGetUniformLocation(ps->prog, "u_offset");
  ps->loc_izoom       = glGetUniformLocation(ps->prog, "u_izoom");
  ps->loc_scroll      = glGetUniformLocation(ps->prog, "u_scroll");
  ps->loc_points_size_minus_one = glGetUniformLocation(ps->prog,
      "u_points_size_minus_one");
}

void plots_gpu_shutdown(struct plots_gpu *ps) {
  glDeleteShader(ps->prog);
  glDeleteBuffers(1, &ps->y_bo);
  glDeleteTextures(1, &ps->y_tx);
  glDeleteVertexArrays(1, &ps->vao);
  *ps = (struct plots_gpu){0};
}

void plots_state_bound(struct plots_state *state) {
  *state = (struct plots_state){
    .current_frame_num =  state->current_frame_num % PLOT_POINTS_COUNT,
    .scroll = clampf32(state->scroll, PLOTS_MIN_SCROLL, PLOTS_MAX_SCROLL),
    .dzoom  = clampf32(state->dzoom, PLOTS_MIN_ZOOM - 1.0f,
        PLOTS_MAX_ZOOM - 1.0f),
  };
}

void plots_gpu_batch_update(struct plots_gpu *ps,
    const struct plots_data_update *pu) {
  u32 insert_idx = pu->frame_num % PLOT_POINTS_COUNT;
  i32 frame_data_size = sizeof(pu->data);

  glBindBuffer(GL_TEXTURE_BUFFER, ps->y_bo);
  glBufferSubData(
      GL_TEXTURE_BUFFER,
      insert_idx * frame_data_size,
      frame_data_size,
      &pu->data);
}

void plots_gpu_partial_update(struct plots_gpu *ps, u32 plot_idx, const f32 v,
    u32 frame_num) {
  u32 insert_idx = frame_num % PLOT_POINTS_COUNT;

  glBindBuffer(GL_TEXTURE_BUFFER, ps->y_bo);
  glBufferSubData(
      GL_TEXTURE_BUFFER,
      sizeof(v) * (insert_idx * PLOTS_COUNT + plot_idx),
      sizeof(v),
      &v);
}

void plots_gpu_draw(const struct plots_gpu *ps) {
  glUseProgram(ps->prog);

  glBindTexture(GL_TEXTURE_BUFFER, ps->y_tx);
  glActiveTexture(GL_TEXTURE0);

  f32 zoom    = 1.0f + ps->state.dzoom;
  f32 izoom   = 1.0f / zoom;

  glUniform1ui(ps->loc_y_buf,                 0); // sampler buffer 0
  glUniform1i(ps->loc_plots_count,            PLOTS_COUNT);
  glUniform1i(ps->loc_offset,                 ps->state.current_frame_num);
  glUniform1f(ps->loc_izoom,                  izoom);
  glUniform1f(ps->loc_scroll,                 ps->state.scroll);
  glUniform1i(ps->loc_points_size_minus_one,  PLOT_POINTS_COUNT - 1);

  glBindVertexArray(ps->vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);
}

struct queries_gpu {
  enum {QUERIES_COUNT = 16};      // Power of 2
  GLuint queries[QUERIES_COUNT];
  u32 frame_nums[QUERIES_COUNT];  // Frame number of a query in a ring buffer
  u64 results[QUERIES_COUNT];     // Ring buffer of query results
  u32 current;                    // Current query, advanced by query_begin/end
};

void queries_gpu_init(struct queries_gpu *qs) {
  glGenQueries(QUERIES_COUNT, qs->queries);
}

void queries_gpu_shutdown(struct queries_gpu *qs) {
  glDeleteQueries(QUERIES_COUNT, qs->queries);
}

void queries_gpu_query_begin(struct queries_gpu *qs, u64 frame_num) {
  glBeginQuery(GL_TIME_ELAPSED, qs->queries[qs->current]);
  qs->results[qs->current] = 0;
  qs->frame_nums[qs->current] = frame_num;
}
void queries_gpu_query_end(struct queries_gpu *qs) {
  glEndQuery(GL_TIME_ELAPSED);
  qs->current = (qs->current + 1) % QUERIES_COUNT;
}

u64 queries_gpu_result(const struct queries_gpu *qs, u32 idx) {
  GLuint64 ret = 0;
  GLuint available = 0;
  glGetQueryObjectuiv(qs->queries[idx], GL_QUERY_RESULT_AVAILABLE, &available);
  if (available) {
    glGetQueryObjectui64v(qs->queries[idx], GL_QUERY_RESULT, &ret);
  }
  return ret;
}

void queries_gpu_poll(struct queries_gpu *qs) {
  for (i32 i = 0; i < QUERIES_COUNT; ++i) {
    u64 res = queries_gpu_result(qs, i);
    qs->results[i] = res;
  }
}

void plots_gpu_partial_update_queries_gpu(struct plots_gpu *ps, struct queries_gpu *qs,
    u32 plot_idx) {
  for (i32 i = 0; i < QUERIES_COUNT; ++i) {
    f32 qs_time = qs->results[i] / 1000.0f / 1000.0f;
    u32 qs_frame = qs->frame_nums[i];
    plots_gpu_partial_update(ps, plot_idx, qs_time, qs_frame);
  }
}

void queries_gpu_debug_print(const struct queries_gpu *qs, u64 frame_num) {
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

  u64 frame_num = 0;

  // Game loop
  f32 cpu_timer_freq  = read_cpu_timer_freq();
  f32 icpu_timer_freq = 1.0f / cpu_timer_freq;
  u64 tsc             = read_cpu_timer();

  f32 loop_s          = 0.0f;
  u64 loop_count      = 0;
  f32 print_dt_tsc    = tsc + 5.0f * cpu_timer_freq;

  b32 debug_frame_mode = 0;

  struct plots_gpu plots = {0};
  struct queries_gpu qs  = {0};

  plots_gpu_init(&plots);
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
        plots.state.dzoom -= dzoom;
      }
      if (is_down_plus) {
        plots.state.dzoom += dzoom;

      }
    } else {
      // Control scroll with - and =
      if (is_down_minus) {
        plots.state.scroll += dscroll;
      }
      if (is_down_plus) {
        plots.state.scroll -= dscroll;
      }
    }
    plots.state.current_frame_num = frame_num;
    plots_state_bound(&plots.state);

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
    struct plots_data_update plots_upd = {
      // Fake data
      .data       = {
        sdt,                                              // scaled dt
        0,                                                // draw query
        loop_s / loop_count * 100,                        // avg fps
        0,                                                // zero
        (frame_num % 200) * 0.001,                        // frame number
        0.5f + 0.5f * sinf32((frame_num % 1000) * 0.01),  // sin
        (plots.state.scroll + 1.0f),                      // scroll
        (1.0f + plots.state.dzoom) * 0.25f,               // zoom
      },
      .frame_num  = frame_num,
    };

    // Draw
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.8f, 0.8f, 0.8f, 0.8f);
    glClear(GL_COLOR_BUFFER_BIT);

    queries_gpu_query_begin(&qs, frame_num);

    // stream datat to plots
    plots_gpu_batch_update(&plots, &plots_upd);
    plots_gpu_partial_update_queries_gpu(&plots, &qs, 1);

    plots_gpu_draw(&plots);

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
  plots_gpu_shutdown(&plots);

  window_shutdown(&w);
  event_loop_shutdown(&loop);

  log_shutdown(&g_log);

  exit(0);
}
