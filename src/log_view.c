// View mmaped ring buffer log file with bitmap font
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/log_view

#include "common.h"

#include "res_font_256.h"
#include "res_ascii_anim.h"

enum {
  BUF_W = LOG_LINE_BYTES,
  BUF_H = LOG_LINES,
  TEXT_W = LOG_LINE_BYTES,
  TEXT_H = 80,
  TEXTS_COUNT = 1,
};

u32 TEXT_COLOR_RGBA = 0xCFDFFFFF; // 0xRRGGBBAA

// --------------------------------------
// GLSL
// --------------------------------------
static const char * const s_text_vert_src  = GLSL_V410 "                     \r\
                                                                             \r\
uniform vec2 u_size;                                                         \r\
uniform uint u_color; /* RGBA */                                             \r\
                                                                             \r\
out vec2 f_uv;                                                               \r\
out vec4 f_color;                                                            \r\
                                                                             \r\
const vec2 verts[4] = vec2[](                                                \r\
  vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0)         \r\
);                                                                           \r\
const vec2 uvs[4] = vec2[](                                                  \r\
  vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0)             \r\
);                                                                           \r\
                                                                             \r\
vec4 rgba2vec4(uint rgba) {                                                  \r\
  return vec4((rgba >> 24) & 0xFFu, (rgba >> 16) & 0xFFu,                    \r\
    (rgba >> 8) & 0xFFu, rgba & 0xFFu) / 255.0;                              \r\
}                                                                            \r\
                                                                             \r\
void main(void) {                                                            \r\
  vec2 vert = verts[gl_VertexID] * u_size;                                   \r\
  vec2 uv = uvs[gl_VertexID];                                                \r\
  gl_Position = vec4(vert, 0.0, 1.0);                                        \r\
  f_uv = uv;                                                                 \r\
  f_color = rgba2vec4(u_color);                                              \r\
}                                                                            \r\
";

static const char * const s_text_frag_src  = GLSL_V410 "                     \r\
in vec2 f_uv;                                                                \r\
in vec4 f_color;                                                             \r\
                                                                             \r\
uniform sampler2D font_tx;                                                   \r\
uniform usamplerBuffer u_text_buf; /* text ring buffer of size u_buf_size */ \r\
                                                                             \r\
uniform ivec2 u_buf_window;                                                  \r\
uniform ivec2 u_buf_size; /* pow of 2 */                                     \r\
uniform ivec2 u_glyphs_count; /* number of glyphs in atlas row and column */ \r\
uniform int u_offset_y;                                                      \r\
uniform int u_line_last;                                                     \r\
                                                                             \r\
out vec4 frag_col;                                                           \r\
                                                                             \r\
/* Returns 0 if x is outside of [l, r) */                                    \r\
float mask_range(float x, float l, float r) {                                \r\
  return step(l, x) * step(x, r);                                            \r\
}                                                                            \r\
                                                                             \r\
int mask_range(int x, int l, int r) {                                        \r\
  return int(step(l, x) * (1.0 - step(r, x)));                               \r\
}                                                                            \r\
                                                                             \r\
void main(void) {                                                            \r\
  int buf_mod_mask = u_buf_size.y - 1;                                       \r\
  vec2 win_pos = f_uv * u_buf_window;                                        \r\
  ivec2 win_ipos = ivec2(win_pos);                                           \r\
                                                                             \r\
  /* Operate in logical space w/o u_line_last offset (first line at 0) */    \r\
  /* Offset for - window hight, so last line is at the bottom of a window */ \r\
  int buf_line = win_ipos.y + u_offset_y + u_buf_size.y - u_buf_window.y;    \r\
  int mask = mask_range(buf_line, 0, u_buf_size.y);                          \r\
  /* Move to ring buffer space physical */                                   \r\
  buf_line = (buf_line + u_line_last) % u_buf_size.y;                        \r\
                                                                             \r\
  int buf_idx = win_ipos.x + buf_line * u_buf_size.x;                        \r\
  uint c = texelFetch(u_text_buf, buf_idx).r;                                \r\
                                                                             \r\
  vec2 glyph_pos = vec2(c % u_glyphs_count.x, c / u_glyphs_count.y);         \r\
  vec2 uv = (glyph_pos + fract(win_pos)) / u_glyphs_count;                   \r\
  float a = texture(font_tx, uv).r;                                          \r\
                                                                             \r\
  /* Dim older recent lines */                                               \r\
  int dist = (buf_line - u_line_last) & buf_mod_mask;                        \r\
  a *= mix(0.4, 1.0, float(dist) / u_buf_size.y);                            \r\
  a *= mask;                                                                 \r\
  frag_col = f_color * a;                                                    \r\
}                                                                            \r\
";

