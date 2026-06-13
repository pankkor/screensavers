// Bitmap and SDF fonts
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/font

#include "common.h"

#include "res_ascii_anim.h"

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
  TEXT_W = 80,
  TEXT_H = 80,
  TEXTS_COUNT = 1,
};

u32 TEXT_COLOR_RGBA = 0xCFDFFFFF; // 0xRRGGBBAA
f32 BG_COLOR[4] = {0.2f, 0.2f, 0.2f, 1.0f};
f32 SDF_BOLD = 0.1f;

// Text that only fits on the screen
ALIGNED(16) u8 s_text[TEXT_W * TEXT_H] =
"Hello Bitmap Font!                                                              "
"Press <1> for SDF font.                                                         "
"Press <2> for Bitmap font.                                                      "
"                                         1234567890-=`~!@#$%^&*(),.<>:\"/;'[]{}\\|"
;

u8 s_luminance[12] = ".,-~:;=!*#$@"; // don't keep null terminator

// --------------------------------------
// GLSL
// --------------------------------------
static const char * const s_text_vert_src = "                                \r\
#version 410 core                                                            \r\
                                                                             \r\
uniform vec2 u_size;                                                         \r\
uniform uint u_color_rgba;                                                   \r\
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
  f_color = rgba2vec4(u_color_rgba);                                         \r\
}                                                                            \r\
";

static const char * const s_text_frag_src = "                                \r\
#version 410 core                                                            \r\
in vec2 f_uv;                                                                \r\
in vec4 f_color;                                                             \r\
                                                                             \r\
uniform sampler2D font_tx;                                                   \r\
uniform sampler2D sdf_tx;                                                    \r\
uniform usamplerBuffer u_text_buf;                                           \r\
                                                                             \r\
uniform ivec2 u_buf_size;                                                    \r\
uniform ivec2 u_glyphs_count;   // number of glyphs in atlas row and column  \r\
uniform int u_sdf_spread;       // spread used to generate SDF               \r\
uniform float u_sdf_bold;       // bolden in screen pixels                   \r\
uniform int u_is_sdf;                                                        \r\
                                                                             \r\
out vec4 frag_col;                                                           \r\
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
  vec2 cell = f_uv * u_buf_size;                                             \r\
  ivec2 buf_pos = ivec2(cell);                                               \r\
  int buf_idx = buf_pos.x + buf_pos.y * u_buf_size.x;                        \r\
  uint c = texelFetch(u_text_buf, buf_idx).r;                                \r\
  vec2 glyph_pos = vec2(c % u_glyphs_count.x, c / u_glyphs_count.x);         \r\
  vec2 uv = (glyph_pos + fract(cell)) / u_glyphs_count;                      \r\
                                                                             \r\
  // Cell bound UV derivatives                                               \r\
  vec2 duvdx = dFdx(cell) / vec2(u_glyphs_count);                            \r\
  vec2 duvdy = dFdy(cell) / vec2(u_glyphs_count);                            \r\
                                                                             \r\
  float a;                                                                   \r\
  if (u_is_sdf == 0) {                                                       \r\
    a = font_bitmap(font_tx, uv, duvdx, duvdy);                              \r\
  } else {                                                                   \r\
    a = font_sdf(sdf_tx, uv, duvdx, duvdy);                                  \r\
  }                                                                          \r\
  frag_col = f_color * a;                                                    \r\
}                                                                            \r\
";

