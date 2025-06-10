/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "private/iov-helper.h"
#include "private/util.h"

static char data1[] = "ABCD";
static char data2[] = "EFGHIJKL";
static char data3[] = "MNOPQRSTUVWX";

static void test_iov_len_normal(void)
{
    struct iovec iovs[] = {{.iov_base = data1, .iov_len = 4},
                           {.iov_base = data2, .iov_len = 8},
                           {.iov_base = data3, .iov_len = 12}};

    uint32_t len = roce_iov_len(iovs, ARRAY_SIZE(iovs));
    CU_ASSERT_EQUAL(len, 24);
}

static void test_iov_len_empty(void)
{
    struct iovec iovs[] = {{.iov_base = NULL, .iov_len = 0},
                           {.iov_base = data1, .iov_len = 4},
                           {.iov_base = NULL, .iov_len = 0}};

    uint32_t len = roce_iov_len(iovs, ARRAY_SIZE(iovs));
    CU_ASSERT_EQUAL(len, 4);
}

static void test_iov_dup_normal(void)
{
    struct iovec src[] = {{.iov_base = data1, .iov_len = 4}, {.iov_base = data2, .iov_len = 8}};
    struct iovec dst[2] = {0};

    roce_iov_dup(src, dst, ARRAY_SIZE(src));

    CU_ASSERT_EQUAL(dst[0].iov_len, 4);
    CU_ASSERT_EQUAL(dst[0].iov_base, data1);
    CU_ASSERT_EQUAL(dst[1].iov_len, 8);
    CU_ASSERT_EQUAL(dst[1].iov_base, data2);
}

static void test_iov_copy_full(void)
{
    struct iovec src[] = {{.iov_base = data1, .iov_len = 4}, {.iov_base = data2, .iov_len = 8}};
    struct iovec dst[2] = {0};

    uint32_t num = roce_iov_copy(src, ARRAY_SIZE(src), 0, 12, dst);

    CU_ASSERT_EQUAL(num, 2);
    CU_ASSERT_EQUAL(dst[0].iov_len, 4);
    CU_ASSERT_EQUAL(dst[0].iov_base, data1);
    CU_ASSERT_EQUAL(dst[1].iov_len, 8);
    CU_ASSERT_EQUAL(dst[1].iov_base, data2);
}

static void test_iov_copy_with_offset(void)
{
    struct iovec src[] = {{.iov_base = data1, .iov_len = 4},
                          {.iov_base = data2, .iov_len = 8},
                          {.iov_base = data3, .iov_len = 12}};
    struct iovec dst[3] = {0};

    uint32_t num = roce_iov_copy(src, ARRAY_SIZE(src), 2, 10, dst);

    CU_ASSERT_EQUAL(num, 2);
    CU_ASSERT_EQUAL(dst[0].iov_len, 2);
    CU_ASSERT_EQUAL(dst[0].iov_base, data1 + 2);
    CU_ASSERT_EQUAL(dst[1].iov_len, 8);
    CU_ASSERT_EQUAL(dst[1].iov_base, data2);
}

static void test_iov_copy_skip_first(void)
{
    struct iovec src[] = {{.iov_base = data1, .iov_len = 4},
                          {.iov_base = data2, .iov_len = 8},
                          {.iov_base = data3, .iov_len = 12}};
    struct iovec dst[3] = {0};

    uint32_t num = roce_iov_copy(src, ARRAY_SIZE(src), 4, 20, dst);

    CU_ASSERT_EQUAL(num, 2);
    CU_ASSERT_EQUAL(dst[0].iov_len, 8);
    CU_ASSERT_EQUAL(dst[0].iov_base, data2);
    CU_ASSERT_EQUAL(dst[1].iov_len, 12);
    CU_ASSERT_EQUAL(dst[1].iov_base, data3);
}

static void test_iov_copy_small_bytes(void)
{
    struct iovec src[] = {{.iov_base = data1, .iov_len = 4}, {.iov_base = data2, .iov_len = 8}};
    struct iovec dst[2] = {0};

    uint32_t num = roce_iov_copy(src, ARRAY_SIZE(src), 1, 2, dst);

    CU_ASSERT_EQUAL(num, 1);
    CU_ASSERT_EQUAL(dst[0].iov_len, 2);
    CU_ASSERT_EQUAL(dst[0].iov_base, data1 + 1);
}

static void test_iov_advance_normal(void)
{
    struct iovec iovs[] = {{.iov_base = data1, .iov_len = 4}, {.iov_base = data2, .iov_len = 8}};
    uint8_t buf[12] = {0};

    uint32_t copied = roce_iov_advance(iovs, ARRAY_SIZE(iovs), 6, buf);

    CU_ASSERT_EQUAL(copied, 6);
    CU_ASSERT_EQUAL(iovs[0].iov_len, 0);
    CU_ASSERT_EQUAL(iovs[0].iov_base, NULL);
    CU_ASSERT_EQUAL(iovs[1].iov_len, 6);
    CU_ASSERT_EQUAL(iovs[1].iov_base, data2 + 2);
    CU_ASSERT_NSTRING_EQUAL((char *)buf, "ABCDEF", 6);
}

