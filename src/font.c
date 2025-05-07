// Bitmap font
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/font

#include "common.h"

enum {
  TEXT_W = 80,
  TEXT_H = 40,
  TEXTS_COUNT = 1,
};

//                                                                              80
// Text that only fits on the screen                                             V
u8 s_text[TEXT_W * TEXT_H] =
"################################################################################"
"############################# H E L L O   T E X T ! ############################"
"################################################################################"
"________________________________________________________________________________"
"_____________________________________$$$$$$$____________________________________"
"____________________________________$$$$$$$$$$__________________________________"
"____________________________________$$$$$$$$$$$_________________________________"
"_____________________________________$$$$$$$$$$$$$$_____________________________"
"______________________________________$$$$$$$$$$$_______________________________"
"_________________________________________$$$$$$$$$$$$$__________________________"
"_______________________________________$$$$$$$$$$_______________________________"
"_____________________________________$$$$$$$$$$$$$$$____________________________"
"____________________________$$$______$$$$$$$$$$$$$$_____________________________"
"__________________________$$$$$$$$_____$$$$$$__$$$$$____________________________"
"_________________________$$$$$$$$$$_____$$$$____$$$$$___________________________"
"_______________________$$$$$$_$$$$$$$$__$$$$______$$$$__________________________"
"______________________$$$$$_____$$$$$$$$_$$$$_______$$$_________________________"
"____________________$$$$$_________$$$$$$$$$$$$_______$$$________________________"
"___________________$$$_____________$$$$$$$$$$$________$$$_______________________"
"_________________$$$________________$$$$$$$$$$________$$$$$$____________________"
"______________$$$$$$__________________$$$$$$$___________________________________"
"________________________________________________________________________________"
"________________________________________________________________________________"
"_____________________________________$$$$$$$____________________________________"
"____________________________________$$$$$$$$$$__________________________________"
"____________________________________$$$$$$$$$$$_________________________________"
"_____________________________________$$$$$$$$$$$$$$_____________________________"
"______________________________________$$$$$$$$$$$_______________________________"
"_________________________________________$$$$$$$$$$$$$__________________________"
"_______________________________________$$$$$$$$$$_______________________________"
"_____________________________________$$$$$$$$$$$$$$$____________________________"
"____________________________$$$______$$$$$$$$$$$$$$_____________________________"
"__________________________$$$$$$$$_____$$$$$$__$$$$$____________________________"
"_________________________$$$$$$$$$$_____$$$$____$$$$$___________________________"
"_______________________$$$$$$_$$$$$$$$__$$$$______$$$$__________________________"
"______________________$$$$$_____$$$$$$$$_$$$$_______$$$_________________________"
"____________________$$$$$_________$$$$$$$$$$$$_______$$$________________________"
"___________________$$$_____________$$$$$$$$$$$________$$$_______________________"
"_________________$$$________________$$$$$$$$$$________$$$$$$____________________"
"______________$$$$$$__________________$$$$$$$___________________________________"
// <- 40
;

// --------------------------------------
// GLSL
// --------------------------------------
static const char * const s_text_vert_src = "                                  \
#version 410 core                                                              \
                                                                               \
uniform vec2 size;                                                             \
                                                                               \
out vec2 f_uv;                                                                 \
                                                                               \
const vec2 verts[4] = vec2[](                                                  \
  /*vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(-0.5, 0.5), vec2(0.5, 0.5)*/       \
  vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0)           \
);                                                                             \
const vec2 uvs[4] = vec2[](                                                    \
  vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0)               \
);                                                                             \
                                                                               \
void main(void) {                                                              \
  vec2 vert = verts[gl_VertexID] * size;                                       \
  vec2 uv = uvs[gl_VertexID];                                                  \
  gl_Position = vec4(vert, 0.0, 1.0);                                          \
  f_uv = uv;                                                                   \
}                                                                              \
";

static const char * const s_text_frag_src = "                                  \
#version 410 core                                                              \
in vec2 f_uv;                                                                  \
                                                                               \
uniform sampler2D font_tx;                                                     \
uniform usamplerBuffer text_buf;                                               \
                                                                               \
out vec4 frag_col;                                                             \
                                                                               \
void main(void) {                                                              \
  int buf_i = int(f_uv.s * 80.0) + int(f_uv.t * 40.0) * 80;                    \
  uint gi = texelFetch(text_buf, buf_i).r;                                     \
  uint giy = gi / 16;                                                          \
  uint gix = gi - giy * 16;                                                    \
  vec2 offset = vec2(gix, giy) / 16.0;                                         \
  vec2 uv = mod(f_uv, vec2(1/80.0, 1/40.0)) * vec2(80.0, 40.0) / 16.0  + offset;\
  vec4 t = texture(font_tx, uv);                                               \
  frag_col = vec4(uv.st, 0.0, 1.0);                                            \
  frag_col = t;                                                                \
}                                                                              \
";

#include "res_font_1024.h"

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
  f32 size[2];
  size[0] = 21 * 80 / w.rect[2];
  size[1] = 21 * 40 / w.rect[3];

  GLuint vao;
  GLuint text_bo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &text_bo);

  glUseProgram(text_prog);
  glBindVertexArray(vao);

  // Font texture
  glActiveTexture(GL_TEXTURE0);
  GLuint font_tx;
  glGenTextures(1, &font_tx);
  glBindTexture(GL_TEXTURE_2D, font_tx);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FONT_TX_W, FONT_TX_H, 0,
      GL_RGBA, GL_UNSIGNED_BYTE, s_font_tx_data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // On screen text buffer
  glBindBuffer(GL_TEXTURE_BUFFER, text_bo);
  glBufferData(GL_TEXTURE_BUFFER, TEXT_W * TEXT_H, s_text, GL_DYNAMIC_DRAW);

  glActiveTexture(GL_TEXTURE1);
  GLuint tbo;
  glGenTextures(1, &tbo);
  glBindTexture(GL_TEXTURE_BUFFER, tbo);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, text_bo);

  glUniform1i(glGetUniformLocation(text_prog, "font_tx"), 0);
  glUniform1i(glGetUniformLocation(text_prog, "text_buf"), 1);
  glUniform1f(glGetUniformLocation(text_prog, "iaspect"), iaspect);
  glUniform2f(glGetUniformLocation(text_prog, "size"), size[0], size[1]);

  // Logic

  // Game loop
  f32 cpu_timer_freq  = read_cpu_timer_freq();
  f32 icpu_timer_freq = 1.0f / cpu_timer_freq;
  u64 tsc             = read_cpu_timer();

  f32 loop_s          = 0.0f;
  u64 loop_count      = 0;
  f32 print_dt_tsc    = tsc + 5.0f * cpu_timer_freq;

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
    // Draw
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

    // Update text on the screen
    glBindBuffer(GL_TEXTURE_BUFFER, text_bo);
    // TODO: orphaning?
    glBufferData(GL_TEXTURE_BUFFER, TEXT_W * TEXT_H, 0, GL_DYNAMIC_DRAW); // Orphan
    glBufferSubData(GL_TEXTURE_BUFFER, 0, TEXT_W * TEXT_H, s_text);

    // Draw text
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, TEXTS_COUNT);

    window_flush(&w);
  }
  print_avg_dt_fps(loop_s / loop_count);

shutdown:
  // Shutdown
  glDeleteShader(text_prog);

  glDeleteVertexArrays(1, &vao);

  window_shutdown(&w);
  event_loop_shutdown(&loop);

  exit(0);
}
