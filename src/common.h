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

#define SWAP(a, b)                                                             \
  do {                                                                         \
    __typeof__(a) tmp = (a);                                                   \
    (a) = (b);                                                                 \
    (b) = tmp;                                                                 \
  } while (0)

#define MAX(a, b) ({                                                           \
    __typeof__(a) a_ = (a);                                                    \
    __typeof__(b) b_ = (b);                                                    \
    a_ >= b_ ? a_ : b_;                                                        \
})

#define MIN(a, b) ({                                                           \
    __typeof__(a) a_ = (a);                                                    \
    __typeof__(b) b_ = (b);                                                    \
    a_ <= b_ ? a_ : b_;                                                        \
})

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
typedef unsigned long long CGSSpaceID;
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

extern CGSSpaceID CGSGetActiveSpace(CGSConnectionID connection);

extern void CGSAddWindowsToSpaces(CGSConnectionID cid, CFArrayRef windows,
    CFArrayRef spaces);

// --------------------------------------
// syscall
// --------------------------------------
static i64 syscall1(i64 sys_num, i64 a0) {
  i64 ret;
  __asm__ volatile (
    "mov x16,     %[sys_num]\n"   // syscall number
    "mov x0,      %[a0]\n"        // a0
    "svc          0x80\n"
    "mov %[ret],  x0\n"
    : [ret] "=r" (ret)
    : [sys_num] "r" (sys_num), [a0] "r" (a0)
    : "x16", "x0"
  );
  return ret;
}

static i64 syscall2(i64 sys_num, i64 a0, i64 a1) {
  i64 ret;
  __asm__ volatile (
    "mov x16,     %[sys_num]\n"
    "mov x0,      %[a0]\n"
    "mov x1,      %[a1]\n"
    "svc          0x80\n"
    "mov %[ret],  x0\n"
    : [ret] "=r" (ret)
    : [sys_num] "r" (sys_num), [a0] "r" (a0), [a1] "r" (a1)
    : "x16", "x0", "x1"
  );
  return ret;
}

