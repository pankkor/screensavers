// View mmaped ring buffer log file with bitmap and SDF fonts
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/log_view

#include "common.h"

#if 0 // Square monospace font (glyph.width == glyph.height)

#include "res_font_square_1024.h"
#define FONT_TX_W           FONT_SQUARE_TX_W
#define FONT_TX_H           FONT_SQUARE_TX_H
#define FONT_GLYPHS_W       FONT_SQUARE_GLYPHS_W
#define FONT_GLYPHS_H       FONT_SQUARE_GLYPHS_H
#define S_FONT_TX_DATA      s_font_square_tx_data

#include "res_font_square_sdf_1024.h"
#define FONT_SDF_TX_W       FONT_SQUARE_SDF_TX_W
#define FONT_SDF_TX_H       FONT_SQUARE_SDF_TX_H
#define FONT_SDF_GLYPHS_W   FONT_SQUARE_SDF_GLYPHS_W
#define FONT_SDF_GLYPHS_H   FONT_SQUARE_SDF_GLYPHS_H
#define FONT_SDF_SPREAD     FONT_SQUARE_SDF_SPREAD
#define S_FONT_SDF_TX_DATA  s_font_square_sdf_tx_data

#else // Roboto Monospace font

#include "res_font_roboto_1024.h"
#define FONT_TX_W           FONT_ROBOTO_TX_W
#define FONT_TX_H           FONT_ROBOTO_TX_H
#define FONT_GLYPHS_W       FONT_ROBOTO_GLYPHS_W
#define FONT_GLYPHS_H       FONT_ROBOTO_GLYPHS_H
#define S_FONT_TX_DATA      s_font_roboto_tx_data

#include "res_font_roboto_sdf_1024.h"
#define FONT_SDF_TX_W       FONT_ROBOTO_SDF_TX_W
#define FONT_SDF_TX_H       FONT_ROBOTO_SDF_TX_H
#define FONT_SDF_GLYPHS_W   FONT_ROBOTO_SDF_GLYPHS_W
#define FONT_SDF_GLYPHS_H   FONT_ROBOTO_SDF_GLYPHS_H
#define FONT_SDF_SPREAD     FONT_ROBOTO_SDF_SPREAD
#define S_FONT_SDF_TX_DATA  s_font_roboto_sdf_tx_data

#endif


static_assert(FONT_GLYPHS_W == FONT_SDF_GLYPHS_W);
static_assert(FONT_GLYPHS_H == FONT_SDF_GLYPHS_H);

enum {
  BUF_W = LOG_LINE_BYTES,
  BUF_H = LOG_LINES,
};

u32 TEXT_COLOR_RGBA = 0xCFDFFFFF; // 0xRRGGBBAA
f32 BG_COLOR[4] = {0.2f, 0.2f, 0.2f, 1.0f};

// --------------------------------------
// GLSL
// --------------------------------------
static const char * const s_text_vert_src  = GLSL_V410 "                     \r\
uniform vec2 u_resolution;                                                   \r\
uniform vec4 u_rect;                                                         \r\
uniform ivec2 u_buf_size; /* pow of 2 */                                     \r\
uniform ivec2 u_buf_window;                                                  \r\
uniform ivec2 u_offset;                                                      \r\
                                                                             \r\
out vec2 f_win_pos;                                                          \r\
                                                                             \r\
void main(void) {                                                            \r\
  vec2 v = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);                    \r\
  vec2 uv = vec2(v.x, 1.0 - v.y);                                            \r\
  vec2 vert = (u_rect.xy + v * u_rect.zw) / u_resolution; /* in [0, 1] */    \r\
  vec2 ndc = 2.0 * vert - 1.0; /* in [-1, 1] */                              \r\
                                                                             \r\
  f_win_pos = uv * u_buf_window + u_offset;                                  \r\
  gl_Position = vec4(ndc, 0.0, 1.0);                                         \r\
}                                                                            \r\
";

