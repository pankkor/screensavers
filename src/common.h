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

#define I32_MAX             (2147483647)
#define I32_MIN             (-(2147483647)-1)
#define U32_MAX             (-1u)
#define I64_MAX             (9223372036854775807ll)
#define I64_MIN             (-(9223372036854775807ll)-1)
#define U64_MAX             (-1ull)

#define INLINE              inline __attribute__((always_inline))
#define NORETURN            __attribute__((noreturn))
#define ALIGNED(x)          __attribute__((aligned(x)))
#define ARRAY_COUNT(x)      (i64)(sizeof(x) / sizeof(x[0]))
#define static_assert       _Static_assert

#define STR1(s)             # s
#define STR(s)              STR1(s)

#define SWAP(a, b)                                                             \
  do {                                                                         \
    __typeof__(a) tmp = (a);                                                   \
    (a) = (b);                                                                 \
    (b) = tmp;                                                                 \
  } while (0)

#define MAX(a, b) ({                                                           \
  __typeof__(a) a_ = (a);                                                      \
  __typeof__(b) b_ = (b);                                                      \
  a_ >= b_ ? a_ : b_;                                                          \
})

#define MIN(a, b) ({                                                           \
  __typeof__(a) a_ = (a);                                                      \
  __typeof__(b) b_ = (b);                                                      \
  a_ <= b_ ? a_ : b_;                                                          \
})