static i64 syscall3(i64 sys_num, i64 a0, i64 a1, i64 a2) {
  i64 ret;
  __asm__ volatile (
    "mov x16,     %[sys_num]\n"
    "mov x0,      %[a0]\n"
    "mov x1,      %[a1]\n"
    "mov x2,      %[a2]\n"
    "svc          0x80\n"
    "mov %[ret],  x0\n"
    : [ret] "=r" (ret)
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

// TODO EINTR
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

static i64 cstr_len(const char *cstr) {
  i64 ret = 0;
  if (cstr) {
    while (*cstr++) {
      ++ret;
    }
  }
  return ret;
}

static void print_buf(i32 fd, const char *buf, i32 size) {
  syscall3(SYS_write, fd, (i64)buf, size);
}

// Print \0 terminated string
static void print_cstr(i32 fd, const char *cstr) {
  if (cstr) {
    i64 size = cstr_len(cstr);
    syscall3(SYS_write, fd, (i64)cstr, size);
  } else {
    syscall3(SYS_write, fd, (i64)"(null)", 6);
  }
}

// Print hex representation of u64
static void print_u64x(i32 fd, u64 v) {
  u8 buf[18];
  buf[0] = '0';
  buf[1] = 'x';

  for (i32 i = 0; i < 16; ++i) {
    u8 r = (v >> ((15 - i) << 2)) & 0xf;
    buf[i + 2] = r > 9 ? r - 10 + 'a' : r + '0';
  }

  syscall3(SYS_write, fd, (i64)buf, sizeof(buf) / sizeof(buf[0]));
}

// Helper
static void print_zero_neg_u64_(i32 fd, u64 v, i32 zero_precision, b32 is_neg) {
  u8 buf[21]; // sign (1 char) + 2^64(20 chars)

  u8 *buf_end = buf + sizeof(buf) / sizeof(buf[0]);
  u8 *buf_cur = buf_end;
  do {
    i64 r = v % 10;
    *--buf_cur = r + '0';
    v = v / 10;
  } while (v);

  i32 buf_size = buf_end - buf_cur;
  for (int i = buf_size, size = MIN(20, zero_precision); i < size; ++i) {
    *--buf_cur = '0';
  }

  if (is_neg) {
    *--buf_cur = '-';
  }
  syscall3(SYS_write, fd, (i64)buf_cur, buf_end - buf_cur);
}

static void print_zero_i64(i32 fd, i64 v, i32 zero_precision) {
  b32 is_neg = 0;
  if (v < 0) {
    is_neg = 1;
    v = -v;
  }
  print_zero_neg_u64_(fd, v, zero_precision, is_neg);
}

static void print_zero_u64(i32 fd, u64 v, i32 zero_precision) {
  print_zero_neg_u64_(fd, v, zero_precision, 0);
}

static void print_i64(i32 fd, i64 v) {
  print_zero_i64(fd, v, 0);
}

static void print_u64(i32 fd, u64 v) {
  print_zero_u64(fd, v, 0);
}

static void print_f32(i32 fd, f32 v) {
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
    // reset sign
    fu.u                &= 0x7FFFFFFF;

    // TODO: well, that's wrong for floats that don't fit into u64 :)
    u64 integer         = (u64)fu.f;

    f32 frac            = fu.f - integer;
    i64 fraci           = frac * 1000.0f;     // 3 digits precision

    if (v < 0) {
      syscall3(SYS_write, fd, (i64)"-", 1);
    }
    print_u64(fd, integer);
    syscall3(SYS_write, fd, (i64)".", 1);
    print_zero_i64(fd, fraci, 3);             // 3 digits precision
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
    msg "\n")

FORCE_INLINE static void expect_msg(i32 condition, const char *msg) {
  if (!condition) {
    print_cstr(STDERR, msg);
    debugbreak();
  }
}

#define WARN_IF(condition, msg) warn_if_msg(!!(condition), \
    __FILE__ ":" STR(__LINE__) ": Warning: (" STR(condition) ") == 0\n"\
    msg "\n")

FORCE_INLINE static void warn_if_msg(i32 condition, const char *msg) {
  if (condition) {
    print_cstr(STDERR, msg);
  }
}

// --------------------------------------
// Helpers
// --------------------------------------
// TODO: this doesn't seem to be a good random
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

FORCE_INLINE static f32 sqrtf32(f32 x) {
  f32 res;
  __asm__ volatile (
    "fsqrt %s[res], %s[x]"
    : [res] "=w" (res)
    : [x] "w" (x)
  );
  return res;
}

FORCE_INLINE static f32 len32x2(const f32 a[2]) {
  return sqrtf32(a[0] * a[0] + a[1] * a[1]);
}

FORCE_INLINE static void normf32x2(const f32 a[2], f32 out[2]) {
  f32 l = len32x2(a);
  out[0] /= l;
  out[1] /= l;
}

FORCE_INLINE static void dotf32x2(const f32 a[2], const f32 b[2], f32 out[2]) {
  out[0] = a[0] * b[0];
  out[1] = a[1] * b[1];
}

// TODO: loses precisions when x and y range is big
FORCE_INLINE static f32 fmodf32(f32 x, f32 y) {
  f32 res;
  __asm__ volatile (
    "fdiv   s2, %s[x], %s[y]\n"     // s2 = x / y
    "frintm s2, s2\n"               // s2 = floor(s2)
    "fmul   s2, s2, %s[y]\n"        // s2 = y * s2
    "fsub   %s[ret], %s[x], s2\n"   // res = s1 - s2
    : [ret] "=w"  (res)
    : [x] "w" (x), [y] "w" (y)
    : "s2"
  );
  return res;
}

FORCE_INLINE static f32 fracf32(f32 x) {
  f32 res;
  __asm__ volatile (
    "frintm s1, %s[x]\n"            // s1 = floor(x)
    "fsub   %s[ret], %s[x], s1\n"   // res = x - s1
    : [ret] "=w"(res)
    : [x] "w" (x)
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

FORCE_INLINE static b32 is_bit_set(i32 flags, i32 bit) {
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
  __asm__ volatile ("isb;\n\tmrs %0, cntvct_el0" : "=r" (val));
  return val;
}

// Print average FPS and Delta time
FORCE_INLINE static void print_avg_dt_fps(f32 avg_dt) {
  print_cstr(STDOUT, "Average fps: ");
  print_i64(STDOUT, (u64)(1.0f / avg_dt));
  print_cstr(STDOUT, ", dt: ");
  print_i64(STDOUT, (u64)(avg_dt * 1e3));
  print_cstr(STDOUT, "ms (");
  print_i64(STDOUT, (u64)(avg_dt * 1e6));
  print_cstr(STDOUT, "us)\n");
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
static void window_init(struct window *w, b32 is_full_screen) {
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
  } else {
    CGSRegionRef      win_region;
    CGSRegionRef      view_region;

    cg_err = CGSNewRegionWithRect(&win_rect, &win_region);
    EXPECT(!cg_err, "CGSNewRegionWithRect() failed\n");

    cg_err = CGSNewRegionWithRect(&win_rect, &view_region);
    EXPECT(!cg_err, "CGSNewRegionWithRect() failed\n");

    cg_err = CGSNewWindow(cid, kCGBackingStoreBuffered, 0.0, 0.0, win_region,
        &wid);
    EXPECT(!cg_err, "CGSNewWindow() failed\n");

    // clear windows surface
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

#if 0
  // Add window to the active Workspace.
  // This uses CFArray and CFNumber and requires to link with
  //   -framework CoreFoundation
  CGSSpaceID spid = CGSGetActiveSpace(cid);
  const void *spid_num  = (void *)CFNumberCreate(0, kCFNumberIntType, &spid);
  const void *wid_num   = (void *)CFNumberCreate(0, kCFNumberIntType, &wid);
  CFArrayRef spids  = CFArrayCreate(0, &spid_num, 1, &kCFTypeArrayCallBacks);
  CFArrayRef wids   = CFArrayCreate(0, &wid_num, 1, &kCFTypeArrayCallBacks);
  CGSAddWindowsToSpaces(CGSMainConnectionID(), wids, spids);
  CFRelease(spid_num);
  CFRelease(wid_num);
  CFRelease(spids);
  CFRelease(wids);
#endif

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

  *w = (struct window){
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

static void window_shutdown(struct window *w) {
  if (w->glctx) {
    CGLDestroyContext(w->glctx);
  }
  if (w->cid && w->wid) {
    CGSReleaseWindow(w->cid, w->wid);
  }
  if (w->did) {
    CGDisplayRelease(w->did);
  }

  *w = (struct window){0};
}


// Swap and present.
// Returns 0 on success.
static i32 window_flush(struct window *w) {
  CGLError cgl_err = CGLFlushDrawable(w->glctx);
  WARN_IF(cgl_err, "CGLFlushDrawable() failed\n");
  return cgl_err;
}

// --------------------------------------
// Event Loop
// --------------------------------------

// Key Codes. Direct mapping to kVK_* virutal keycodes.
// All virtual keycode constants are defined in HIToolbox/Events.h
// #include <Carbon/Carbon.h>
// and go to definition of kVK_Escape
enum KC : u8 {
  KC_RET    = 0x24,
  KC_TAB    = 0x30,
  KC_SPACE  = 0x31,
  KC_DEL    = 0x33,
  KC_ESC    = 0x35,

  KC_SENTINEL, // keep it the biggest value in the enum
};

enum {KC_SIZE=256};
static_assert(KC_SENTINEL < KC_SIZE, "s_keycodes can't contain enum KC");

struct keycodes {
  enum KC e[KC_SIZE]; // TODO: pack it
};

struct event_loop {
  struct keycodes     keycodes;
  CFMachPortRef       event_tap;
  CFRunLoopSourceRef  source;
  CFRunLoopRef        runloop;
};

static CGEventRef event_handler(CGEventTapProxy proxy, CGEventType type,
    CGEventRef event, void *userdata)
{
  (void)proxy;

  struct event_loop *loop = userdata;
  CGKeyCode keycode;

  switch (type)
  {
    case kCGEventTapDisabledByTimeout:
    case kCGEventTapDisabledByUserInput:
      EXPECT(0, "Event tap was cancelled");
      break;

    case kCGEventKeyDown:
    case kCGEventKeyUp:
      keycode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
      WARN_IF(keycode >= KC_SIZE, "keycode value >= 256");
      if (keycode < KC_SIZE) {
        loop->keycodes.e[keycode] = type == kCGEventKeyDown;
      }
      break;

    default:
      WARN_IF(1, "Unhandled event type");
      break;
  }
  return event;
}

// Must be paired with event_loop_shutdown()
static void event_loop_init(struct event_loop *loop) {
  CFMachPortRef       event_tap;
  CGEventMask         event_mask;
  CFRunLoopSourceRef  source;
  CFRunLoopRef        runloop;

  event_mask  = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp);
  event_tap   = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
      kCGEventTapOptionListenOnly, event_mask, event_handler, loop);
  EXPECT(event_tap, "CGEventTapCreate() failed");

  source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap, 0);
  EXPECT(source, "CGEventTapCreate() failed");

  runloop = CFRunLoopGetCurrent();
  CFRetain(runloop);

  CFRunLoopAddSource(runloop, source, kCFRunLoopDefaultMode);

  CGEventTapEnable(event_tap, 1);

  *loop = (struct event_loop){
    .keycodes   = {0},
    .event_tap  = event_tap,
    .source     = source,
    .runloop    = runloop,
  };
}

// Runs Run Loop one time, updates input events (loop->keycodes)
// Returns true if input was read.
static b32 event_loop_step(struct event_loop *loop) {
#if 0
  // Signal and wakeup are not needed since we only have one RunLoop atm.
  CFRunLoopSourceSignal(loop->source);
  CFRunLoopWakeUp(loop->runloop);
#else
  (void)loop;
#endif
  CFRunLoopRunResult res = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, 1);
  return res == kCFRunLoopRunHandledSource;
}

static void event_loop_shutdown(struct event_loop *loop) {
  CGEventTapEnable(loop->event_tap, 0);
  CFMachPortInvalidate(loop->event_tap);
  CFRunLoopRemoveSource(loop->runloop, loop->source, kCFRunLoopDefaultMode);
  CFRelease(loop->source);
  CFRelease(loop->event_tap);
  CFRelease(loop->runloop);

  *loop = (struct event_loop){0};
}

// --------------------------------------
// OpenGL helpers
// --------------------------------------
#define CHECK_GL_ERROR()                                                       \
  do {                                                                         \
    GLenum gl_err = glGetError();                                              \
    WARN_IF(gl_err, "");                                                       \
    if (gl_err) {                                                              \
      print_cstr(STDERR, "glGetError == ");                                    \
      print_u64x(STDERR, gl_err);                                              \
      print_cstr(STDERR, "\n");                                                \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

// Create glCreateProgram and log compilation and linker errors
static GLuint create_gl_shader_program(const char *vert_glsl,
    const char *frag_glsl) {
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
