#include <unity.h>

void test_template_smoke_test() {
    TEST_ASSERT_TRUE(true);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_template_smoke_test);
    return UNITY_END();
}
