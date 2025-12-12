#include <unity.h>
#include "../mocks/testable_functions.h"

void setUp(void) {}
void tearDown(void) {}

void test_clampOperatorHandle10_trims_and_clamps(void) {
    char out[16];
    size_t n = clampOperatorHandle10("   NELEDOV_TOO_LONG   ", out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)n);
    TEST_ASSERT_EQUAL_STRING("NELEDOV_TOO", out);
}

void test_clampOperatorHandle10_empty_string(void) {
    char out[16];
    size_t n = clampOperatorHandle10("   ", out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT(0, (unsigned)n);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_replaceOpToken_replaces_all_instances(void) {
    char out[64];
    size_t n = replaceOpToken("$OP snout > $OP", "NELEDOV", out, sizeof(out));
    (void)n;
    TEST_ASSERT_EQUAL_STRING("NELEDOV snout > NELEDOV", out);
}

void test_replaceOpToken_fallback_op(void) {
    char out[32];
    size_t n = replaceOpToken("$OP", "", out, sizeof(out));
    (void)n;
    TEST_ASSERT_EQUAL_STRING("OP", out);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_clampOperatorHandle10_trims_and_clamps);
    RUN_TEST(test_clampOperatorHandle10_empty_string);
    RUN_TEST(test_replaceOpToken_replaces_all_instances);
    RUN_TEST(test_replaceOpToken_fallback_op);
    return UNITY_END();
}
