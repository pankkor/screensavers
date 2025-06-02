// Bitmap font
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/font

#include "common.h"

#include "res_font_256.h"
#include "res_ascii_anim.h"

enum {
  TEXT_W = 80,
  TEXT_H = 80,
  TEXTS_COUNT = 1,
};

u32 TEXT_COLOR_RGBA = 0xCFDFFFFF; // 0xRRGGBBAA

// Text that only fits on the screen
ALIGNED(16) u8 s_text[TEXT_W * TEXT_H] =
"                                                                                "
"Hello Bitmap Font!                                                              "
"1234567890-=`~!@#$%^&*(),.<>:\"/;'[]{}\\|                                         "
;

u8 s_luminance[12] = ".,-~:;=!*#$@"; // don't keep null terminator

// --------------------------------------
// GLSL
// --------------------------------------
static const char * const s_text_vert_src = "                                \r\
#version 410 core                                                            \r\
                                                                             \r\
uniform vec2 size;                                                           \r\
uniform uint color_rgba;                                                     \r\
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
  vec2 vert = verts[gl_VertexID] * size;                                     \r\
  vec2 uv = uvs[gl_VertexID];                                                \r\
  gl_Position = vec4(vert, 0.0, 1.0);                                        \r\
  f_uv = uv;                                                                 \r\
  f_color = rgba2vec4(color_rgba);                                           \r\
}                                                                            \r\
";

static const char * const s_text_frag_src = "                                \r\
#version 410 core                                                            \r\
in vec2 f_uv;                                                                \r\
in vec4 f_color;                                                             \r\
                                                                             \r\
uniform sampler2D font_tx;                                                   \r\
uniform usamplerBuffer text_buf;                                             \r\
                                                                             \r\
uniform ivec2 buf_size;                                                      \r\
uniform ivec2 glyphs_count; /* number of glyphs in atlas row and column */   \r\
                                                                             \r\
out vec4 frag_col;                                                           \r\
                                                                             \r\
void main(void) {                                                            \r\
  ivec2 buf_pos = ivec2(f_uv * buf_size);                                    \r\
  int buf_idx = buf_pos.x + buf_pos.y * buf_size.x;                          \r\
  uint c = texelFetch(text_buf, buf_idx).r;                                  \r\
  vec2 glyph_pos = vec2(c % glyphs_count.x, c / glyphs_count.y);             \r\
  vec2 uv = (glyph_pos + mod(f_uv * buf_size, 1.0)) / glyphs_count;          \r\
  float a = texture(font_tx, uv).r;                                          \r\
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
  window_init(&w, 0 /*is_full_screen*/);

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
  glBufferData(GL_TEXTURE_BUFFER, TEXT_W * TEXT_H, s_text, GL_STREAM_DRAW);

  glActiveTexture(GL_TEXTURE1);
  GLuint tbo;
  glGenTextures(1, &tbo);
  glBindTexture(GL_TEXTURE_BUFFER, tbo);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, text_bo);

  glUniform1i(glGetUniformLocation(text_prog, "font_tx"), 0);
  glUniform1i(glGetUniformLocation(text_prog, "text_buf"), 1);
  glUniform1f(glGetUniformLocation(text_prog, "iaspect"), iaspect);
  glUniform2f(glGetUniformLocation(text_prog, "size"), size[0], size[1]);
  glUniform1ui(glGetUniformLocation(text_prog, "color_rgba"), TEXT_COLOR_RGBA);
  glUniform2i(glGetUniformLocation(text_prog, "buf_size"), TEXT_W, TEXT_H);
  glUniform2i(glGetUniformLocation(text_prog, "glyphs_count"), FONT_GLYPHS_W, FONT_GLYPHS_H);

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

    // Animate background
    bg_anim_t = fmodf32(bg_anim_t + dt, 1.0f);

    for (i32 x = 0; x < TEXT_W; ++x) {
      for (i32 y = 3; y < TEXT_H; ++y) {
        f32 l = 0.25f * cosf32(bg_anim_t + 0.02f * x) + 0.25f;
        f32 k = 0.25f * sinf32(bg_anim_t + 0.02f * y) + 0.25f;
        i32 idx = (l + k) * ARRAY_COUNT(s_luminance) - 1;
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

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