// --------------------------------------
// Entry point (aka main)
// --------------------------------------
void start(void) {
  // Init
  struct event_loop loop;
  struct window w;

  event_loop_init(&loop);
  window_init(&w, /*vsync=*/0, /*high_dpi*/1, /*is_full_screen=*/0);

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
  glBufferData(GL_TEXTURE_BUFFER, TEXT_W * TEXT_H, s_text, GL_DYNAMIC_DRAW);

  GLuint tbo;
  glActiveTexture(GL_TEXTURE2);
  glGenTextures(1, &tbo);
  glBindTexture(GL_TEXTURE_BUFFER, tbo);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, text_bo);

  glUniform1i(glGetUniformLocation(text_prog, "font_tx"), 0);
  glUniform1i(glGetUniformLocation(text_prog, "sdf_tx"), 1);
  glUniform1i(glGetUniformLocation(text_prog, "u_text_buf"), 2);
  glUniform2f(glGetUniformLocation(text_prog, "u_size"), size[0], size[1]);
  glUniform1ui(glGetUniformLocation(text_prog, "u_color_rgba"), TEXT_COLOR_RGBA);
  glUniform2i(glGetUniformLocation(text_prog, "u_buf_size"), TEXT_W, TEXT_H);
  glUniform2i(glGetUniformLocation(text_prog, "u_glyphs_count"), FONT_GLYPHS_W,
      FONT_GLYPHS_H);
  glUniform1i(glGetUniformLocation(text_prog, "u_sdf_spread"), FONT_SDF_SPREAD);
  glUniform1f(glGetUniformLocation(text_prog, "u_sdf_bold"), SDF_BOLD);

  // Logic

  // Game loop
  f32 cpu_timer_freq  = read_cpu_timer_freq();
  f32 icpu_timer_freq = 1.0f / cpu_timer_freq;
  u64 tsc             = read_cpu_timer();

  f32 loop_s          = 0.0f;
  u64 loop_count      = 0;
  f32 print_dt_tsc    = tsc + 5.0f * cpu_timer_freq;

  i32 anim_w          = ANIM_ASCII_W;
  i32 anim_h          = ANIM_ASCII_H;
  i32 anim_f_count    = ANIM_ASCII_FRAME_COUNT;
  u32 anim_fps        = 2;

  f32 anim_t          = 0.0f; // not normalized, range [0, anim_f_count)

  // Animation position in text buffer in [l, r), where  l - left, r - right
  i32 anim_dst_lx     = MAX((TEXT_W - ANIM_ASCII_W) * 0.5f, 0.0f);
  i32 anim_dst_ly     = MAX((TEXT_H - ANIM_ASCII_H) * 0.5f, 0.0f);
  i32 anim_dst_rx     = MIN(anim_dst_lx + ANIM_ASCII_W, TEXT_W);
  i32 anim_dst_ry     = MIN(anim_dst_ly + ANIM_ASCII_H, TEXT_H);

  i32 anim_copy_w     = anim_dst_rx - anim_dst_lx;
  i32 anim_copy_h     = anim_dst_ry - anim_dst_ly;

  f32 bg_anim_t       = 0.0f; // normalized [0; 1.0)
  b32 is_sdf          = 0;

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

    // Hold space for SDF font
    b32 is_down_1 = loop.keycodes.e[KC_1];
    b32 is_down_2 = loop.keycodes.e[KC_2];
    if (is_down_1) {
      is_sdf = 1;
    }
    if (is_down_2) {
      is_sdf = 0;
    }

    // Text
    s_text[ 6] = is_sdf ? ' ' : 'B';
    s_text[ 7] = is_sdf ? 'S' : 'i';
    s_text[ 8] = is_sdf ? 'D' : 't';
    s_text[ 9] = is_sdf ? 'F' : 'm';
    s_text[10] = is_sdf ? ' ' : 'a';
    s_text[11] = is_sdf ? ' ' : 'p';

    // Animate background
    bg_anim_t = fmodf32(bg_anim_t + dt, 1.0f);

    for (i32 x = 0; x < TEXT_W; ++x) {
      for (i32 y = 4; y < TEXT_H; ++y) {
        f32 l = 0.25f * cosf32(bg_anim_t + 0.02f * x) + 0.25f;
        f32 k = 0.25f * sinf32(bg_anim_t + 0.02f * y) + 0.25f;
        i32 idx = (l + k) * (ARRAY_COUNT(s_luminance) - 1);
        s_text[x + TEXT_W * y] = s_luminance[idx];
      }
    }

    // ASCII animation
    anim_t = fmodf32(anim_t + dt * anim_fps, anim_f_count);
    i32 anim_idx = anim_t;
    u8 *anim_src = &s_anim_ascii[anim_w * anim_h * anim_idx];

    for (i32 y = 0; y < anim_copy_h; ++y) {
      i32 dst_y = anim_dst_ly + y;
      for (i32 x = 0; x < anim_copy_w; ++x) {
        i32 dst_x = anim_dst_lx + x;
        s_text[dst_x + TEXT_W * dst_y] = anim_src[x + anim_w * y];
      }
    }

    // Draw
    glUniform1i(glGetUniformLocation(text_prog, "u_is_sdf"), is_sdf);

    glViewport(0, 0, w.view_size_px[0], w.view_size_px[1]);

    glClearColor(BG_COLOR[0], BG_COLOR[1], BG_COLOR[2], BG_COLOR[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Update text on the screen
    glBindBuffer(GL_TEXTURE_BUFFER, text_bo);
    glBufferData(GL_TEXTURE_BUFFER, TEXT_W * TEXT_H, 0, GL_DYNAMIC_DRAW); // Orphan
    glBufferSubData(GL_TEXTURE_BUFFER, 0, TEXT_W * TEXT_H, s_text);

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

  exit(0);
}