// --------------------------------------
// Entry point (aka main)
// --------------------------------------
void start(void) {
  // Init
  u64 start_tsc = read_cpu_timer();
  f32 tsc_ifreq = 1.0f / read_cpu_timer_freq();
  log_init(&g_log, "log_view.log", start_tsc, tsc_ifreq);

  struct event_loop loop;
  struct window w;

  event_loop_init(&loop);
  window_init(&w, 0 /* vsync */, 0 /*is_full_screen*/);

  GLint max_array_texture_layers;
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_array_texture_layers);
  const GLubyte* version_cstr = glGetString(GL_VERSION);
  print_cstr(STDOUT, "OpenGL version: '");
  print_cstr(STDOUT, (const char *)version_cstr);
  print_cstr(STDOUT, "'\nGL_MAX_ARRAY_TEXTURE_LAYERS: ");
  print_i64(STDOUT, max_array_texture_layers);
  print_cstr(STDOUT, "\n\n");
  print_cstr(STDOUT, "<Press ESC to exit>\n");

  GLuint text_prog = create_gl_shader_program(
    s_text_vert_src,
    s_text_frag_src
  );

  f32 aspect  = w.rect[2] / w.rect[3];
  f32 iaspect = 1.0f / aspect;
  f32 size[2] = { 1.0f, 1.0f };
  size[0] = 1.0f * iaspect;
  size[1] = 1.0f;

  GLuint vao;
  GLuint text_bo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &text_bo);

  glUseProgram(text_prog);
  glBindVertexArray(vao);

  // Font texture
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glActiveTexture(GL_TEXTURE0);
  GLuint font_tx;
  glGenTextures(1, &font_tx);
  glBindTexture(GL_TEXTURE_2D, font_tx);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_TX_W, FONT_TX_H, 0, GL_RED,
      GL_UNSIGNED_BYTE, s_font_tx_data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glGenerateMipmap(GL_TEXTURE_2D);

  // On screen text buffer
  glBindBuffer(GL_TEXTURE_BUFFER, text_bo);
  glBufferData(GL_TEXTURE_BUFFER, BUF_W * BUF_H, 0, GL_DYNAMIC_DRAW); // Orphan

  glActiveTexture(GL_TEXTURE1);
  GLuint tbo;
  glGenTextures(1, &tbo);
  glBindTexture(GL_TEXTURE_BUFFER, tbo);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, text_bo);

  glUniform1i(glGetUniformLocation(text_prog, "font_tx"), 0);
  glUniform1i(glGetUniformLocation(text_prog, "u_text_buf"), 1);
  glUniform1f(glGetUniformLocation(text_prog, "u_iaspect"), iaspect);
  glUniform2f(glGetUniformLocation(text_prog, "u_size"), size[0], size[1]);
  glUniform1ui(glGetUniformLocation(text_prog, "u_color"), TEXT_COLOR_RGBA);
  glUniform2i(glGetUniformLocation(text_prog, "u_buf_size"), BUF_W, BUF_H);
  glUniform2i(glGetUniformLocation(text_prog, "u_buf_window"), TEXT_W, TEXT_H);
  glUniform2i(glGetUniformLocation(text_prog, "u_glyphs_count"), FONT_GLYPHS_W,
      FONT_GLYPHS_H);

  // Logic

  // Game loop
  f32 cpu_timer_freq  = read_cpu_timer_freq();
  f32 icpu_timer_freq = 1.0f / cpu_timer_freq;
  u64 tsc             = read_cpu_timer();

  f32 loop_s          = 0.0f;
  u64 loop_count      = 0;
  f32 print_dt_tsc    = tsc + 5.0f * cpu_timer_freq;

  // Create text that will be printed to the log. Fill it with a pattern.
  u8 luminance[12] = ".,-~:;=!*#$@"; // don't keep null terminator
  u8 msg[TEXT_W * TEXT_H];

  f32 kpx = (f32)2 / TEXT_W;
  f32 kpy = (f32)2 / TEXT_H;
  for (i32 y = 0; y < TEXT_H; ++y) {
    for (i32 x = 0; x < TEXT_W; ++x) {
      f32 l = 0.25f * cosf32(kpx * x) + 0.25;
      f32 k = 0.25f * sinf32(kpy * y) + 0.25;
      i32 idx = (l + k) * (ARRAY_COUNT(luminance) - 1);
      msg[TEXT_W * y + x] = luminance[idx];
    }
  }
  u32 text_line = 0;

  f32 log_delay = 0;
  u32 log_line_prev = log_atomic_load_last_line(&g_log);
  (void)log_line_prev; // TODO:
  i32 offset_y = 0;

  while (1) {
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
    // Keyboard Input
    // Step through event loop once, updating input events
    event_loop_step(&loop);

    // ESC to exit
    if (loop.keycodes.e[KC_ESC]) {
      goto shutdown;
    }

    if (!loop.keycodes.e[KC_SPACE]) {
      log_delay += dt;
      if (log_delay > 0.3f) {
        log_delay = 0.0f;
        text_line += 1;
        u32 line_in_buf = text_line % TEXT_H;
        if (text_line % 10 == 0) {
          LOG_M(text_line, "Hold <SPACE> to Pause logging. Use <UP> and <DOWN> to scroll.");
        } else {
          LOG_M(text_line, (const char*)(msg + TEXT_W * line_in_buf));
        }
      }
    } else {
      if (!loop.keycodes.e[KC_UP]) {
        offset_y += 1;
      }
      if (!loop.keycodes.e[KC_DOWN]) {
        offset_y -= 1;
      }
    }
    i32 margin_y = 4;
    offset_y = clampi32(offset_y, -BUF_H + TEXT_H - margin_y, margin_y);

    // Draw
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    // Update text on the screen
    u32 log_line_last = log_atomic_load_last_line(&g_log);

    glUniform1i(glGetUniformLocation(text_prog, "u_offset_y"), offset_y);
    glUniform1i(glGetUniformLocation(text_prog, "u_line_last"), log_line_last);

    // OpenGL can't map buffer
    glBindBuffer(GL_TEXTURE_BUFFER, text_bo);

#if 1
    // TODO: for now simply copy full string buffer
    glBufferData(GL_TEXTURE_BUFFER, BUF_W * BUF_H, 0, GL_DYNAMIC_DRAW); // Orphan
    glBufferSubData(GL_TEXTURE_BUFFER, 0, BUF_W * BUF_H, g_log.buf);
#else
    if (log_line_last > log_line_prev) {
      u32 dlog_lines = log_line_last - log_line_prev;
      glBufferSubData(GL_TEXTURE_BUFFER,
          log_line_prev * TEXT_W,
          dlog_lines * TEXT_W,
          g_log.buf);
    } else {
      glBufferSubData(GL_TEXTURE_BUFFER, 0, log_line_last * TEXT_W, g_log.buf);
      glBufferSubData(
          GL_TEXTURE_BUFFER,
          log_line_prev * TEXT_W,
          TEXT_H * TEXT_W,
          g_log.buf);
    }
#endif

    // Draw text
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, TEXTS_COUNT);

    window_flush(&w);
  }

shutdown:
  print_avg_dt_fps(loop_s / loop_count);

  // Shutdown
  glDeleteShader(text_prog);

  glDeleteVertexArrays(1, &vao);

  window_shutdown(&w);
  event_loop_shutdown(&loop);
  log_shutdown(&g_log);

  exit(0);
}
