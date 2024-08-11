// Common include

// --------------------------------------
// Types
// --------------------------------------
typedef signed char         i8;
typedef unsigned char       u8;
typedef short               i16;
typedef unsigned short      u16;
typedef int                 i32;
typedef unsigned int        u32;
typedef long long           i64;
typedef unsigned long long  u64;
typedef float               f32;
typedef double              f64;
typedef i32                 b32;

#define U64_MAX             -1UL

#define FORCE_INLINE        inline __attribute__((always_inline))
#define NO_RETURN           __attribute__((noreturn))
#define ALIGNED(x)          __attribute__((aligned(x)))

#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#include <CoreGraphics/CoreGraphics.h>

// --------------------------------------
// Private CoreGraphics API
// --------------------------------------
typedef enum {
  kCGSOrderBelow = -1,
  kCGSOrderOut,       // hides the window
  kCGSOrderAbove,
  kCGSOrderIn         // shows the window
} CGSWindowOrderingMode;

typedef int CGSConnectionID;
typedef int CGSSurfaceID;
typedef CFTypeRef CGSRegionRef;

extern CGSConnectionID CGSMainConnectionID(void);

extern CGError CGSNewWindow(CGSConnectionID cid,
    CGWindowBackingType backingType, CGFloat left, CGFloat top,
    CGSRegionRef region, CGWindowID *outWID);

extern CGError CGSReleaseWindow(CGSConnectionID cid, CGWindowID wid);

extern CGContextRef CGWindowContextCreate(CGSConnectionID cid,
    CGWindowID wid, CFDictionaryRef options);

extern CGError CGSFlushWindow(CGSConnectionID cid, CGWindowID wid,
    CGSRegionRef flushRegion);

extern CGError CGSNewRegionWithRect(const CGRect *rect, CGSRegionRef *out);

extern CGError CGSSetWindowLevel(CGSConnectionID cid, CGWindowID wid,
    CGWindowLevel level);

extern CGError CGSOrderWindow(CGSConnectionID cid, CGWindowID wid,
    CGSWindowOrderingMode mode, CGWindowID relativeToWID);

extern CGError CGSSetWindowOpacity(CGSConnectionID cid, CGWindowID wid,
    bool isOpaque);

extern CGError CGSSetWindowTags (const CGSConnectionID cid, CGWindowID wid,
    int *tag, int tagSize); // tag could be i32 or i64 with tagSize 32 or 64

extern CGError CGSAddSurface(CGSConnectionID cid, CGWindowID wid,
    CGSSurfaceID *outSID);

extern CGError CGSOrderSurface(CGSConnectionID cid, CGWindowID wid,
    CGSSurfaceID surface, CGSSurfaceID otherSurface, int place);

extern CGError CGSSetSurfaceBounds(CGSConnectionID cid, CGWindowID wid,
    CGSSurfaceID sid, CGRect bounds);

extern CGLError CGLSetSurface(CGLContextObj glctx, CGSConnectionID cid,
    CGWindowID wid, CGSSurfaceID sid);

// --------------------------------------
// syscall
// --------------------------------------
i64 syscall1(i64 sys_num, i64 a0) {
  i64 ret;
  __asm__ volatile (
    "mov x16, %[sys_num]\n"   // syscall number
    "mov x0,  %[a0]\n"        // a0
    "svc      0x80\n"
    "mov %0,  x0\n"
    : "=r" (ret)
    : [sys_num] "r" (sys_num), [a0] "r" (a0)
    : "x16", "x0"
  );
  return ret;
}

i64 syscall2(i64 sys_num, i64 a0, i64 a1) {
  i64 ret;
  __asm__ volatile (
    "mov x16, %[sys_num]\n"
    "mov x0,  %[a0]\n"
    "mov x1,  %[a1]\n"
    "svc      0x80\n"
    "mov %0,  x0\n"
    : "=r" (ret)
    : [sys_num] "r" (sys_num), [a0] "r" (a0), [a1] "r" (a1)
    : "x16", "x0", "x1"
  );
  return ret;
}