INLINE static void debugbreak(void) {
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

// Include std headers after all common macroses defined
#if defined(__ARM_NEON)
#include <arm_neon.h>
#else
#error Unsupported architecture
#endif

// --------------------------------------
// syscall
// --------------------------------------
INLINE static i64 syscall1(i64 sys_num, i64 a0) {
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

INLINE static i64 syscall2(i64 sys_num, i64 a0, i64 a1) {
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

INLINE static i64 syscall3(i64 sys_num, i64 a0, i64 a1, i64 a2) {
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

INLINE static i64 syscall6(i64 sys_num, i64 a0, i64 a1, i64 a2, i64 a3, i64 a4,
    i64 a5) {
  i64 ret;
  __asm__ volatile (
    "mov x16,     %[sys_num]\n"
    "mov x0,      %[a0]\n"
    "mov x1,      %[a1]\n"
    "mov x2,      %[a2]\n"
  "mov x3,      %[a3]\n"
    "mov x4,      %[a4]\n"
    "mov x5,      %[a5]\n"
    "svc          0x80\n"
    "mov %[ret],  x0\n"
    : [ret] "=r" (ret)
    : [sys_num] "r" (sys_num), [a0] "r" (a0), [a1] "r" (a1), [a2] "r" (a2),
      [a3] "r" (a3), [a4] "r" (a4), [a5] "r" (a5)
    : "x16", "x0", "x1", "x2", "x3", "x4", "x5"
  );
  return ret;
}

// --------------------------------------
// Syscalls
// --------------------------------------
#define SYS_EXIT        1
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_MUNMAP      73
#define SYS_MMAP        197
#define SYS_FTRUNCATE   201

// sys_create
#define O_CREAT         0x00000200      /* create if nonexistant */
#define O_RDONLY        0x0000          /* open for reading only */
#define O_RDWR          0x0002          /* open for reading and writing */

// TODO EINTR
INLINE static NORETURN void exit(i32 ec) {
  syscall1(SYS_EXIT, ec);
  __builtin_unreachable();
}

INLINE static i64 sys_write(i32 fd, const void *buf, u64 size) {
  return syscall3(SYS_WRITE, fd, (i64)buf, size);
}

INLINE static i32 sys_open(const char *filepath, i32 flags, u16 mode) {
  return syscall3(SYS_OPEN, (i64)filepath, flags, mode);
}

INLINE static i32 sys_close(i32 fd) {
  return syscall1(SYS_CLOSE, fd);
}

INLINE static i32 sys_munmap(void *addr, u64 len) {
  return syscall2(SYS_MUNMAP, (u64)addr, len);
}

INLINE static void *sys_mmap(void *addr, u64 len, i32 prot, i32 flags, i32 fd,
    u64 offset) {
  return (void *)syscall6(SYS_MMAP, (u64)addr, len, prot, flags, fd, offset);
}

INLINE static i32 sys_ftruncate(i32 fd, u64 length) {
  return syscall2(SYS_FTRUNCATE, fd, length);
}

// --------------------------------------
// Virtual Memory
// --------------------------------------
#if 0
#include <mach/vm_page_size.h>  // extern vm_page_size
#define OS_PAGE_SIZE vm_page_size
#else
enum { OS_PAGE_SIZE = 16384 };
#endif

#define PROT_WRITE      0x02    /* [MC2] pages can be written */
#define PROT_READ       0x01    /* [MC2] pages can be read */

#define MAP_ANON        0x1000  /* allocated from memory, swap space */

#define MAP_SHARED      0x0001  /* [MF|SHM] share changes */
#define MAP_PRIVATE     0x0002  /* [MF|SHM] changes are private */

u64 os_bytes_to_pages(u64 bytes) {
  return (bytes + OS_PAGE_SIZE - 1) / OS_PAGE_SIZE;
}

void *os_alloc_pages(u64 page_count) {
  void *m;
  i32 flags = MAP_PRIVATE | MAP_ANON;
  u64 size = page_count * OS_PAGE_SIZE;
  m = sys_mmap(0, size, PROT_READ | PROT_WRITE, flags, -1, 0);
  return m;
}

i64 os_free_pages(void *p, u64 page_count) {
  u64 size = page_count * OS_PAGE_SIZE;
  return sys_munmap(p, size);
}

// --------------------------------------
// Helper
// --------------------------------------
INLINE static b32 is_bit_set(i32 flags, i32 bit) {
  return (flags & bit) == bit;
}

// --------------------------------------
// Atomics
// --------------------------------------
// Relaxed fetch+add i32. Returns old value.
INLINE static i32 fetch_add_i32(i32 *a, i32 inc) {
  i32 old;
  __asm__ volatile(
    "ldadd %w[inc], %w[old], [%[a]]"
      : [old] "=r" (old)
      : [a] "r" (a), [inc] "r" (inc)
  );
  return old;
}

// Relaxed fetch+add i64. Returns old value
INLINE static i32 fetch_add_i64(i64 *a, i64 inc) {
  i64 old;
  __asm__ volatile(
    "ldadd %[inc] %[old] [%[a]]"
      : [old] "=r" (old)
      : [a] "r" (a), [inc] "r" (inc)
  );
  return old;
}

// --------------------------------------
// Time Stamp Counter
// --------------------------------------
INLINE static u64 read_cpu_timer_freq(void) {
  u64 val;
  __asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (val));
  return val;
}

INLINE static u64 read_cpu_timer(void) {
  u64 val;
  // use isb to avoid speculative read of cntvct_el0
  __asm__ volatile ("isb;\n\tmrs %0, cntvct_el0" : "=r" (val));
  return val;
}

// --------------------------------------
// Rand
// --------------------------------------
// TODO: this doesn't seem to be a good random
struct xorshift64_state {
  u64 a;
};

INLINE static u64 xorshift64(struct xorshift64_state *state) {
  u64 x = state->a;
  x ^= x << 7;
  x ^= x >> 9;
  state->a = x;
  return x;
}

// --------------------------------------
// Math
// --------------------------------------
INLINE static u32 absi32(i32 v) {
  u32 t = v >> 31;
  v ^= t;
  v += t & 1;
  return v;
}

#if 1
INLINE static u64 absi64(i64 v) {
  u64 t = v >> 63;
  v ^= t;
  v += t & 1;
  return v;
}
#else
// This is not any faster
INLINE static u64 absi64(i64 x) {
  u64 res;
  __asm__ (
    "abs %d[res], %d[x]"
    : [res] "=r" (res)
    : [x] "w" (x)
  );
  return res;
}
#endif

INLINE static f32 absf(f32 v) {
  union f32u32 {
    f32 f;
    u32 u;
  };
  union f32u32 fu = {.f = v};
  fu.u &= 0x7FFFFFFF;
  return fu.f;
}

INLINE static f32 clampf32(f32 v, f32 lo, f32 hi) {
  return v > hi ? hi : v < lo ? lo : v;
}

INLINE static f32 lerpf32(f32 k, f32 x, f32 y) {
  return (1.0f - k) * x + y * k;
}

INLINE static f32 sqrtf32(f32 x) {
  f32 res;
  __asm__ (
    "fsqrt %s[res], %s[x]"
    : [res] "=w" (res)
    : [x] "w" (x)
  );
  return res;
}

// TODO: loses precisions when x and y range is big
INLINE static f32 fmodf32(f32 x, f32 y) {
  f32 res;
  __asm__ (
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

INLINE static f32 fracf32(f32 x) {
  f32 res;
  __asm__ (
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

  f32 x2  = x * x;
  f32 p11 =              0.000385937753182769f; // x^11
  f32 p9  = p11 * x2 +  -0.006860187425683514f; // x^9
  f32 p7  = p9  * x2 +   0.0751872634325299f;   // x^7
  f32 p5  = p7  * x2 +  -0.5240361513980939f;   // x^5
  f32 p3  = p5  * x2 +   2.0261194642649887f;   // x^3
  f32 p1  = p3  * x2 +  -3.1415926444234477f;   // x
  return (x - 1.0f) * (x + 1.0f) * p1 * x;
}

// Cosine in turns (1 turn == 2pi)
f32 cosf32(f32 turns) {
  return sinf32(0.25 - turns);
}

INLINE static f32 len_v2(const f32 v[2]) {
  return sqrtf32(v[0] * v[0] + v[1] * v[1]);
}

INLINE static f32 len_v3(const f32 v[3]) {
  return sqrtf32(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

INLINE static f32 len_v4(const f32 v[4]) {
  return sqrtf32(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
}

INLINE static void norm_v2(f32 inout[2]) {
  f32 l = len_v2(inout);
  inout[0] /= l;
  inout[1] /= l;
}

INLINE static void norm_v3(f32 inout[3]) {
  f32 l = len_v3(inout);
  inout[0] /= l;
  inout[1] /= l;
  inout[2] /= l;
}

INLINE static void norm_v4(f32 inout[4]) {
  f32 l = len_v4(inout);
  inout[0] /= l;
  inout[1] /= l;
  inout[2] /= l;
  inout[3] /= l;
}

INLINE static f32 dot_v2(const f32 v1[2], const f32 v2[2]) {
  return v1[0] * v2[0] + v1[1] * v2[1];
}

INLINE static f32 dot_v3(const f32 v1[3], const f32 v2[3]) {
  return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

INLINE static void cross_v3(
    f32 out[3], const f32 v1[3], const f32 v2[3]) {
  f32 ret[3];
  ret[0] = v1[1] * v2[2] - v1[2] * v2[1];
  ret[1] = v1[2] * v2[0] - v1[0] * v2[2];
  ret[2] = v1[0] * v2[1] - v1[1] * v2[0];
  out[0] = ret[0]; out[1] = ret[1]; out[2] = ret[2];
}

// k * vector 3
void mul_v3_k(f32 out[3], const f32 v[3], f32 k) {
  f32 ret[3];
  ret[0] = v[0] * k;
  ret[1] = v[1] * k;
  ret[2] = v[2] * k;
  out[0] = ret[0]; out[1] = ret[1]; out[2] = ret[2];
}

// Quaternion (x, y, z, w): in normalized axis, angle in turns (1 turn = 2pi)
void q4_axis_angle(f32 out[4], const f32 axis[3], f32 turns) {
  f32 ret[4];
  f32 t = turns * 0.5f;
  f32 sin_t = sinf32(t);
  ret[0] = axis[0] * sin_t;
  ret[1] = axis[1] * sin_t;
  ret[2] = axis[2] * sin_t;
  ret[3] = cosf32(t);
  out[0] = ret[0]; out[1] = ret[1]; out[2] = ret[2]; out[3] = ret[3];
}

// Unit quaternion conjugate q^-1
void conj_q4(f32 out[4], const f32 q[4]) {
  f32 ret[4];
  ret[0] = -q[0];
  ret[1] = -q[1];
  ret[2] = -q[2];
  ret[3] =  q[3];
  out[0] = ret[0]; out[1] = ret[1]; out[2] = ret[2]; out[3] = ret[3];
}

// Multiply 2 quaternions q1*q2
void mul_q4_q4(f32 out[4], const f32 q1[4], const f32 q2[4]) {
  f32 ret[4];
  f32 dot = dot_v3(q1, q2);
  f32 cross[3];
  cross_v3(cross, q1, q2);

  ret[0] = q1[3] * q2[0] + q2[3] * q1[0] + cross[0];
  ret[1] = q1[3] * q2[1] + q2[3] * q1[1] + cross[1];
  ret[2] = q1[3] * q2[2] + q2[3] * q1[2] + cross[2];
  ret[3] = q1[3] * q2[3] - dot;
  out[0] = ret[0]; out[1] = ret[1]; out[2] = ret[2]; out[3] = ret[3];
}

// Rotate point v3 around quaternion q4:
// v' = q * v * q^-1, where q^-1 is conjugate(q)
void rot_v3_q4(f32 out[3], const f32 v[3], const f32 q[4]) {
  f32 ret[3];
  f32 dotqv = dot_v3(v, q);
  f32 dotqq = dot_v3(q, q);

  // Simplified math:
  // 2 * q.v • v * q.v + (q.w * q.w - q.v • q.v) * v + 2 * q.w * q.v ⨯ v ->
  // k1 * q.v + k2 * v + k3 * q.v x v, where
  f32 k1 = 2.0f * dotqv;
  f32 k2 = q[3] * q[3] - dotqq;
  f32 k3 = 2.0f * q[3];

  f32 cross[3];
  cross_v3(cross, q, v);

  ret[0] = k1 * q[0] + k2 * v[0] + k3 * cross[0];
  ret[1] = k1 * q[1] + k2 * v[1] + k3 * cross[1];
  ret[2] = k1 * q[2] + k2 * v[2] + k3 * cross[2];
  out[0] = ret[0]; out[1] = ret[1]; out[2] = ret[2];
}

// --------------------------------------
// Number to char array
// --------------------------------------
const u8 s_hex[16] = "0123456789ABCDEF";

// Write u64 to buffer[16] as Hex.
// Buffer has to be at least 16 bytes long
INLINE static void u64_to_a16x(u8 out[16], u64 v) {
  out[ 0] = s_hex[(v >> 60) & 0xF];
  out[ 1] = s_hex[(v >> 56) & 0xF];
  out[ 2] = s_hex[(v >> 52) & 0xF];
  out[ 3] = s_hex[(v >> 48) & 0xF];
  out[ 4] = s_hex[(v >> 44) & 0xF];
  out[ 5] = s_hex[(v >> 40) & 0xF];
  out[ 6] = s_hex[(v >> 36) & 0xF];
  out[ 7] = s_hex[(v >> 32) & 0xF];
  out[ 8] = s_hex[(v >> 28) & 0xF];
  out[ 9] = s_hex[(v >> 24) & 0xF];
  out[10] = s_hex[(v >> 20) & 0xF];
  out[11] = s_hex[(v >> 16) & 0xF];
  out[12] = s_hex[(v >> 12) & 0xF];
  out[13] = s_hex[(v >>  8) & 0xF];
  out[14] = s_hex[(v >>  4) & 0xF];
  out[15] = s_hex[(v >>  0) & 0xF];
}

// Write u32 to buffer[8] as Hex.
// Buffer has to be at least 8 bytes long
INLINE static void u32_to_a8x(u8 out[8], u32 v) {
  out[0]  = s_hex[(v >> 28) & 0xF];
  out[1]  = s_hex[(v >> 24) & 0xF];
  out[2]  = s_hex[(v >> 20) & 0xF];
  out[3]  = s_hex[(v >> 16) & 0xF];
  out[4]  = s_hex[(v >> 12) & 0xF];
  out[5]  = s_hex[(v >>  8) & 0xF];
  out[6]  = s_hex[(v >>  4) & 0xF];
  out[7]  = s_hex[(v >>  0) & 0xF];
}

// Print hex representation of a buffer.
// `out` has to be twice as big as `buf`
INLINE static void buf_to_ax(u8 *out, const u8 *buf, i32 size) {
  for (i32 i = 0; i < size; ++i) {
    u8 b = buf[i];
    out[(i << 1) + 0] = s_hex[(b >> 4) & 0xF];
    out[(i << 1) + 1] = s_hex[(b >> 0) & 0xF];
  }
}

// Write vector register uint8x16_t to buffer[32] as Hex.
// Buffer has to be at least 16 bytes long
INLINE static void uint8x16_to_a32x(u8 out[32], uint8x16_t v) {
  u8 buf[16];
  vst1q_u8(buf, v);
  buf_to_ax(out, buf, 16);
}

INLINE static u64 u64_to_a1d_(u8 *out, u64 u) {
  u64 q = u / 10;
  u64 r = u - q * 10;
  out[0] = '0' + r;
  return q;
}

INLINE static u32 u32_to_a1d_(u8 *out, u32 u) {
  u32 q = u / 10;
  u32 r = u - q * 10;
  out[0] = '0' + r;
  return q;
}

// Write u64 to buffer[20]
INLINE static void u64_to_a20(u8 out[20], u64 u) {
  u = u64_to_a1d_(out + 19, u);
  u = u64_to_a1d_(out + 18, u);
  u = u64_to_a1d_(out + 17, u);
  u = u64_to_a1d_(out + 16, u);
  u = u64_to_a1d_(out + 15, u);
  u = u64_to_a1d_(out + 14, u);
  u = u64_to_a1d_(out + 13, u);
  u = u64_to_a1d_(out + 12, u);
  u = u64_to_a1d_(out + 11, u);
  u = u64_to_a1d_(out + 10, u);
  u = u64_to_a1d_(out +  9, u);
  u = u64_to_a1d_(out +  8, u);
  u = u64_to_a1d_(out +  7, u);
  u = u64_to_a1d_(out +  6, u);
  u = u64_to_a1d_(out +  5, u);
  u = u64_to_a1d_(out +  4, u);
  u = u64_to_a1d_(out +  3, u);
  u = u64_to_a1d_(out +  2, u);
  u = u64_to_a1d_(out +  1, u);
  u = u64_to_a1d_(out +  0, u);
}

// Write u32 to buffer[10]
INLINE static void u32_to_a10(u8 out[10], u32 u) {
  u = u32_to_a1d_(out +  9, u);
  u = u32_to_a1d_(out +  8, u);
  u = u32_to_a1d_(out +  7, u);
  u = u32_to_a1d_(out +  6, u);
  u = u32_to_a1d_(out +  5, u);
  u = u32_to_a1d_(out +  4, u);
  u = u32_to_a1d_(out +  3, u);
  u = u32_to_a1d_(out +  2, u);
  u = u32_to_a1d_(out +  1, u);
  u = u32_to_a1d_(out +  0, u);
}

// Write i64 to buffer[20] with sign at buffer[0].
// i=-1234 -> "-0000000000000001234"
INLINE static void i64_to_a20(u8 out[20], i64 i) {
  out[0] = '+' + (('-' - '+') & (i >> 63));
  u64 u = absi64(i);
  u = u64_to_a1d_(out + 19, u);
  u = u64_to_a1d_(out + 18, u);
  u = u64_to_a1d_(out + 17, u);
  u = u64_to_a1d_(out + 16, u);
  u = u64_to_a1d_(out + 15, u);
  u = u64_to_a1d_(out + 14, u);
  u = u64_to_a1d_(out + 13, u);
  u = u64_to_a1d_(out + 12, u);
  u = u64_to_a1d_(out + 11, u);
  u = u64_to_a1d_(out + 10, u);
  u = u64_to_a1d_(out +  9, u);
  u = u64_to_a1d_(out +  8, u);
  u = u64_to_a1d_(out +  7, u);
  u = u64_to_a1d_(out +  6, u);
  u = u64_to_a1d_(out +  5, u);
  u = u64_to_a1d_(out +  4, u);
  u = u64_to_a1d_(out +  3, u);
  u = u64_to_a1d_(out +  2, u);
  u = u64_to_a1d_(out +  1, u);
}

// Write i32 to buffer[11] with sign at buffer[0].
// i=-1234 -> "-0000001234"
INLINE static void i32_to_a11(u8 out[11], i32 i) {
  out[0] = '+' + (('-' - '+') & (i >> 31));
  u32 u = absi32(i);
  u32_to_a10(out + 1, u);
}

// Substitute leading zeroes.
// u=00001234, c='_') -> "____1234"
//                            ^
// Returns pointer to the first not substituted character
INLINE static u8 *fmt_subs_leading_zeroes(u8 *inout, i32 size, u8 c) {
  while(size > 0 && inout[0] == '0') {
    inout[0] = c;
    ++inout; --size;
  }
  return inout;
}

// Shift left trimming all `c` characters
INLINE static u8 *fmt_trim(u8 *inout, i32 size, u8 c) {
  while(size > 0 && inout[0] == '0') {
    inout[0] = c;
    ++inout; --size;
  }
  return inout;
}

// Write i64 to buffer[21] padding right with all '0' substituted with `c`.
// i=-1234, c='_' -> "_______________-1234"
INLINE static void i64_to_a20_fmt_right(u8 out[20], i64 i, u8 c) {
  i64_to_a20(out, i);
  u8 *next = fmt_subs_leading_zeroes(out + 1, 18, c);
  SWAP(next[-1], out[0]);
}

// Write i32 to buffer[11] padding right with all '0' substituted with `c`.
// i=-1234, c='_' -> "_______________-1234"
INLINE static void i32_to_a11_fmt_right(u8 out[11], i32 i, u8 c) {
  i32_to_a11(out, i);
  u8 *next = fmt_subs_leading_zeroes(out + 1, 9, c);
  SWAP(next[-1], out[0]);
}

// Write i64 to buffer[21] padding right with all '0' substituted with `c`.
// u=1234, c='_' -> "_______________1234"
INLINE static void u64_to_a20_fmt_right(u8 out[20], u64 u, u8 c) {
  u64_to_a20(out, u);
  fmt_subs_leading_zeroes(out, 19, c);
}

// Write i32 to buffer[11] padding right with all '0' substituted with `c`.
// u=1234, c='_' -> "_______________1234"
INLINE static void u32_to_a10_fmt_right(u8 out[10], u32 u, u8 c) {
  u32_to_a10(out, u);
  fmt_subs_leading_zeroes(out, 9, c);
}

// --------------------------------------
// C-String operations
// --------------------------------------
static i64 cstr_len(const char *cstr) {
  i64 ret = 0;
  while (*cstr++) {
    ++ret;
  }
  return ret;
}

// Copy c-string up to `n` characters or 0 terminator.
// 0 terminator is not copied.
// Returns number of bytes copied.
static i32 cstr_n_copy(u8 *dst, const char *src, i32 n) {
  i32 ret = 0;
  while (ret < n && *src != 0) {
    *dst++ = *src++;
    ++ret;
  }
  return ret;
}

// --------------------------------------
// Memory operations
// --------------------------------------

// Take `src` buffer up to `n` characters and copy it to `dst` buffer.
// omitting leading charactes `c`.
// c='0' "0000abc0" -> "abc0"
// Returns number of characters copied
INLINE static i32 buf_n_copy_trim_leading(u8 * restrict dst,
    const u8 * restrict src, i32 n, u8 c) {
  i32 ret = 0;
  i32 i = 0;
  for (; i < n && src[i] == c; ++i) { }
  for (; i < n; ++i, ++ret) {
    dst[ret] = src[i];
  }
  return ret;
}


// Fill buffer with bytes `b`
static void buf_fill(u8 *dst, i32 n, u8 b) {
  for (i32 i = 0; i < n; ++i) {
    dst[i] = b;
  }
}

// Unaligned memory comparison
static i32 is_mem_eq(u8 * restrict l, u8 * restrict r, i64 size) {
  while (size >= 32) {
    u8 b0 = *(u64 *)(l +  0) != *(u64 *)(r +  0);
    u8 b1 = *(u64 *)(l +  8) != *(u64 *)(r +  8);
    u8 b2 = *(u64 *)(l + 16) != *(u64 *)(r + 16);
    u8 b3 = *(u64 *)(l + 24) != *(u64 *)(r + 24);
    if (b0 | b1 | b2 | b3) { return 0; }
    size -= 32; l += 32; r += 32;
  }

  if (size >= 24) {
    u8 b0 = *(u64 *)(l +  0) != *(u64 *)(r +  0);
    u8 b1 = *(u64 *)(l +  8) != *(u64 *)(r +  8);
    u8 b2 = *(u64 *)(l + 16) != *(u64 *)(r + 16);
    if (b0 | b1 | b2) { return 0; }
    size -= 24; l += 24; r += 24;
  } else if (size >= 16) {
    u8 b0 = *(u64 *)(l +  0) != *(u64 *)(r +  0);
    u8 b1 = *(u64 *)(l +  8) != *(u64 *)(r +  8);
    if (b0 | b1) { return 0; }
    size -= 16; l += 16; r += 16;
  } else if (size >= 8) {
    u8 b0 = *(u64 *)(l +  0) != *(u64 *)(r +  0);
    if (b0) { return 0; }
    size -= 8; l += 8; r += 8;
  }

  while (size > 0) {
    u8 b0 = *l != *r ;
    if (b0) { return 0; }
    --size; ++l; ++r;
  }

  return 1;
}

// Create a 32 byte mask:
//  - `is_FF00 == 0` mask filled with 0x00 up to `size` and 0xFF up to 32,
//  - `is_FF00 != 0` mask filled with 0xFF up to `size` and 0x00 up to 32,
// Example
// - mask32_neon(4, is_FF00):
// is_FF00 == 0   ->  [0, size) -> 0x00, [size, 31] -> 0xFF
//    0  1  2  3  4  5  6  7  8    31
//   00 00 00 00 FF FF FF FF FF .. FF
//                ^- size = 4
// is_FF00 == 1   ->  [0, size) -> 0xFF, [size, 31] -> 0x00
//    0  1  2  3  4  5  6  7  8    31
//   FF FF FF FF 00 00 00 00 00 .. 00
//                ^- size = 4
INLINE static uint8x16x2_t mask32_neon(i32 size, b32 is_FF00) {
  uint8x16x2_t ret;
  uint8x16_t idx0  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15};
  uint8x16_t idx1  = {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};

  uint8x16_t vsize = vdupq_n_u8((u8)size);
  if (is_FF00) {
    ret.val[0] = vcltq_u8(idx0, vsize);
    ret.val[1] = vcltq_u8(idx1, vsize);
  } else {
    ret.val[0] = vcgeq_u8(idx0, vsize);
    ret.val[1] = vcgeq_u8(idx1, vsize);
  }
  return ret;
}

static i32 is_mem_eq_neon_aligned32(u8 * ALIGNED(32) restrict l,
    u8 * ALIGNED(32) restrict r, i64 size) {
  while (size >= 32) {
    uint8x16x2_t vl = { vld1q_u8(l), vld1q_u8(l + 16) };
    uint8x16x2_t vr = { vld1q_u8(r), vld1q_u8(r + 16) };
    uint8x16_t eq0 = vceqq_u8(vl.val[0], vr.val[0]);  // eq - 0xFF, ne - 0x00
    uint8x16_t eq1 = vceqq_u8(vl.val[1], vr.val[1]);
    uint8_t min0 = vminvq_u8(eq0);
    uint8_t min1 = vminvq_u8(eq1);
    if ((min0 & min1) == 0) { return 0; }
    size -= 32; l += 32; r += 32;
  }

  if (size > 0) {
    // 1..31 bytes left to cmp
    uint8x16x2_t vmask = mask32_neon(size, 0 /*00FF mask*/);
    uint8x16x2_t vl = { vld1q_u8(l), vld1q_u8(l + 16) };
    uint8x16x2_t vr = { vld1q_u8(r), vld1q_u8(r + 16) };
    uint8x16_t eq0  = vceqq_u8(vl.val[0], vr.val[0]); // eq - 0xFF, ne - 0x00
    uint8x16_t eq1  = vceqq_u8(vl.val[1], vr.val[1]);
    uint8x16_t meq0 = vorrq_u8(eq0, vmask.val[0]);    // 0xFF - masked out
    uint8x16_t meq1 = vorrq_u8(eq1, vmask.val[1]);
    uint8_t min0    = vminvq_u8(meq0);
    uint8_t min1    = vminvq_u8(meq1);
    if ((min0 & min1) == 0) { return 0; }
  }
  return 1;
}

// Memory comparison of aligned buffers, 32 bytes at a time.
// Make sure that buffer size is multiple of 32 bytes, all 32 bytes will be
// fetched. Size however can be not multiple of 32.
INLINE static i32 is_mem_eq_aligned32(u8 * ALIGNED(32) restrict l,
    u8 * ALIGNED(32) restrict r, i64 size) {
#if defined(__ARM_NEON)
  return is_mem_eq_neon_aligned32(l, r, size);
#else
  return is_mem_eq(l, r, size);
#endif
}

INLINE static void mem_cp(u8 * dst, const u8 *src, int size) {
  for (i32 i = 0; i < size; ++i) {
    dst[i] = src[i];
  }
}

static void mem_cp_neon_aligned32(u8 * ALIGNED(32) restrict dst,
    u8 * ALIGNED(32) restrict src, i64 size) {
  while (size >= 32) {
    // De-interleaved loads paired with interleaved stores
    uint8x16x2_t vsrc = vld2q_u8(src);
    vst2q_u8(dst, vsrc);
    size -= 32; dst += 32; src += 32;
  }

  if (size > 0) {
    // 1..31 bytes left to cmp
    uint8x16x2_t vmask = mask32_neon(size, 0 /*00FF mask*/);
    uint8x16x2_t vdst   = { vld1q_u8(dst), vld1q_u8(dst + 16) };
    uint8x16x2_t vsrc   = { vld1q_u8(src), vld1q_u8(src + 16) };

    // Blend
    // mask:  0 0 0 0 F F F F
    // out :  S S S S D D D D
    uint8x16x2_t vres   = {
      vbslq_u8(vmask.val[0], vdst.val[0], vsrc.val[0]),
      vbslq_u8(vmask.val[1], vdst.val[1], vsrc.val[1]),
    };

    vst1q_u8(dst +  0, vres.val[0]);
    vst1q_u8(dst + 16, vres.val[1]);
  }
}

// Copy aligned aligned buffers, 32 bytes at a time.
// Make sure that buffer size is multiple of 32 bytes, all 32 bytes will be
// fetched. Size however can be not multiple of 32.
INLINE static void mem_cp_aligned32(u8 * ALIGNED(32) restrict dst,
    u8 * ALIGNED(32) restrict src, i64 size) {
#if defined(__ARM_NEON)
  mem_cp_neon_aligned32(dst, src, size);
#else
  mem_cp(dst, src, size);
#endif
}

// --------------------------------------
// Print
// print_*() functions result in unbuffered WRITE syscalls
// --------------------------------------
#define STDIN           0
#define STDOUT          1
#define STDERR          2

// Print buffer as it is
INLINE static void print_buf(i32 fd, const u8 *buf, i32 size) {
  sys_write(fd, buf, size);
}

// Print buffer as hex
INLINE static void print_bufx(i32 fd, const u8 *buf, i32 size) {
  u8 tmp[2];
  for (i32 i = 0; i < size; ++i) {
    u8 b = buf[i];
    tmp[0] = s_hex[(b >> 4) & 0xF];
    tmp[1] = s_hex[(b >> 0) & 0xF];
    sys_write(fd, tmp, 2);
  }
}

// Print \0 terminated string
INLINE static void print_cstr(i32 fd, const char *cstr) {
  if (cstr) {
    i64 size = cstr_len(cstr);
    sys_write(fd, cstr, size);
  } else {
    sys_write(fd, "(null)", 6);
  }
}

// Print hex representation of u64
static void print_u64x(i32 fd, u64 v) {
  u8 buf[18];
  buf[0] = '0';
  buf[1] = 'x';
  u64_to_a16x(buf + 2, v);
  sys_write(fd, buf, ARRAY_COUNT(buf));
}

// Helper
static void print_zero_neg_u64_(i32 fd, u64 v, i32 zero_precision, b32 is_neg) {
  u8 buf[21]; // sign (1 char) + 2^64(20 chars)

  u8 *buf_end = buf + sizeof(buf) / sizeof(buf[0]);
  u8 *buf_cur = buf_end;
  do {
    i64 q = v / 10;
    i64 r = v - q * 10;
    *--buf_cur = r + '0';
    v = q;
  } while (v);

  i32 buf_size = buf_end - buf_cur;
  for (i32 i = buf_size, size = MIN(20, zero_precision); i < size; ++i) {
    *--buf_cur = '0';
  }

  if (is_neg) {
    *--buf_cur = '-';
  }
  sys_write(fd, buf_cur, buf_end - buf_cur);
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
    buf = "NAN"; // can also print +NAN and -NAN if needed, depending on bit 31
    buf_size = 3;
  }

  if (buf) {
    sys_write(fd, buf, buf_size);
  } else {
    // Reset sign, remembering it
    i32 sign            = (fu.u & 0x80000000);
    f32 abs_f           = absf(v);

    // Use scientific notation for floats outside of [2^-10; 2^32] range
    // We display up to 3 digits of fractional precision so, 2^-10 is enough
    i32 exp2 = exp - 127; // unbias
    i32 e10 = 0;
    b32 is_scientific = exp2 < -10 || exp2 > 32;
    if (is_scientific) {
      // Normalize to range [1.0, 10.0)
      while (abs_f >= 10.0f) {
       abs_f /= 10.0f; e10 += 1;
      }
      while (abs_f < 1.0f) {
        abs_f *= 10.0f; e10 -= 1;
      }
    }

    // At this point f is small enough to fit into u64
    u64 integer         = abs_f;
    f32 frac            = abs_f - integer;

#if 0
    // Round to 3 digits
    i64 fraci           = frac * 1000.0f + 0.5f;
    if (fraci >= 1000) {
      fraci   = 0;
      integer += 1;

      // Integer can overflow to 10 and we need to renormalize to [1.0, 10.0)
      if (is_scientific && integer >= 10.0f) {
       integer /= 10.0f; e10 += 1;
      }
    }
#else
    // Truncate to 3 digits
    i64 fraci           = frac * 1000.0f;
#endif

    if (sign) {
      sys_write(fd, "-", 1);
    }
    print_u64(fd, integer);
    sys_write(fd, ".", 1);
    print_zero_i64(fd, fraci, 3);                 // 3 digits precision

    if (is_scientific) {
      if (e10 >= 0) {
        sys_write(fd, "e+", 2);
      } else {
        sys_write(fd, "e", 1);
      }
      print_zero_i64(fd, e10, 2);
    }
  }
}

static void print_f32s(i32 fd, i32 size, f32 arr[size]) {
  if (size > 0) {
    print_cstr(fd, "{");
    print_f32(fd, arr[0]);
    for (i32 i = 1; i < size; ++i) {
      print_cstr(fd, ", ");
      print_f32(fd, arr[i]);
    }
    print_cstr(fd, "}");
  }
}

INLINE static void print_v3(i32 fd, f32 v[3]) {
  print_f32s(fd, 3, v);
}

INLINE static void print_v4(i32 fd, f32 v[4]) {
  print_f32s(fd, 4, v);
}

// Print hex representation of uint8x16_t
INLINE static void print_uint8x16(i32 fd, uint8x16_t v) {
  u8 buf[32];
  uint8x16_to_a32x(buf, v);
  sys_write(fd, buf, ARRAY_COUNT(buf));
}

// Print average FPS and Delta time
INLINE static void print_avg_dt_fps(f32 avg_dt) {
  print_cstr(STDOUT, "Average fps: ");
  print_i64(STDOUT, (u64)(1.0f / avg_dt));
  print_cstr(STDOUT, ", dt: ");
  print_i64(STDOUT, (u64)(avg_dt * 1e3));
  print_cstr(STDOUT, "ms (");
  print_i64(STDOUT, (u64)(avg_dt * 1e6));
  print_cstr(STDOUT, "us)\n");
}

INLINE static void print_ln(i32 fd) {
  print_buf(fd, (const u8 *)"\n", 1);
}

// --------------------------------------
// Expect/Assert
// --------------------------------------

// EXPECT() behaves like Debug + Release assert
#define EXPECT(condition, msg) expect_msg(!!(condition), \
    __FILE__ ":" STR(__LINE__) ": Fatal:   (" STR(condition) ") == 0\n"\
    msg "\n")

INLINE static void expect_msg(i32 condition, const char *msg) {
  if (!condition) {
    print_cstr(STDERR, msg);
    debugbreak();
  }
}

#define WARN_IF(condition, msg) warn_if_msg(!!(condition), \
    __FILE__ ":" STR(__LINE__) ": Warning: (" STR(condition) ") == 0\n"\
    msg "\n")

INLINE static void warn_if_msg(i32 condition, const char *msg) {
  if (condition) {
    print_cstr(STDERR, msg);
  }
}

// TEST is EXPECT without message
#define TEST_EXPECT(condition) expect_msg(!!(condition), \
    __FILE__ ":" STR(__LINE__) ": Test failed: (" STR(condition) ") == 0\n")

// --------------------------------------
// nostdlib stubs
// --------------------------------------
// Stack protector stubs could be generated by the compiler
u64 __stack_chk_guard = 0xDEADBEEF;

void __stack_chk_fail(void) {
    print_cstr(STDERR, "Stack smashed!\n");
    debugbreak();
}

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

extern CGError CGSSetWindowTags(const CGSConnectionID cid, CGWindowID wid,
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

    b32 is_dock_and_desktop_control_visile = 0;
    CGWindowLevel w_level = is_dock_and_desktop_control_visile
      ? kCGUtilityWindowLevel - 1
      : kCGMaximumWindowLevel;
    cg_err = CGSSetWindowLevel(cid, wid, w_level);
    EXPECT(!cg_err, "CGSSetWindowLevel() failed\n");

    cg_err = CGSSetWindowOpacity(cid, wid, 0);
    EXPECT(!cg_err, "CGSSetWindowOpacity() failed\n");

    i32 w_tags[] = {
      // 0x0200, // pass through mouse clicks
      0,      // 0 terminator
    };
    cg_err = CGSSetWindowTags(cid, wid, w_tags, sizeof(w_tags[0]) * 8);
    EXPECT(!cg_err, "CGSSetWindowTags() failed\n");

    // make window appear
    cg_err = CGSOrderWindow(cid, wid, kCGSOrderIn, 0);
    EXPECT(!cg_err, "CGSOrderWindow() failed\n");
  }

#if 0
  // TODO: test with multiple monitors
  // Add window to the active Workspace.
  // This uses CFArray and CFNumber and requires linking with
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
  KC_RET      = 0x24,
  KC_TAB      = 0x30,
  KC_SPACE    = 0x31,
  KC_DEL      = 0x33,
  KC_ESC      = 0x35,
  KC_COMMAND  = 0x37,
  KC_SHIFT    = 0x38,
  KC_CAPSLOCK = 0x39,
  KC_OPTION   = 0x3A,
  KC_CONTROL  = 0x3B,
  KC_LEFT     = 0x7B,
  KC_RIGHT    = 0x7C,
  KC_DOWN     = 0x7D,
  KC_UP       = 0x7E,

  KC_SENTINEL, // keep it the biggest value in the enum
};

enum { KC_SIZE=256 };
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

u8 s_map_keycodes[] = {
  KC_SHIFT,
  KC_CONTROL,
  KC_OPTION,
  KC_COMMAND,
  KC_SHIFT,
  KC_CAPSLOCK,
};
int s_map_flags[] = {
  kCGEventFlagMaskShift,
  kCGEventFlagMaskControl,
  kCGEventFlagMaskAlternate,
  kCGEventFlagMaskCommand,
  kCGEventFlagMaskShift,
  kCGEventFlagMaskAlphaShift,
};
static_assert(ARRAY_COUNT(s_map_keycodes) == ARRAY_COUNT(s_map_flags),
    "size mismatch");

static CGEventRef event_handler(CGEventTapProxy proxy, CGEventType type,
    CGEventRef event, void *userdata)
{
  (void)proxy;

  struct event_loop *loop = userdata;
  CGKeyCode keycode;
  CGEventFlags flags;

  switch (type)
  {
    case kCGEventTapDisabledByTimeout:
    case kCGEventTapDisabledByUserInput:
      EXPECT(0, "Event tap was cancelled");
      break;

    case kCGEventKeyDown:
    case kCGEventKeyUp:
      // Keycodes
      keycode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
      WARN_IF(keycode >= KC_SIZE, "keycode value >= 256");
      if (keycode < KC_SIZE) {
        loop->keycodes.e[keycode] = type == kCGEventKeyDown;
      }
      // fallthrough
    case kCGEventFlagsChanged:
      // Modifier flags
      flags = CGEventGetFlags(event);
      for (i32 i = 0; i < ARRAY_COUNT(s_map_keycodes); ++i) {
        if (flags & s_map_flags[i]) {
          u8 flag_keycode = s_map_keycodes[i];
          loop->keycodes.e[flag_keycode] = type == kCGEventKeyDown;
        }
      }
      break;

    default:
      WARN_IF(1, "Unhandled event type");
      break;
  }

#if 1
    // We swallow all the touches. As a precaution monitor Cmd+Opt+Esc for
    // emergency exit.
    if (loop->keycodes.e[KC_COMMAND] &&
        loop->keycodes.e[KC_OPTION] &&
        loop->keycodes.e[KC_ESC]) {
      print_cstr(STDERR, "Emergency exit!\n");
      exit(1);
    }
#endif

  // By not returning `event` we swallow it
  return 0;
}

// Must be paired with event_loop_shutdown()
static void event_loop_init(struct event_loop *loop) {
  CFMachPortRef       event_tap;
  CGEventMask         event_mask;
  CFRunLoopSourceRef  source;
  CFRunLoopRef        runloop;

  event_mask  = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp)
    | (1 << kCGEventFlagsChanged);
  event_tap   = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
      kCGEventTapOptionDefault, event_mask, event_handler, loop);
  EXPECT(event_tap, "CGEventTapCreate() failed");

  source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap, 0);
  EXPECT(source, "CFMachPortCreateRunLoopSource() failed");

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