static void test_iov_advance_exact(void)
{
    struct iovec iovs[] = {{.iov_base = data1, .iov_len = 4}, {.iov_base = data2, .iov_len = 8}};
    uint8_t buf[12] = {0};

    uint32_t copied = roce_iov_advance(iovs, ARRAY_SIZE(iovs), 12, buf);

    CU_ASSERT_EQUAL(copied, 12);
    CU_ASSERT_EQUAL(iovs[0].iov_len, 0);
    CU_ASSERT_EQUAL(iovs[0].iov_base, NULL);
    CU_ASSERT_EQUAL(iovs[1].iov_len, 0);
    CU_ASSERT_EQUAL(iovs[1].iov_base, NULL);
}

static void test_iov_advance_discard(void)
{
    struct iovec iovs[] = {{.iov_base = data1, .iov_len = 4}, {.iov_base = data2, .iov_len = 8}};

    uint32_t copied = roce_iov_advance(iovs, ARRAY_SIZE(iovs), 5, NULL);

    CU_ASSERT_EQUAL(copied, 5);
    CU_ASSERT_EQUAL(iovs[0].iov_len, 0);
    CU_ASSERT_EQUAL(iovs[0].iov_base, NULL);
    CU_ASSERT_EQUAL(iovs[1].iov_len, 7);
    CU_ASSERT_EQUAL(iovs[1].iov_base, data2 + 1);
}

static void test_iov_reverse_normal(void)
{
    struct iovec iovs[] = {{.iov_base = data1, .iov_len = 4},
                           {.iov_base = data2, .iov_len = 8},
                           {.iov_base = data3, .iov_len = 12}};
    uint8_t buf[24] = {0};

    uint32_t copied = roce_iov_reverse(iovs, ARRAY_SIZE(iovs), 6, buf);

    CU_ASSERT_EQUAL(copied, 6);
    CU_ASSERT_EQUAL(iovs[2].iov_len, 6);
    CU_ASSERT_EQUAL(iovs[2].iov_base, data3);
    CU_ASSERT_NSTRING_EQUAL((char *)buf, "STUVWX", 6);
}

static void test_iov_reverse_exact(void)
{
    struct iovec iovs[] = {{.iov_base = data1, .iov_len = 4}, {.iov_base = data2, .iov_len = 8}};
    uint8_t buf[12] = {0};

    uint32_t copied = roce_iov_reverse(iovs, 2, 12, buf);

    CU_ASSERT_EQUAL(copied, 12);
    CU_ASSERT_EQUAL(iovs[0].iov_len, 0);
    CU_ASSERT_EQUAL(iovs[0].iov_base, NULL);
    CU_ASSERT_EQUAL(iovs[1].iov_len, 0);
    CU_ASSERT_EQUAL(iovs[1].iov_base, NULL);
}

static void test_iov_reverse_discard(void)
{
    struct iovec iovs[] = {{.iov_base = data1, .iov_len = 4},
                           {.iov_base = data2, .iov_len = 8},
                           {.iov_base = data3, .iov_len = 12}};

    uint32_t copied = roce_iov_reverse(iovs, 3, 10, NULL);

    CU_ASSERT_EQUAL(copied, 10);
    CU_ASSERT_EQUAL(iovs[2].iov_len, 2);
    CU_ASSERT_EQUAL(iovs[2].iov_base, data3);
}

static void test_edge_zero_bytes(void)
{
    struct iovec iovs[] = {{.iov_base = data1, .iov_len = 4}};
    struct iovec dst[1] = {0};

    uint32_t len = roce_iov_copy(iovs, 1, 0, 0, dst);
    CU_ASSERT_EQUAL(len, 0);

    uint32_t adv = roce_iov_advance(iovs, 1, 0, NULL);
    CU_ASSERT_EQUAL(adv, 0);

    uint32_t rev = roce_iov_reverse(iovs, 1, 0, NULL);
    CU_ASSERT_EQUAL(rev, 0);
}

static void test_edge_all_zero_lengths(void)
{
    struct iovec iovs[] = {{.iov_base = NULL, .iov_len = 0}, {.iov_base = NULL, .iov_len = 0}};

    uint32_t len = roce_iov_len(iovs, 2);
    CU_ASSERT_EQUAL(len, 0);
}

static int setup_suite(void)
{
    return 0;
}

static int teardown_suite(void)
{
    return 0;
}

int main(void)
{
    CU_pSuite suite = NULL;

    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    suite = CU_add_suite("IOV Helper Tests", setup_suite, teardown_suite);
    if (suite == NULL) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite, test_iov_len_normal);
    CU_ADD_TEST(suite, test_iov_len_empty);
    CU_ADD_TEST(suite, test_iov_dup_normal);
    CU_ADD_TEST(suite, test_iov_copy_full);
    CU_ADD_TEST(suite, test_iov_copy_with_offset);
    CU_ADD_TEST(suite, test_iov_copy_skip_first);
    CU_ADD_TEST(suite, test_iov_copy_small_bytes);
    CU_ADD_TEST(suite, test_iov_advance_normal);
    CU_ADD_TEST(suite, test_iov_advance_exact);
    CU_ADD_TEST(suite, test_iov_advance_discard);
    CU_ADD_TEST(suite, test_iov_reverse_normal);
    CU_ADD_TEST(suite, test_iov_reverse_exact);
    CU_ADD_TEST(suite, test_iov_reverse_discard);
    CU_ADD_TEST(suite, test_edge_zero_bytes);
    CU_ADD_TEST(suite, test_edge_all_zero_lengths);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();

    return failures;
}
