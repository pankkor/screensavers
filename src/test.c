// Tests for common code
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/test

#include "common.h"

static void test_constants(void) {
  u64 expected_u64_max = 18446744073709551615ull;
  u64 u64_max = U64_MAX;
  TEST_EXPECT(u64_max == expected_u64_max);

  u64 expected_u32_max = 4294967295;
  u64 u32_max = U32_MAX;
  TEST_EXPECT(u32_max == expected_u32_max);
}

static void test_neon(void) {
#if defined(__ARM_NEON)
  // Test uint8x16_t to hex
  {
    u8 expected[32] = "DEADBEEFDEADBEEFDEADBEEFDEADBEEF";
    u8 v_buf[16]    = {
      0xDE, 0xAD, 0xBE, 0xEF,
      0xDE, 0xAD, 0xBE, 0xEF,
      0xDE, 0xAD, 0xBE, 0xEF,
      0xDE, 0xAD, 0xBE, 0xEF,
    };
    u8 hex[32];
    uint8x16_t v    = vld1q_u8(v_buf);
    uint8x16_to_a32x(hex, v);
    TEST_EXPECT(is_mem_eq(hex, expected, ARRAY_COUNT(hex)) == 1);
  }

  {
    u8 expected[32] = "000102030405060708090A0B0C0D0E0F";
    u8 v_buf[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
    u8 hex[32];
    uint8x16_t v = vld1q_u8(v_buf);
    uint8x16_to_a32x(hex, v);
    TEST_EXPECT(is_mem_eq(hex, expected, ARRAY_COUNT(hex)) == 1);
  }

  {
    u8 expected[32] = "A0B1C2D3E4F5A6B7C8D9EAFBACBDEFF0";
    u8 v_buf[16] = {0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5, 0xA6, 0xB7, 0xC8, 0xD9,
      0xEA, 0xFB, 0xAC, 0xBD, 0xEF, 0xF0};
    u8 hex[32];
    uint8x16_t v    = vld1q_u8(v_buf);
    uint8x16_to_a32x(hex, v);
    TEST_EXPECT(is_mem_eq(hex, expected, ARRAY_COUNT(hex)) == 1);
  }

  // Test masks
  for (i32 size = 0; size < 32; ++size) {
    b32 is_FF00 = 0;
    u8 mask[32] = {0};
    for (i32 i = size; i < 32; ++i) { mask[i] = 0xFF; }
    uint8x16x2_t expected_vmask = { vld1q_u8(mask), vld1q_u8(mask + 16) };
    uint8x16x2_t vmask = mask32_neon(size, is_FF00);
    uint8x16_t eq0 = vceqq_u8(expected_vmask.val[0], vmask.val[0]);
    uint8x16_t eq1 = vceqq_u8(expected_vmask.val[1], vmask.val[1]);
    uint8_t min0 = vminvq_u8(eq0);
    uint8_t min1 = vminvq_u8(eq1);
    TEST_EXPECT(min0 == 0xFF);
    TEST_EXPECT(min1 == 0xFF);
  }

  for (i32 size = 0; size < 32; ++size) {
    b32 is_FF00 = 1;
    u8 mask[32] = {0};
    for (i32 i = 0; i < size; ++i) { mask[i] = 0xFF; }
    uint8x16x2_t expected_vmask = { vld1q_u8(mask), vld1q_u8(mask + 16) };
    uint8x16x2_t vmask = mask32_neon(size, is_FF00);
    uint8x16_t eq0 = vceqq_u8(expected_vmask.val[0], vmask.val[0]);
    uint8x16_t eq1 = vceqq_u8(expected_vmask.val[1], vmask.val[1]);
    uint8_t min0 = vminvq_u8(eq0);
    uint8_t min1 = vminvq_u8(eq1);
    TEST_EXPECT(min0 == 0xFF);
    TEST_EXPECT(min1 == 0xFF);
  }
#endif
}

static void test_math(void) {
  // abs
  TEST_EXPECT(absi32(2147483647) == 2147483647);
  TEST_EXPECT(absi32(12345678) == 12345678);
  TEST_EXPECT(absi32(1) == 1);
  TEST_EXPECT(absi32(0) == 0);
  TEST_EXPECT(absi32(-1) == 1);
  TEST_EXPECT(absi32(-12345678) == 12345678);
  TEST_EXPECT(absi32(-2147483647) == 2147483647);
  TEST_EXPECT(absi32(-2147483648) == 2147483648u);

  TEST_EXPECT(absi64(9223372036854775807) == 9223372036854775807);
  TEST_EXPECT(absi64(2147483648) == 2147483648);
  TEST_EXPECT(absi64(2147483647) == 2147483647);
  TEST_EXPECT(absi64(1) == 1);
  TEST_EXPECT(absi64(0) == 0);
  TEST_EXPECT(absi64(-1) == 1);
  TEST_EXPECT(absi64(-2147483648) == 2147483648);
  TEST_EXPECT(absi64(-9223372036854775807) == 9223372036854775807);
  TEST_EXPECT(absi64(-9223372036854775807ll - 1) == 9223372036854775808ull);

  // Fetch add u32
  {
    u32 a = 0xFFFFFFFF;
    TEST_EXPECT(fetch_add_u32(&a, 0x10000) == 0xFFFFFFFF);
    TEST_EXPECT(a == 0x0000FFFF);
  }
  {
    u32 a = 0xFFFFFFFF;
    TEST_EXPECT(fetch_add_u32(&a, 0x1) == 0xFFFFFFFF);
    TEST_EXPECT(a == 0x00000000);
  }
  {
    u32 a = 0x0000FFFF;
    TEST_EXPECT(fetch_add_u32(&a, 0x1) == 0x0000FFFF);
    TEST_EXPECT(a == 0x00010000);
  }
  // Fetch add u64
  {
    u64 a = 0xFFFFFFFFFFFFFFFF;
    TEST_EXPECT(fetch_add_u64(&a, 0x100000000) == 0xFFFFFFFFFFFFFFFF);
    TEST_EXPECT(a == 0x00000000FFFFFFFF);
  }
  {
    u64 a = 0xFFFFFFFFFFFFFFFF;
    TEST_EXPECT(fetch_add_u64(&a, 0x1) == 0xFFFFFFFFFFFFFFFF);
    TEST_EXPECT(a == 0x0000000000000000);
  }
  // Fetch add mixed
  {
    u64 a = 0x00000000FFFFFFFF;
    TEST_EXPECT(fetch_add_u64(&a, 0x1) == 0x00000000FFFFFFFF);
    TEST_EXPECT(a == 0x0000000100000000);
  }
  {
    u64 a = 0xFFFFFFFFFFFFFFFF;
    TEST_EXPECT(fetch_add_u32((u32 *)&a, 0x1) == 0xFFFFFFFF);
    TEST_EXPECT(a == 0xFFFFFFFF00000000);
  }
  {
    u64 a = 0xFFFFFFFFFFFFFFFF;
    TEST_EXPECT(fetch_add_u32((u32 *)&a + 1, 0x1) == 0xFFFFFFFF);
    TEST_EXPECT(a == 0x00000000FFFFFFFF);
  }
}

static void test_memory(void) {
  // Unaligned memory comparison is_mem_eq()
  {
    // is_mem_eq(): equal up to `size` characters
    u8 l[128] =
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"  // 64
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"; // 64
    for (i32 size = 0; size < ARRAY_COUNT(l) - 1; ++size) {
      u8 r[128] =
        "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"
        "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE";
      // Making char after size non-equal shall not alter equality
      r[size] = 'X';
      TEST_EXPECT(is_mem_eq(l, r, size) == 1);
    }
  }

  {
    // is_mem_eq(): non-equal at `wrong` index
    u8 l[128] =
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"  // 64
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"; // 64
    for (i32 size = 0; size < ARRAY_COUNT(l); ++size) {
      for (i32 wrong = 0; wrong < size; ++wrong) {
        u8 r[128] =
          "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"
          "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE";
        r[wrong] = 'X';
        TEST_EXPECT(is_mem_eq(l, r, size) == 0);
      }
    }
  }
  // Aligned memory comparison is_mem_eq_aligned32()
  {
    // is_mem_eq_aligned32(): equal up to `size` characters
    ALIGNED(32) u8 l[128] =
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"  // 64
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"; // 64
    for (i32 size = 0; size < ARRAY_COUNT(l) - 1; ++size) {
      ALIGNED(32) u8 r[128] =
        "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"
        "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE";
      // Making char after size non-equal shall not alter equality
      r[size] = 'X';
      TEST_EXPECT(is_mem_eq_aligned32(l, r, size) == 1);
    }
  }

  {
    // is_mem_eq_aligned32(): non-equal at `wrong` index
    ALIGNED(32) u8 l[128] =
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"  // 64
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"; // 64
    for (i32 size = 0; size < ARRAY_COUNT(l); ++size) {
      for (i32 wrong = 0; wrong < size; ++wrong) {
        ALIGNED(32) u8 r[128] =
          "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"
          "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE";
        r[wrong] = 'X';
        TEST_EXPECT(is_mem_eq_aligned32(l, r, size) == 0);
      }
    }
  }

  {
    // is_mem_eq_aligned32(): non-equal at `wrong` index
    u8 l[16] = "0123456789ABCDEF";  // 16
    for (i32 size = 0; size < ARRAY_COUNT(l); ++size) {
      for (i32 wrong = 0; wrong < size; ++wrong) {
        u8 r[16] = "0123456789ABCDEF";
        r[wrong] = 'X';
        TEST_EXPECT(is_mem_eq_aligned32(l, r, size) == 0);
      }
    }
  }

  // mem_cp
  {
    ALIGNED(32) u8 src[128] =
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"  // 64
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"; // 64
    for (i32 size = 0; size < ARRAY_COUNT(src); ++size) {
      ALIGNED(32) u8 dst[128] =
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
      ALIGNED(32) u8 expected[128] =
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
      for (i32 i = 0; i < size; ++i) { expected[i] = src[i]; }

      mem_cp(dst, src, size);
      TEST_EXPECT(is_mem_eq(dst, src, size) == 1);
      TEST_EXPECT(is_mem_eq(dst, expected, ARRAY_COUNT(dst)) == 1);
    }
  }

  // mem_cp_aligned32
  {
    ALIGNED(32) u8 src[128] =
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"  // 64
      "0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE0123456789ABCDFE"; // 64
    for (i32 size = 0; size < ARRAY_COUNT(src); ++size) {
      ALIGNED(32) u8 dst[128] =
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
      ALIGNED(32) u8 expected[128] =
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
      mem_cp(expected, src, size);

      mem_cp_aligned32(dst, src, size);
      TEST_EXPECT(is_mem_eq(dst, src, size) == 1);
      TEST_EXPECT(is_mem_eq(dst, expected, ARRAY_COUNT(dst)) == 1);
    }
  }
}

static void test_x_to_a(void) {
  // u32
  {
    u8 expected[16] = "0000000000XXXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    u32_to_a10(buf, 0);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "0000000123XXXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    u32_to_a10(buf, 123);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "4294967295XXXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    u32_to_a10(buf, 4294967295);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // i32
  {
    u8 expected[16] = "+0000000000XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11(buf, 0);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "+0000000123XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11(buf, 123);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "-0000000123XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11(buf, -123);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "+2147483647XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11(buf, 2147483647);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "-2147483648XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11(buf, -2147483648);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // u64
  {
    ALIGNED(32) u8 expected[32] = "00000000000000000000XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20(buf, 0);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "00000000000000000123XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20(buf, 123);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "09223372036854775807XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20(buf, 9223372036854775807);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "18446744073709551615XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20(buf, 18446744073709551615ull);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // i64
  {
    ALIGNED(32) u8 expected[32] = "+0000000000000000000XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20(buf, 0);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "+0000000000000000123XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20(buf, 123);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "+9223372036854775807XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20(buf, 9223372036854775807);
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // u32 fmt
  {
    u8 expected[16] = "_________0XXXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    u32_to_a10_fmt_right(buf, 0, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "_______123XXXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    u32_to_a10_fmt_right(buf, 123, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "4294967295XXXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    u32_to_a10_fmt_right(buf, 4294967295, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // i32 fmt
  {
    u8 expected[16] = "_________+0XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11_fmt_right(buf, 0, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "_______+123XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11_fmt_right(buf, 123, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "_______-123XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11_fmt_right(buf, -123, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "-2147483648XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11_fmt_right(buf, -2147483648, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    u8 expected[16] = "+2147483647XXXXX";
    u8 buf[16] = "XXXXXXXXXXXXXXXX";
    i32_to_a11_fmt_right(buf, 2147483647, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // u64 fmt
  {
    ALIGNED(32) u8 expected[32] = "___________________0XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20_fmt_right(buf, 0, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "_________________123XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20_fmt_right(buf, 123, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "_9223372036854775807XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20_fmt_right(buf, 9223372036854775807, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "18446744073709551615XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20_fmt_right(buf, 18446744073709551615ull, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // i64 fmt
  {
    ALIGNED(32) u8 expected[32] = "__________________+0XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20_fmt_right(buf, 0, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "________________+123XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20_fmt_right(buf, 123, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "+9223372036854775807XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20_fmt_right(buf, 9223372036854775807, '_');
    TEST_EXPECT(is_mem_eq(buf, expected, ARRAY_COUNT(buf)) == 1);
  }


  // Buffer to Hex
  {
    ALIGNED(32) u8 expected[32] = "DEADBEEFXXXXXXXXXXXXXXXXXXXXXXXX";
    ALIGNED(32) u8 in[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    ALIGNED(32) u8 out[32]      = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    buf_to_ax(out, in, ARRAY_COUNT(in));
    TEST_EXPECT(is_mem_eq(out, expected, ARRAY_COUNT(out)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "000102030405060708090A0B0C0D0E0F";
    ALIGNED(32) u8 in[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    ALIGNED(32) u8 out[32]      = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    buf_to_ax(out, in, ARRAY_COUNT(in));
    TEST_EXPECT(is_mem_eq(out, expected, ARRAY_COUNT(out)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "FFFEFDFCFBFAF9F8F7F6F5F4F3F2F1F0";
    ALIGNED(32) u8 in[16] = {255,254,253,252,251,250,249,248,247,246,245,244,
      243,242,241,240};
    ALIGNED(32) u8 out[32]      = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    buf_to_ax(out, in, ARRAY_COUNT(in));
    TEST_EXPECT(is_mem_eq(out, expected, ARRAY_COUNT(out)) == 1);
  }
}

// --------------------------------------
// Entry point (aka main)
// --------------------------------------
void start(void) {
  // Test memory routines first, because these routines are used in other tests
  print_cstr(STDOUT, "TESTING MEMORY        ... ");
  test_memory();

  print_cstr(STDOUT, "OK\n");
  print_cstr(STDOUT, "TESTING CONSTANTS     ... ");
  test_constants();
  print_cstr(STDOUT, "OK\n");

  print_cstr(STDOUT, "TESTING MATH          ... ");
  test_math();
  print_cstr(STDOUT, "OK\n");

  print_cstr(STDOUT, "TESTING X to A        ... ");
  test_x_to_a();
  print_cstr(STDOUT, "OK\n");

  print_cstr(STDOUT, "TESTING NEON          ... ");
  test_neon();
  print_cstr(STDOUT, "OK\n");

  exit(0);
}
