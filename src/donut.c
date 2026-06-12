// ASCII donut
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/donut

#include "common.h"

#include "res_font_256.h"

enum {
  TEXT_W = 80,
  TEXT_H = 80,
  TEXTS_COUNT = 1,
};

u32 TEXT_COLOR_RGBA = 0xFFCFDFFF; // 0xRRGGBBAA

// Text that only fits on the screen
ALIGNED(16) u8 s_text[TEXT_W * TEXT_H];

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
  vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(-0.5, 0.5), vec2(0.5, 0.5)         \r\
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

enum { W = TEXT_W, H = TEXT_H };
f32 depth[W * H];

void donut(f32 turns) {
  // Clean inverse Z depth buffer
  for (i32 i = 0; i < ARRAY_COUNT(depth); ++i) {
    depth[i] = 0.0f;
  }

  // Torus
  f32 R1 = 1.0f; // minor radius (radius of the tube)
  f32 R2 = 2.0f; // major radius (distance from the center of torus to the tube)

  // Projection on the screen
  // p' = p * (Z'/z), where Z' - is constant screen Z.
  f32 DONUT_Z = 5.0f; // Z - distance from the donut to the viewer
  // SCREEN_Z - Z position of the screen. Choose it so donit's edge is ~3/4 to
  // the scren edge.
  // X axis: R1+R2 is the farthest point on the torus at object space obj_z = 0.
  // We want it to be 3/4 of the screen edge from center, or 3/8 from origin.
  // SCREEN_W * 3/8 = SCREEN_Z * (R1 + R2) / (DONUT_Z + obj_z), obj_z = 0.
  // From that SCREEN_Z is:
  f32 SCREEN_Z = 3.0f / 8.0f * W * DONUT_Z / (R1 + R2);

  // L - direction to light source.
  f32 L[3] = {-0.57735027f, 0.57735027f, -0.57735027f};
  f32 DIFFUSE = 0.01f;

  // Theta - torus tube circle (R1).
  // Phi - center of revolution of the torus (R2).
  // Choose delta Theta and Phi angles small enough so there are no visible gaps
  // Angles are in turns [0; 2pi)
  f32 DTHETA = 0.006f;
  f32 DPHI = 0.001;

  // Torus object space rotation
  f32 axis[3] = {0.0f, -0.70710678f, 0.70710678f};
  f32 q[4];
  q4_axis_angle(q, axis, turns);

  for (f32 theta = 0.0f; theta < 1.0f; theta += DTHETA) {
    f32 cos_theta = cosf32(theta);
    f32 sin_theta = sinf32(theta);

    for (f32 phi = 0.0f; phi < 1.0f; phi += DPHI) {
      f32 cos_phi = cosf32(phi);
      f32 sin_phi = sinf32(phi);

      // Point on torus, object space
      f32 v[3] = {
       (R2 + R1 * cos_theta) * cos_phi,
       R1 * sin_theta,
       -(R2 + R1 * cos_theta) * sin_phi,
      };

      rot_v3_q4(v, v, q); // Rotated point, object space

      v[2] += DONUT_Z; // Translate away from the viewer along Z axis

      f32 iz = 1.0f / v[2]; // Inverse Z for depth test

      // Projection p' = p * Z'/z.
      // Invert Y since we render into char buffer.
      i32 xp = (i32)(W / 2.0f + SCREEN_Z * iz * v[0]);
      i32 yp = (i32)(H / 2.0f - SCREEN_Z * iz * v[1]);

      // Check if projected point is in our screen buffer
      i32 idx = xp + yp * W;
      if (xp >= 0 && xp < W && yp >= 0 && yp < H) {
        if (iz > depth[idx]) {
          // Torus normal, object space
          f32 n[3] = {cos_theta * cos_phi, sin_theta, -cos_theta * sin_phi};
          rot_v3_q4(n, n, q); // Rotated normal

          // Shade N • L
          f32 lum = MAX(DIFFUSE, dot_v3(n, L));

          // Luminance to ASCII
          i32 lum_idx = lum * (ARRAY_COUNT(s_luminance) - 1);

          u8 c = s_luminance[lum_idx];

          depth[idx] = iz;
          s_text[idx] = c;
        }
      }
    }
  }
}

// --------------------------------------
// Entry point (aka main)
// --------------------------------------
void start(void) {
  // Init
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

  // Donut
  f32 turns           = 0.0f;   // normalized [0; 1.0) in turns [0; 2pi)

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
      goto shutdown; }

    // Clear
    for (i32 i = 0; i < TEXT_W * TEXT_H; ++i) {
      s_text[i] = ' ';
    }

    turns = fmodf32(turns + 0.5f * dt, 1.0f);
    donut(turns);

    // Draw
    glViewport(0, 0, w.view_size_px[0], w.view_size_px[1]);

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
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