static const char * const s_text_frag_src  = GLSL_V410 "                     \r\
uniform sampler2D font_tx;                                                   \r\
uniform sampler2D sdf_tx;                                                    \r\
uniform usamplerBuffer u_text_buf; // text ring buffer of size u_buf_size    \r\
                                                                             \r\
uniform vec4 u_rect;                                                         \r\
uniform uint u_color;         // RGBA                                        \r\
uniform ivec2 u_buf_size;     // pow of 2                                    \r\
uniform ivec2 u_buf_window;                                                  \r\
uniform ivec2 u_glyphs_count; // number of glyphs in atlas row and column    \r\
uniform int u_line_last;                                                     \r\
uniform int u_sdf_spread;     // spread used to generate SDF                 \r\
uniform float u_sdf_bold;     // bolden in SDF space                         \r\
uniform int u_is_sdf;                                                        \r\
                                                                             \r\
in vec2 f_win_pos;                                                           \r\
out vec4 frag_col;                                                           \r\
                                                                             \r\
/* Returns 0 if x is outside of [l, r) */                                    \r\
float mask_range(float x, float l, float r) {                                \r\
  return step(l, x) * (1.0 - step(r, x));                                    \r\
}                                                                            \r\
                                                                             \r\
vec4 rgba2vec4(uint rgba) {                                                  \r\
  return vec4((rgba >> 24) & 0xFFu, (rgba >> 16) & 0xFFu,                    \r\
    (rgba >> 8) & 0xFFu, rgba & 0xFFu) / 255.0;                              \r\
}                                                                            \r\
                                                                             \r\
float font_bitmap(sampler2D r8_tx, vec2 uv, vec2 duvdx, vec2 duvdy) {        \r\
  return textureGrad(r8_tx, uv, duvdx, duvdy).r;                             \r\
}                                                                            \r\
                                                                             \r\
// SDF with AA                                                               \r\
float font_sdf(sampler2D r8_tx, vec2 uv, vec2 duvdx, vec2 duvdy) {           \r\
  float to_sdf_space = 0.5 / u_sdf_spread; // to SDF space (-spread, +spread)\r\
  float d = textureGrad(r8_tx, uv, duvdx, duvdy).r;                          \r\
  float d_norm = d - 0.5; // [-0.5, 0.5] in SDF space                        \r\
                                                                             \r\
  vec2 tx_size = vec2(textureSize(r8_tx, 0));                                \r\
  float texels_per_px = length(vec2(                                         \r\
    length(duvdx * tx_size),                                                 \r\
    length(duvdy * tx_size)                                                  \r\
  ));                                                                        \r\
  float smoothing_ideal = texels_per_px * to_sdf_space;                      \r\
  // Account for room left in texture spread after accounting for bold       \r\
  float smoothing_max = 0.5 - abs(u_sdf_bold);                               \r\
  float smoothing = min(smoothing_ideal, smoothing_max);                     \r\
  float a = smoothstep(-smoothing, smoothing, d_norm + u_sdf_bold);          \r\
  return a;                                                                  \r\
}                                                                            \r\
                                                                             \r\
void main(void) {                                                            \r\
  vec4 color = rgba2vec4(u_color);                                           \r\
  int buf_mod_mask = u_buf_size.y - 1;                                       \r\
                                                                             \r\
  vec2 win_pos = f_win_pos;                                                  \r\
                                                                             \r\
  // Operate in logical space w/o u_line_last offset (first line at 0)       \r\
  // Offset for - window hight, so last line is at the bottom of a window    \r\
  int buf_row = int(win_pos.x);                                              \r\
  float buf_linef = win_pos.y + u_buf_size.y - u_buf_window.y;               \r\
  float mask_x = mask_range(win_pos.x, 0.0, float(u_buf_size.x));            \r\
  float mask_y = mask_range(buf_linef, 0.0, float(u_buf_size.y));            \r\
  float mask = mask_x * mask_y;                                              \r\
  // Move to ring buffer space physical                                      \r\
  int buf_line = (int(buf_linef) + u_line_last) & buf_mod_mask;              \r\
                                                                             \r\
  int buf_idx = buf_row + buf_line * u_buf_size.x;                           \r\
  uint c = texelFetch(u_text_buf, buf_idx).r;                                \r\
                                                                             \r\
  vec2 glyph_pos = vec2(c % u_glyphs_count.x, c / u_glyphs_count.x);         \r\
  vec2 uv = (glyph_pos + fract(win_pos)) / u_glyphs_count;                   \r\
                                                                             \r\
  // Cell bound UV derivatives                                               \r\
  vec2 duvdx = dFdx(win_pos) / vec2(u_glyphs_count);                         \r\
  vec2 duvdy = dFdy(win_pos) / vec2(u_glyphs_count);                         \r\
                                                                             \r\
  float a;                                                                   \r\
  if (u_is_sdf == 0) {                                                       \r\
    a = font_bitmap(font_tx, uv, duvdx, duvdy);                              \r\
  } else {                                                                   \r\
    a = font_sdf(sdf_tx, uv, duvdx, duvdy);                                  \r\
  }                                                                          \r\
                                                                             \r\
  // Dim older recent lines                                                  \r\
  int dist = (buf_line - u_line_last) & buf_mod_mask;                        \r\
  a *= mix(0.4, 1.0, float(dist) / u_buf_size.y);                            \r\
  a *= mask;                                                                 \r\
  frag_col = color * a;                                                      \r\
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

  f32 aspect  = (f32)w.view_size_px[0] / w.view_size_px[1];
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

  GLuint font_tx;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &font_tx);
  glBindTexture(GL_TEXTURE_2D, font_tx);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_TX_W, FONT_TX_H, 0, GL_RED,
      GL_UNSIGNED_BYTE, S_FONT_TX_DATA);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // No mipmaps for bitmap font atlas

  GLuint sdf_tx;
  glActiveTexture(GL_TEXTURE1);
  glGenTextures(1, &sdf_tx);
  glBindTexture(GL_TEXTURE_2D, sdf_tx);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_SDF_TX_W, FONT_SDF_TX_H, 0,
      GL_RED, GL_UNSIGNED_BYTE, S_FONT_SDF_TX_DATA);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // No mipmaps for SDF font atlas

  // On screen text buffer
  glBindBuffer(GL_TEXTURE_BUFFER, text_bo);
  glBufferData(GL_TEXTURE_BUFFER, BUF_W * BUF_H, 0, GL_DYNAMIC_DRAW);

  GLuint tbo;
  glActiveTexture(GL_TEXTURE2);
  glGenTextures(1, &tbo);
  glBindTexture(GL_TEXTURE_BUFFER, tbo);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, text_bo);

  f32 rect[4] = {0, 0, w.view_size_px[0], w.view_size_px[1]};

  glUniform1i(glGetUniformLocation(text_prog, "font_tx"), 0);
  glUniform1i(glGetUniformLocation(text_prog, "sdf_tx"), 1);
  glUniform1i(glGetUniformLocation(text_prog, "u_text_buf"), 2);
  glUniform1ui(glGetUniformLocation(text_prog, "u_color"), TEXT_COLOR_RGBA);
  glUniform2i(glGetUniformLocation(text_prog, "u_buf_size"), BUF_W, BUF_H);
  glUniform2i(glGetUniformLocation(text_prog, "u_glyphs_count"), FONT_GLYPHS_W,
      FONT_GLYPHS_H);
  glUniform2f(glGetUniformLocation(text_prog, "u_resolution"), rect[2], rect[3]);
  glUniform1i(glGetUniformLocation(text_prog, "u_sdf_spread"), FONT_SDF_SPREAD);

  // Logic

  // Game loop
  f32 cpu_timer_freq  = read_cpu_timer_freq();
  f32 icpu_timer_freq = 1.0f / cpu_timer_freq;
  u64 tsc             = read_cpu_timer();

  f32 loop_s          = 0.0f;
  u64 loop_count      = 0;
  f32 print_dt_tsc    = tsc + 5.0f * cpu_timer_freq;
  u64 frame_num   = 0;
  (void)frame_num;

  // Create text that will be printed to the log. Fill it with a pattern.
  u8 luminance[12] = ".,-~:;=!*#$@"; // don't keep null terminator
  u8 msg[BUF_W * BUF_H];

  f32 kpx = (f32)2 / BUF_W;
  f32 kpy = (f32)2 / BUF_H;
  for (i32 y = 0; y < BUF_H; ++y) {
    for (i32 x = 0; x < BUF_W; ++x) {
      f32 l = 0.25f * cosf32(kpx * x) + 0.25;
      f32 k = 0.25f * sinf32(kpy * y) + 0.25;
      i32 idx = (l + k) * (ARRAY_COUNT(luminance) - 1);
      msg[BUF_W * y + x] = luminance[idx];
    }
  }
  u32 text_line         = 0;

  f32 log_delay         = 0;
  u32 log_line_prev     = log_atomic_load_last_line(&g_log);
  (void)log_line_prev; // TODO:

  i32 offset[2]         = {0};
  f32 dzoom             = 0.0f;
  f32 dsdf_bold         = 0.1f;
  b32 is_log_view_shown = 1;
  b32 is_sdf            = 1;
  b32 is_sdf_old        = is_sdf;

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
    struct keycodes old_kcs = loop.keycodes;
    event_loop_step(&loop);
    struct keycodes kcs = loop.keycodes;

    // ESC to exit
    if (kcs.e[KC_ESC]) {
      goto shutdown;
    }

    b32 is_up_grave = keycode_changed_to_up(KC_GRAVE, &old_kcs, &kcs);
    if (is_up_grave) {
      is_log_view_shown = !is_log_view_shown;
    }

    b32 is_up_1 = keycode_changed_to_up(KC_1, &old_kcs, &kcs);
    b32 is_up_2 = keycode_changed_to_up(KC_2, &old_kcs, &kcs);
    if (is_up_1) {
      is_sdf = 1;
    }
    if (is_up_2) {
      is_sdf = 0;
    }

    if (kcs.e[KC_EQUAL]) {
      if (kcs.e[KC_SHIFT]) {
        dsdf_bold += 0.001;
      } else {
        dzoom += 0.01;
      }
    }
    if (kcs.e[KC_MINUS]) {
      if (kcs.e[KC_SHIFT]) {
        dsdf_bold -= 0.001;
      } else {
        dzoom -= 0.01;
      }
    }

    if (kcs.e[KC_LEFT]) {
      offset[0] -= 1;
    }
    if (kcs.e[KC_RIGHT]) {
      offset[0] += 1;
    }
    if (kcs.e[KC_UP]) {
      offset[1] -= 1;
    }
    if (kcs.e[KC_DOWN]) {
      offset[1] += 1;
    }

    // Don't add text when SPACE is held
    if (!kcs.e[KC_SPACE]) {
      log_delay += dt;
      if (log_delay > 0.2f) {
        log_delay = 0.0f;
        if (text_line % 30 == 0) {
          LOG_M(text_line + 1, "Hold <SPACE> to Pause logging.");
          LOG_M(text_line + 2, "Press <ESC> to exit.");
          LOG_M(text_line + 3, "Use <UP>, <DOWN>, <LEFT>, <RIGHT> to scroll.)");
          LOG_M(text_line + 4, "Use '+' and '-' to zoom in and out.");
          LOG_M(text_line + 5, "Use <SHIFT>+'+' and <SHIFT>+'-' to change boldness of SDF font.");
          LOG_M(text_line + 6, "Press '1' for SDF font.");
          LOG_M(text_line + 7, "Press '2' for bitmap font.");
          text_line += 7;
        } else {
          u32 line_in_buf = text_line % BUF_H;
          LOG_M(text_line + 1, (const char*)(msg + BUF_W * line_in_buf));
          text_line += 1;
        }
      }
      if (is_sdf != is_sdf_old) {
        LOG_M(text_line + 1,
            is_sdf
              ? "                            Font: SDF                                  "
              : "                            Font: Bitmap                               ");
        text_line += 1;
      }
    }

    if (is_log_view_shown) {
      rect[1] = MAX(rect[1] - 7000.0f * dt, 0.0f);
    } else {
      rect[1] = MIN(rect[1] + 7000.0f * dt, rect[3]);
    }

    dsdf_bold = clampf32(dsdf_bold, -0.2f, 0.2f);
    f32 sdf_bold = 0.0f + dsdf_bold;

    dzoom = clampf32(dzoom, -0.5f, 10.0f);
    f32 zoom = 1.0f + dzoom;

    f32 glyph_size_zoomed[2];
    i32 buf_win[2];
    f32 glyph_size[2] = {
      is_sdf ? (f32)FONT_SDF_TX_W / FONT_SDF_GLYPHS_W : (f32)FONT_TX_W / FONT_GLYPHS_W,
      is_sdf ? (f32)FONT_SDF_TX_H / FONT_SDF_GLYPHS_H : (f32)FONT_TX_H / FONT_GLYPHS_H,
    };

    f32 glyph_scale = zoom * rect[2] / BUF_W / glyph_size[0];
    glyph_size_zoomed[0] = glyph_size[0] * glyph_scale;
    glyph_size_zoomed[1] = glyph_size[1] * glyph_scale;
    buf_win[0] = rect[2] / glyph_size_zoomed[0];
    buf_win[1] = rect[3] / glyph_size_zoomed[1];

    // Center zoomed buffer (round up to i32)
    i32 offset_zoom_x = MIN((BUF_W - buf_win[0]) * 0.5f + 0.5f, 0.0f);

    i32 margin[2] = {0};
    offset[0] = clamp_minmax_i32(
        offset[0],
        -margin[0],
        MAX(BUF_W - buf_win[0], 0) + margin[0]);
    offset[1] = clamp_minmax_i32(
        offset[1],
        MIN(-BUF_H + buf_win[1], 0) - margin[1],
        margin[1]);

    // Draw
    glViewport(0, 0, w.view_size_px[0], w.view_size_px[1]);

    glClearColor(BG_COLOR[0], BG_COLOR[1], BG_COLOR[2], BG_COLOR[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Update text on the screen
    u32 log_line_last = log_atomic_load_last_line(&g_log);

    glUniform1i(glGetUniformLocation(text_prog, "u_is_sdf"), is_sdf);
    glUniform4f(glGetUniformLocation(text_prog, "u_rect"), rect[0], rect[1],
        rect[2], rect[3]);
    glUniform2i(glGetUniformLocation(text_prog, "u_offset"),
        offset[0] + offset_zoom_x, offset[1]);
    glUniform1i(glGetUniformLocation(text_prog, "u_line_last"), log_line_last);
    glUniform2i(glGetUniformLocation(text_prog, "u_buf_window"), buf_win[0],
        buf_win[1]);
    glUniform1f(glGetUniformLocation(text_prog, "u_sdf_bold"), sdf_bold);

    // OpenGL can't map buffer
    glBindBuffer(GL_TEXTURE_BUFFER, text_bo);

#if 1
    // Copy all strings for simplicity
    // Orphan buffer
    glBufferData(GL_TEXTURE_BUFFER, BUF_W * BUF_H, 0, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_TEXTURE_BUFFER, 0, BUF_W * BUF_H, g_log.buf);
#else
    // Copy only changed strings
    if (log_line_last > log_line_prev) {
      u32 dlog_lines = log_line_last - log_line_prev;
      glBufferSubData(GL_TEXTURE_BUFFER,
          log_line_prev * BUF_W,
          dlog_lines * BUF_W,
          g_log.buf);
    } else {
      glBufferSubData(GL_TEXTURE_BUFFER, 0, log_line_last * BUF_W, g_log.buf);
      u32 dlog_lines = BUF_H - log_line_last;
      glBufferSubData(
          GL_TEXTURE_BUFFER,
          log_line_prev * BUF_W,
          dlog_lines * BUF_W,
          g_log.buf);
    }
#endif

    // Draw text
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    window_flush(&w);

    ++frame_num;
    is_sdf_old = is_sdf;
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
