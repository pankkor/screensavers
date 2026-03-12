// Draw timeline graphs in pixel shader
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/timelines

#include "common.h"

// -----------------------------------------------------------------------------
// Screen space Timelines
// -----------------------------------------------------------------------------

// 1 full screen triangle
static const char * const s_timeline_vert_src = GLSL_V410 "                  \r\
const vec2 verts[3] = vec2[](                                                \r\
  vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0)                         \r\
);                                                                           \r\
                                                                             \r\
void main() {                                                                \r\
  gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);                          \r\
}                                                                            \r\
";

static const char * const s_timeline_frag_src = GLSL_V410 "                  \r\
uniform int u_offset;                                                        \r\
uniform int u_timelines_count;                                               \r\
uniform int u_points_count; /* points_count is power of 2 */                 \r\
uniform float u_izoom; /* inverse zoom */                                    \r\
uniform float u_scroll;                                                      \r\
uniform vec2 u_resolution;                                                   \r\
/* Point value buffer layout: timelines_count points for frame #N */         \r\
uniform samplerBuffer u_v_buf; /* p0,p0,..,p0,p1,.. ,p#(timelines_count-1) */\r\
                                                                             \r\
out vec4 frag_col;                                                           \r\
                                                                             \r\
const int POINTS_MAX = 1024;                                                 \r\
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
  vec2 st = gl_FragCoord.xy / u_resolution; /* 0->1 */                       \r\
  float timeline = st.t * u_timelines_count; /* 0->timelines_count */        \r\
  int timeline_idx = int(timeline);                                          \r\
  int palette_idx = timeline_idx % palettes.length();                        \r\
  vec4 col = vec4(palettes[palette_idx], 1.0);                               \r\
  int window = int(u_points_count * u_izoom); /* # points to display */      \r\
  int p_idx = int((st.s + u_scroll) * window); /* point # */                 \r\
  int p_idx_in_buf = p_idx * u_timelines_count + timeline_idx;               \r\
  float f = fract(timeline); /* 0->timelines_count to 0->1, 0->1 .. 0->1 */  \r\
  float v = texelFetch(u_v_buf, p_idx_in_buf).r;                             \r\
  float hpx = u_timelines_count / u_resolution.y; /* norm 1 px line height */\r\
  bool is_in_bounds = p_idx >= 0 && p_idx < u_points_count;                  \r\
  bool shall_draw_value = v > f || f < hpx; /* draw value or first pixel */  \r\
  float alpha_mask = float(is_in_bounds && shall_draw_value);                \r\
  col.a = alpha_mask;  /* erase points not in window and above value */      \r\
  int dp = (p_idx - u_offset) & u_points_count; /* fade out old points */    \r\
  col.a *= mix(0.4, 1.0, float(dp) / POINTS_MAX);                            \r\
  col.rgb *= col.a; /* premultiply alpha */                                  \r\
  frag_col = col;                                                            \r\
}                                                                            \r\
";

enum {
  TIMELINE_POINTS_COUNT = 1024, // Power of 2
  TIMELINES_COUNT = 8,
};

#define TIMELINES_MIN_ZOOM 0.1f
#define TIMELINES_MAX_ZOOM 3.0f
#define TIMELINES_MAX_SCROLL 1.0f
#define TIMELINES_MIN_SCROLL -1.0f

struct timelines_state {
  u32 current_frame_num;
  f32 dzoom;          // delta zoom,   effective zoom   = 1.0 + dzoom
  f32 scroll;         //               effective scroll = 0.0 + dscroll
  f32 resolution[2];  // screen [width, height]
};

// Clamp to MIN, MAX limits
void timelines_state_bound(struct timelines_state *state) {
  state->current_frame_num =  state->current_frame_num % TIMELINE_POINTS_COUNT;
  state->scroll = clampf32(state->scroll,
        TIMELINES_MIN_SCROLL, TIMELINES_MAX_SCROLL);
  state->dzoom  = clampf32(state->dzoom,
        TIMELINES_MIN_ZOOM - 1.0f, TIMELINES_MAX_ZOOM - 1.0f);
}

struct timelines_gpu {
  // No internal buffer - data is streamed to GPU
  // OpenGL
  GLuint vao;
  GLuint y_bo;
  GLuint y_tx;

  GLuint prog;
  GLint loc_y_buf;
  GLint loc_timelines_count;
  GLint loc_offset;
  GLint loc_izoom;
  GLint loc_scroll;
  GLint loc_points_count;
  GLint loc_resolution;
};