i64 syscall3(i64 sys_num, i64 a0, i64 a1, i64 a2) {
  i64 ret;
  __asm__ volatile (
    "mov x16, %[sys_num]\n"
    "mov x0,  %[a0]\n"
    "mov x1,  %[a1]\n"
    "mov x2,  %[a2]\n"
    "svc      0x80\n"
    "mov %0,  x0\n"
    : "=r" (ret)
    : [sys_num] "r" (sys_num), [a0] "r" (a0), [a1] "r" (a1), [a2] "r" (a2)
    : "x16", "x0", "x1", "x2"
  );
  return ret;
}

// --------------------------------------
// Syscalls
// --------------------------------------
#define SYS_exit        1
#define SYS_write       4

NO_RETURN void exit(i32 ec) {
  syscall1(SYS_exit, ec);
  __builtin_unreachable();
}

// --------------------------------------
// Print
// --------------------------------------
#define STDIN           0
#define STDOUT          1
#define STDERR          2

// meh
i64 cstr_len(const char *cstr) {
  i64 ret = 0;
  if (cstr) {
    while (*cstr++) {
      ++ret;
    }
  }
  return ret;
}

void print_buf(i32 fd, const char *buf, i32 size) {
  syscall3(SYS_write, fd, (i64)buf, size);
}

void print_cstr(i32 fd, const char *cstr) {
  if (cstr) {
    i64 size = cstr_len(cstr);
    syscall3(SYS_write, fd, (i64)cstr, size);
  } else {
    syscall3(SYS_write, fd, (i64)"(null)", 6);
  }
}

void print_u64x(i32 fd, u64 v) {
  u8 buf[18];
  buf[0] = '0';
  buf[1] = 'x';

  for (i32 i = 0; i < 16; ++i) {
    u8 r = (v >> ((15 - i) << 2)) & 0xf;
    buf[i + 2] = r > 9 ? r - 10 + 'a' : r + '0';
  }

  syscall3(SYS_write, fd, (i64)buf, sizeof(buf) / sizeof(buf[0]));
}

void print_i64(i32 fd, i64 v) {
  u8 buf[21]; // sign (1 char) + 2^64(20 chars)

  i32 is_neg = 0;
  if (v < 0) {
    is_neg = 1;
    v = -v;
  }

  u8 *buf_end = buf + sizeof(buf) / sizeof(buf[0]);
  u8 *buf_cur = buf_end;
  do {
    i64 r = v % 10;
    *--buf_cur = r + '0';
    v = v / 10;
  } while (v);

  if (is_neg) {
    *--buf_cur = '-';
  }
  syscall3(SYS_write, fd, (i64)buf_cur, buf_end - buf_cur);
}

void print_f32(i32 fd, f32 v) {
  const char *buf = 0;
  u8 buf_size;

  union f32u32 {
    f32 f;
    u32 u;
  };

  union f32u32 fu = {.f = v};

  // IEEE-754: 1 bit sign, 8 bits exponent, 23 bits mantissa
  u32 exp = (fu.u & 0x7F800000) >> 23;
  u32 man = (fu.u & 0x007FFFFF);

  if (fu.u == 0x0) {
    buf = "0.0";
    buf_size = 3;
  } else if (fu.u == 0x80000000) {
    buf = "-0.0";
    buf_size = 4;
  } else if (fu.u == 0x7F800000) {
    buf = "INF";
    buf_size = 3;
  } else if (fu.u == 0xFF800000) {
    buf = "-INF";
    buf_size = 4;
  } else if (exp == 0xFF && man != 0) {
    buf = "NAN";
    buf_size = 3;
  }

  if (buf) {
    syscall3(SYS_write, fd, (i64)buf, buf_size);
  } else {
    // TODO: well, that's wrong for floats that don't fit into i64:)
    i64 integer         = (i64)v;

    // reset sign
    union f32u32 fracfu = {.f = fu.f - integer};
    fracfu.u            &= 0x7FFFFFFF;

    i64 fraci           = fracfu.f * 1000.0f;

    print_i64(fd, integer);
    syscall3(SYS_write, fd, (i64)".", 1);
    print_i64(fd, fraci);
  }
}

