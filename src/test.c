// Tests for common code
//
// Platforms
//   macOS AArch64
// Build
//   ./build.sh
// Run
//   ./build/test

#include "common.h"

#define ESC_DEL "\033[3~"

void test_math(void) {
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
}

void test_helper(void) {
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
}

void test_x_to_a(void) {
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
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "00000000000000000123XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20(buf, 123);
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "09223372036854775807XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20(buf, 9223372036854775807);
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "18446744073709551615XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20(buf, 18446744073709551615ull);
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // i64
  {
    ALIGNED(32) u8 expected[32] = "+0000000000000000000XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20(buf, 0);
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "+0000000000000000123XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20(buf, 123);
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "+9223372036854775807XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20(buf, 9223372036854775807);
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
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
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "_________________123XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20_fmt_right(buf, 123, '_');
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "_9223372036854775807XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20_fmt_right(buf, 9223372036854775807, '_');
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "18446744073709551615XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    u64_to_a20_fmt_right(buf, 18446744073709551615ull, '_');
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
  // i64 fmt
  {
    ALIGNED(32) u8 expected[32] = "__________________+0XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20_fmt_right(buf, 0, '_');
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "________________+123XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20_fmt_right(buf, 123, '_');
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }

  {
    ALIGNED(32) u8 expected[32] = "+9223372036854775807XXXXXXXXXXXX";
    ALIGNED(32) u8 buf[32] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    i64_to_a20_fmt_right(buf, 9223372036854775807, '_');
    TEST_EXPECT(is_mem_eq_aligned32(buf, expected, ARRAY_COUNT(buf)) == 1);
  }
}

// --------------------------------------
// Entry point (aka main)
// --------------------------------------
void start(void) {
  print_cstr(STDOUT, "TESTING MATH          ...");
  test_math();
  print_cstr(STDOUT, " OK\n");

  print_cstr(STDOUT, "TESTING HELPER        ...");
  test_helper();
  print_cstr(STDOUT, " OK\n");

  print_cstr(STDOUT, "TESTING X to A        ...");
  test_x_to_a();
  print_cstr(STDOUT, " OK\n");

  exit(0);
}