void timelines_gpu_init(struct timelines_gpu *tgs) {
  EXPECT(tgs->vao == 0, "timelines_gpu is already initialized?");
  EXPECT(tgs->y_bo == 0, "timelines_gpu is already initialized?");
  EXPECT(tgs->y_tx == 0, "timelines_gpu is already initialized?");
  glGenVertexArrays(1, &tgs->vao);
  glGenTextures(1, &tgs->y_tx);
  glGenBuffers(1, &tgs->y_bo);

  glBindBuffer(GL_TEXTURE_BUFFER, tgs->y_bo);
  glBufferData(GL_TEXTURE_BUFFER,
      GL_R32F * TIMELINES_COUNT * TIMELINE_POINTS_COUNT, 0, GL_STATIC_DRAW);

  glBindTexture(GL_TEXTURE_BUFFER, tgs->y_tx);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, tgs->y_bo);

  tgs->prog = create_gl_shader_program(
    s_timeline_vert_src,
    s_timeline_frag_src
  );

  tgs->loc_y_buf           = glGetUniformLocation(tgs->prog, "u_v_buf");
  tgs->loc_offset          = glGetUniformLocation(tgs->prog, "u_offset");
  tgs->loc_izoom           = glGetUniformLocation(tgs->prog, "u_izoom");
  tgs->loc_scroll          = glGetUniformLocation(tgs->prog, "u_scroll");
  tgs->loc_points_count    = glGetUniformLocation( tgs->prog, "u_points_count");
  tgs->loc_resolution      = glGetUniformLocation(
      tgs->prog, "u_resolution");
  tgs->loc_timelines_count = glGetUniformLocation(
      tgs->prog, "u_timelines_count");
}

void timelines_gpu_shutdown(struct timelines_gpu *tgs) {
  glDeleteShader(tgs->prog);
  glDeleteBuffers(1, &tgs->y_bo);
  glDeleteTextures(1, &tgs->y_tx);
  glDeleteVertexArrays(1, &tgs->vao);
  *tgs = (struct timelines_gpu){0};
}
void timelines_gpu_draw(const struct timelines_gpu *tgs,
    const struct timelines_state *state) {
  glUseProgram(tgs->prog);

  glBindTexture(GL_TEXTURE_BUFFER, tgs->y_tx);
  glActiveTexture(GL_TEXTURE0);

  f32 zoom    = 1.0f + state->dzoom;
  f32 izoom   = 1.0f / zoom;
  f32 w       = state->resolution[0];
  f32 h       = state->resolution[1];

  glUniform1ui(tgs->loc_y_buf,                 0); // sampler buffer 0
  glUniform1i(tgs->loc_timelines_count,        TIMELINES_COUNT);
  glUniform1i(tgs->loc_offset,                 state->current_frame_num);
  glUniform1f(tgs->loc_izoom,                  izoom);
  glUniform1f(tgs->loc_scroll,                 state->scroll);
  glUniform1i(tgs->loc_points_count,           TIMELINE_POINTS_COUNT);
  glUniform2f(tgs->loc_resolution,             w, h);

  glBindVertexArray(tgs->vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);
}

// Stream timeline data to GPU. Normalize data values to [0.0, 1.0]
struct timelines_data_update {
  f32 data[TIMELINES_COUNT];    // Data to be updated for 1 frame
  u32 frame_num;
};

void timelines_gpu_batch_update(struct timelines_gpu *tgs,
    const struct timelines_data_update *pu) {
  u32 insert_idx = pu->frame_num % TIMELINE_POINTS_COUNT;
  i32 frame_data_size = sizeof(pu->data);

  glBindBuffer(GL_TEXTURE_BUFFER, tgs->y_bo);
  glBufferSubData(
      GL_TEXTURE_BUFFER,
      insert_idx * frame_data_size,
      frame_data_size,
      &pu->data);
}

void timelines_gpu_partial_update(struct timelines_gpu *tgs, u32 timeline_idx,
    const f32 v, u32 frame_num) {
  u32 insert_idx = frame_num % TIMELINE_POINTS_COUNT;

  glBindBuffer(GL_TEXTURE_BUFFER, tgs->y_bo);
  glBufferSubData(
      GL_TEXTURE_BUFFER,
      sizeof(v) * (insert_idx * TIMELINES_COUNT + timeline_idx),
      sizeof(v),
      &v);
}

// GPU elapesed time queries
struct queries_gpu {
  // Number of queries in flight, 1 query per frame
  enum {QUERIES_COUNT = 4};       // Power of 2
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

void timelines_gpu_partial_update_queries_gpu(struct timelines_gpu *tgs,
    struct queries_gpu *qs, u32 timeline_idx) {
  for (i32 i = 0; i < QUERIES_COUNT; ++i) {
    f32 qs_time = qs->results[i] / 1000.0f / 1000.0f;
    u32 qs_frame = qs->frame_nums[i];
    timelines_gpu_partial_update(tgs, timeline_idx, qs_time, qs_frame);
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
    struct timelines_data_update timelines_upd = {
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

    // stream datat to timelines
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