// --------------------------------------
// Expect/Assert
// --------------------------------------
FORCE_INLINE static void debugbreak(void) {
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__clang__)
    __builtin_debugtrap();
#else
    // gcc doesn't have __builtin_debugtrap equivalent
    // Beware:
    // __builtin_trap generates SIGILL and code after it will be optmized away.
    __builtin_trap();
#endif
}

#define STR1(s) # s
#define STR(s) STR1(s)

// EXPECT() behaves like Debug + Release assert
#define EXPECT(condition, msg) expect_msg(!!(condition), \
    __FILE__ ":" STR(__LINE__) ": Fatal:   (" STR(condition) ") == 0\n"\
    msg)

FORCE_INLINE static void expect_msg(i32 condition, const char *msg) {
  if (!condition) {
    print_cstr(STDERR, msg);
    debugbreak();
  }
}

#define WARN_IF(condition, msg) warn_if_msg(!!(condition), \
    __FILE__ ":" STR(__LINE__) ": Warning: (" STR(condition) ") == 0\n"\
    msg)

FORCE_INLINE static void warn_if_msg(i32 condition, const char *msg) {
  if (condition) {
    print_cstr(STDERR, msg);
  }
}

// --------------------------------------
// Helpers
// --------------------------------------
// TODO: this doesn't seem to be agood random
struct xorshift64_state {
  u64 a;
};

FORCE_INLINE static u64 xorshift64(struct xorshift64_state *state) {
  uint64_t x = state->a;
  x ^= x << 7;
  x ^= x >> 9;
  state->a = x;
  return x;
}

FORCE_INLINE static f32 clampf32(f32 v, f32 lo, f32 hi) {
  return v > hi ? hi : v < lo ? lo : v;
}

FORCE_INLINE static f32 lerpf32(f32 k, f32 x, f32 y) {
  return (1.0f - k) * x + y * k;
}

// TODO: loses precisions when x and y range is big
FORCE_INLINE static f32 fmodf32(f32 x, f32 y) {
    f32 res;
    __asm__ volatile (
      "fdiv   s2, %s1, %s2\n"   // s2 = x / y
      "frintm s2, s2\n"         // s2 = floor(s2)
      "fmul   s2, s2, %s2\n"    // s2 = y * s2
      "fsub   %s0, %s1, s2\n"   // res = s1 - s2
      : "=w"  (res)
      : "w"   (x), "w" (y)
      : "s2"
    );
    return res;
}

FORCE_INLINE static f32 fracf32(f32 x) {
    f32 res;
    __asm__ volatile (
        "frintm s1, %s1\n"      // s1 = floor(x)
        "fsub   %s0, %s1, s1\n" // res = x - s1
        : "=w"(res)
        : "w"(x)
        : "s1"
    );
    return res;
}

// Sine in turns (1 turn == 2pi)
f32 sinf32(f32 turns) {
  // Calculate sine in range [-pi, pi]
  // https://mooooo.ooo/chebyshev-sine-approximation/
  f32 x = turns * 2.0f; // half turns
  x = fmodf32(x + 1.0f, 2.0f) - 1.0f; // to [-1; 1] half turns

  f32 x2 = x * x;
  f32 p11 =              0.000385937753182769f; // x^11
  f32 p9  = p11 * x2 +  -0.006860187425683514f; // x^9
  f32 p7  = p9  * x2 +   0.0751872634325299f;   // x^7
  f32 p5  = p7  * x2 +  -0.5240361513980939f;   // x^5
  f32 p3  = p5  * x2 +   2.0261194642649887f;   // x^3
  f32 p1  = p3  * x2 +  -3.1415926444234477f;   // x
  return (x - 1.0f) * (x + 1.0f) * p1 * x;
}

FORCE_INLINE static i32 is_bit_set(i32 flags, i32 bit) {
  return (flags & bit) == bit;
}

FORCE_INLINE static u64 read_cpu_timer_freq(void) {
  u64 val;
  __asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (val));
  return val;
}

FORCE_INLINE static u64 read_cpu_timer(void) {
  u64 val;
  // use isb to avoid speculative read of cntvct_el0
  __asm__ volatile ("isb;\n\tmrs %0, cntvct_el0" : "=r" (val) :: "memory");
  return val;
}

// --------------------------------------
// nostdlib stubs
// --------------------------------------
// Stack protector stubs could be generated by the compiler
u64 __stack_chk_guard = 0xDEADBEEF;

void __stack_chk_fail(void) {
    print_cstr(STDERR, "Stack smashed!\n");
    debugbreak();
}

// --------------------------------------
// Window
// --------------------------------------
// Window with OpenGL context
struct window {
  CGDirectDisplayID did;
  CGSConnectionID   cid;
  CGWindowID        wid;
  CGLContextObj     glctx;
  f32               rect[4]; // x, y, w, h
};

// Init transparent window and OpenGL context
struct window init_window(int is_full_screen) {
  CGDirectDisplayID did;
  CGWindowID        wid;
  CGSConnectionID   cid;
  CGError           cg_err;
  CGLError          cgl_err;
  CGRect            view_rect;
  CGRect            win_rect;

  cid = CGSMainConnectionID();
  EXPECT(cid, "CGSMainConnectionID() failed\n");

  did = CGMainDisplayID();
  EXPECT(did, "CGMainDisplayID() failed\n");

  win_rect  = CGDisplayBounds(did);
  view_rect = (CGRect){.size = win_rect.size};

  if (is_full_screen) {
    cg_err = CGDisplayCapture(did);
    EXPECT(!cg_err, "Failed to capture display\n");

    wid = CGShieldingWindowID(did);
    EXPECT(wid, "Failed to get shielding window\n");
    win_rect = CGRectMake(800.0, 100.0, 800.0, 600.0);
  } else {
    CGSRegionRef      win_region;
    CGSRegionRef      view_region;

    cg_err = CGSNewRegionWithRect(&win_rect, &win_region);
    EXPECT(!cg_err, "CGSNewRegionWithRect() failed\n");

    cg_err = CGSNewRegionWithRect(&win_rect, &view_region);
    EXPECT(!cg_err, "CGSNewRegionWithRect() failed\n");

    cg_err = CGSNewWindow(cid, kCGBackingStoreBuffered, 0.0, 0.0, win_region, &wid);
    EXPECT(!cg_err, "CGSNewWindow() failed\n");

    // clear windows surfeace
    CGContextRef cgctx = CGWindowContextCreate(cid, wid, 0);
    CGContextClearRect(cgctx, view_rect);
    CGContextRelease(cgctx);

    cg_err = CGSSetWindowLevel(cid, wid, kCGMaximumWindowLevel);
    EXPECT(!cg_err, "CGSSetWindowLevel() failed\n");

    cg_err = CGSSetWindowOpacity(cid, wid, 0);
    EXPECT(!cg_err, "CGSSetWindowOpacity() failed\n");

    i32 tags[] = {
      0x0200, // pass through mouse clicks
      0,      // 0 terminator
    };
    cg_err = CGSSetWindowTags(cid, wid, tags, sizeof(tags[0]) * 8);
    EXPECT(!cg_err, "CGSSetWindowTags() failed\n");

    // make window appear
    cg_err = CGSOrderWindow(cid, wid, kCGSOrderIn, 0);
    EXPECT(!cg_err, "CGSOrderWindow() failed\n");
  }

  // Create OpenGL context
  CGLPixelFormatAttribute attributes[] = {
    kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,
    kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
    kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
    kCGLPFAStencilSize, (CGLPixelFormatAttribute)8,
    kCGLPFAAccelerated,
    kCGLPFADoubleBuffer,
    (CGLPixelFormatAttribute)0
  };

  CGLPixelFormatObj pixelFormat;
  GLint numPixelFormats;
  CGLChoosePixelFormat(attributes, &pixelFormat, &numPixelFormats);
  EXPECT(pixelFormat, "Failed to create OpenGL pixel format\n");

  CGLContextObj glctx;
  CGLCreateContext(pixelFormat, 0, &glctx);
  CGLDestroyPixelFormat(pixelFormat);
  EXPECT(glctx, "CGLCreateContext() failed\n");

  GLint vsync_enabled = 1;
  CGLSetParameter(glctx, kCGLCPSwapInterval, &vsync_enabled);

  GLint surface_opacity = 0;
  CGLSetParameter(glctx, kCGLCPSurfaceOpacity, &surface_opacity);

  CGSSurfaceID sid;
  cg_err = CGSAddSurface(cid, wid, &sid);
  EXPECT(!cg_err, "CGSAddSurface() failed\n");

  cg_err = CGSSetSurfaceBounds(cid, wid, sid, view_rect);
  EXPECT(!cg_err, "CGSSetSurfaceBounds() failed\n");

  cg_err = CGSOrderSurface(cid, wid, sid, 1, 0);
  EXPECT(!cg_err, "CGSOrderSurface() failed\n");

  cgl_err = CGLSetSurface(glctx, cid, wid, sid);
  EXPECT(!cgl_err, "CGLSetSurface() failed\n");

  GLint is_drawable = 0;
  cgl_err = CGLGetParameter(glctx, kCGLCPHasDrawable, &is_drawable);
  EXPECT(!cgl_err, "CGLGetParameter() failed\n");

  cgl_err = CGLSetCurrentContext(glctx);
  EXPECT(!cgl_err, "CGLSetCurrentContext() failed\n");

  return (struct window){
    .did    = did,
    .cid    = cid,
    .wid    = wid,
    .glctx  = glctx,
    .rect   = {
      win_rect.origin.x,
      win_rect.origin.y,
      win_rect.size.width,
      win_rect.size.height
    },
  };
}

void shutdown_window(struct window *w) {
  if (w->glctx) {
    CGLDestroyContext(w->glctx);
  }
  if (w->cid && w->wid) {
    CGSReleaseWindow(w->cid, w->wid);
  }
  if (w->did) {
    CGDisplayRelease(w->did);
  }

  w->did = 0;
  w->cid = 0;
  w->wid = 0;
  w->glctx = 0;
}

// --------------------------------------
// OpenGL helpers
// --------------------------------------
#define CHECK_GL_ERROR()                                                       \
do {                                                                           \
  GLenum gl_err = glGetError();                                                \
  WARN_IF(gl_err, "");                                                         \
  if (gl_err) {                                                                \
    print_cstr(STDERR, "glGetError == ");                                      \
    print_u64x(STDERR, gl_err);                                                \
    print_cstr(STDERR, "\n");                                                  \
    exit(1);                                                                   \
  }                                                                            \
} while (0)


// Create glCreateProgram and log compilation and linker errors
GLuint create_gl_shader_program(const char *vert_glsl, const char *frag_glsl) {
  GLuint prog = glCreateProgram();
  GLint is_ok;
  GLchar info[1024];

  GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vert_shader, 1, &vert_glsl, 0);
  glCompileShader(vert_shader);

  glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &is_ok);
  if (!is_ok) {
    glGetShaderInfoLog(vert_shader, sizeof(info), 0, info);
    print_cstr(STDOUT, "Vertex shader compile error:\n");
    print_cstr(STDOUT, info);
  }
  EXPECT(is_ok, "failed to compile vertex shader\n");

  GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(frag_shader, 1, &frag_glsl, 0);
  glCompileShader(frag_shader);

  glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &is_ok);
  if (!is_ok) {
    glGetShaderInfoLog(frag_shader, sizeof(info), 0, info);
    print_cstr(STDOUT, "Fragment shader compile error:\n");
    print_cstr(STDOUT, info);
  }
  EXPECT(is_ok, "failed to compile fragment shader\n");

  prog = glCreateProgram();
  glAttachShader(prog, vert_shader);
  glAttachShader(prog, frag_shader);
  glLinkProgram(prog);

  glGetProgramiv(prog, GL_LINK_STATUS, &is_ok);
  if (!is_ok) {
    glGetProgramInfoLog(prog, sizeof(info), 0, info);
    print_cstr(STDOUT, "Program link error:\n");
    print_cstr(STDOUT, info);
  }
  EXPECT(is_ok, "Fata: failed to link shader program\n");

  glDeleteShader(vert_shader);
  glDeleteShader(frag_shader);

  return prog;
}
