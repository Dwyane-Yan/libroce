/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "private/sge-helper.h"

#define MTU 1024
#define PAGE_SIZE 4096

static void test_sge_len(void)
{
    uint64_t addr = 0x10000000;
    roce_sge sges[] = {
        {.addr = addr, .length = 256, .lkey = 0},     {.addr = addr * 2, .length = 512, .lkey = 1},
        {.addr = addr * 3, .length = 512, .lkey = 2}, {.addr = addr * 4, .length = 512, .lkey = 3},
        {.addr = addr * 5, .length = 512, .lkey = 4}, {.addr = addr * 6, .length = 128, .lkey = 5},
    };

    CU_ASSERT_EQUAL(roce_sge_len(sges, ARRAY_SIZE(sges), 0, 0), 2432);
    CU_ASSERT_EQUAL(roce_sge_len(sges, ARRAY_SIZE(sges), 1, 0), 2176);
    CU_ASSERT_EQUAL(roce_sge_len(sges, ARRAY_SIZE(sges), 1, 256), 1920);
    CU_ASSERT_EQUAL(roce_sge_len(sges, ARRAY_SIZE(sges), 5, 0), 128);
    CU_ASSERT_EQUAL(roce_sge_len(sges, ARRAY_SIZE(sges), 5, 64), 64);
}

static void test_sge_pages(void)
{
    uint64_t addr = 0x10000000;
    roce_sge sges0[] = {{.addr = addr - 128, .length = 256 + 8192, .lkey = 0}};
    CU_ASSERT_EQUAL(roce_sge_pages(sges0, 1, PAGE_SIZE), 4);

    roce_sge sges1[] = {{.addr = addr - 128, .length = 256, .lkey = 0},
                        {.addr = addr * 2 + 128, .length = 512, .lkey = 1}};
    CU_ASSERT_EQUAL(roce_sge_pages(sges1, 2, PAGE_SIZE), 3);

    roce_sge sges2[] = {{.addr = addr - 128, .length = 2048, .lkey = 0},
                        {.addr = addr * 2 + 2048, .length = 512 + 2048, .lkey = 1}};
    CU_ASSERT_EQUAL(roce_sge_pages(sges2, 2, PAGE_SIZE), 4);

    roce_sge sges3[] = {{.addr = 0, .length = 1, .lkey = 0}};
    CU_ASSERT_EQUAL(roce_sge_pages(sges3, 1, PAGE_SIZE), 1);
}

static void test_advance_small(void)
{
    uint64_t addr = 0x10000000;
    roce_sge src[] = {
        {.addr = addr, .length = 256, .lkey = 0},     {.addr = addr * 2, .length = 512, .lkey = 1},
        {.addr = addr * 3, .length = 512, .lkey = 2}, {.addr = addr * 4, .length = 512, .lkey = 3},
        {.addr = addr * 5, .length = 512, .lkey = 4}, {.addr = addr * 6, .length = 128, .lkey = 5},
    };
    roce_sge dst[8] = {0};
    uint32_t sge_idx = 0, off = 0;
    uint32_t total = roce_sge_len(src, ARRAY_SIZE(src), 0, 0);
    uint32_t dst_cnt;

    dst_cnt = roce_sge_advance(src, ARRAY_SIZE(src), MTU, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 3);
    CU_ASSERT_EQUAL(sge_idx, 2);
    CU_ASSERT_EQUAL(off, 256);
    CU_ASSERT_EQUAL(dst[0].addr, addr);
    CU_ASSERT_EQUAL(dst[0].length, 256);
    CU_ASSERT_EQUAL(dst[1].addr, addr * 2);
    CU_ASSERT_EQUAL(dst[1].length, 512);
    CU_ASSERT_EQUAL(dst[2].addr, addr * 3);
    CU_ASSERT_EQUAL(dst[2].length, 256);
    total -= MTU;

    dst_cnt = roce_sge_advance(src, 6, MTU, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 3);
    CU_ASSERT_EQUAL(sge_idx, 4);
    CU_ASSERT_EQUAL(off, 256);
    CU_ASSERT_EQUAL(dst[0].addr, addr * 3 + 256);
    CU_ASSERT_EQUAL(dst[0].length, 256);
    CU_ASSERT_EQUAL(dst[1].addr, addr * 4);
    CU_ASSERT_EQUAL(dst[1].length, 512);
    CU_ASSERT_EQUAL(dst[2].addr, addr * 5);
    CU_ASSERT_EQUAL(dst[2].length, 256);
    total -= MTU;

    CU_ASSERT_EQUAL(total, 384);
    dst_cnt = roce_sge_advance(src, 6, total, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 2);
    CU_ASSERT_EQUAL(sge_idx, 6);
    CU_ASSERT_EQUAL(off, 0);
    CU_ASSERT_EQUAL(dst[0].addr, addr * 5 + 256);
    CU_ASSERT_EQUAL(dst[0].length, 256);
    CU_ASSERT_EQUAL(dst[1].addr, addr * 6);
    CU_ASSERT_EQUAL(dst[1].length, 128);
}

static void test_advance_large(void)
{
    uint64_t addr = 0x10000000;
    roce_sge src[] = {
        {.addr = addr, .length = 2048, .lkey = 0},
        {.addr = addr * 2, .length = 1024 + 511, .lkey = 1},
        {.addr = addr * 3, .length = 768, .lkey = 2},
    };
    roce_sge dst[8] = {0};
    uint32_t sge_idx = 0, off = 0;
    uint32_t total = roce_sge_len(src, ARRAY_SIZE(src), 0, 0);
    uint32_t dst_cnt;

    /* src[0] 0-1023 */
    dst_cnt = roce_sge_advance(src, ARRAY_SIZE(src), MTU, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 0);
    CU_ASSERT_EQUAL(off, 1024);
    CU_ASSERT_EQUAL(dst[0].addr, addr);
    CU_ASSERT_EQUAL(dst[0].length, 1024);
    total -= MTU;

    /* src[0] 1024-2047 */
    dst_cnt = roce_sge_advance(src, ARRAY_SIZE(src), MTU, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 1);
    CU_ASSERT_EQUAL(off, 0);
    CU_ASSERT_EQUAL(dst[0].addr, addr + 1024);
    CU_ASSERT_EQUAL(dst[0].length, 1024);
    total -= MTU;

    /* src[1] 0-1023 */
    dst_cnt = roce_sge_advance(src, ARRAY_SIZE(src), MTU, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 1);
    CU_ASSERT_EQUAL(off, 1024);
    CU_ASSERT_EQUAL(dst[0].addr, addr * 2);
    CU_ASSERT_EQUAL(dst[0].length, 1024);
    total -= MTU;

    /* src[1] 1024-1535, src[2] 0-512 */
    dst_cnt = roce_sge_advance(src, ARRAY_SIZE(src), MTU, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 2);
    CU_ASSERT_EQUAL(sge_idx, 2);
    CU_ASSERT_EQUAL(off, 513);
    CU_ASSERT_EQUAL(dst[0].addr, addr * 2 + 1024);
    CU_ASSERT_EQUAL(dst[0].length, 511);
    CU_ASSERT_EQUAL(dst[1].addr, addr * 3);
    CU_ASSERT_EQUAL(dst[1].length, 513);
    total -= MTU;

    /* src[1] 513-767 */
    CU_ASSERT_EQUAL(total, 255);
    dst_cnt = roce_sge_advance(src, ARRAY_SIZE(src), total, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 3);
    CU_ASSERT_EQUAL(off, 0);
    CU_ASSERT_EQUAL(dst[0].addr, addr * 3 + 513);
    CU_ASSERT_EQUAL(dst[0].length, 255);
}

static void test_advance_edge(void)
{
    uint64_t addr = 0x10000000;
    roce_sge src[] = {
        {.addr = addr, .length = 100, .lkey = 0},
        {.addr = addr + 100, .length = 200, .lkey = 1},
    };
    roce_sge dst[4] = {0};
    uint32_t sge_idx = 0, off = 0;
    uint32_t dst_cnt;

    dst_cnt = roce_sge_advance(src, 2, 0, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 0);
    CU_ASSERT_EQUAL(sge_idx, 0);
    CU_ASSERT_EQUAL(off, 0);

    dst_cnt = roce_sge_advance(src, 2, 50, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 0);
    CU_ASSERT_EQUAL(off, 50);
    CU_ASSERT_EQUAL(dst[0].addr, addr);
    CU_ASSERT_EQUAL(dst[0].length, 50);

    dst_cnt = roce_sge_advance(src, 2, 50, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 1);
    CU_ASSERT_EQUAL(off, 0);
    CU_ASSERT_EQUAL(dst[0].addr, addr + 50);
    CU_ASSERT_EQUAL(dst[0].length, 50);

    dst_cnt = roce_sge_advance(src, 2, 150, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 1);
    CU_ASSERT_EQUAL(off, 150);
    CU_ASSERT_EQUAL(dst[0].addr, addr + 100);
    CU_ASSERT_EQUAL(dst[0].length, 150);
}

static void test_advance_exact_boundary(void)
{
    uint64_t addr = 0x10000000;
    roce_sge src[] = {
        {.addr = addr, .length = 256, .lkey = 0},
        {.addr = addr + 256, .length = 256, .lkey = 1},
    };
    roce_sge dst[3] = {0};
    uint32_t sge_idx = 0, off = 0;
    uint32_t dst_cnt;

    dst_cnt = roce_sge_advance(src, 2, 256, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 1);
    CU_ASSERT_EQUAL(off, 0);
    CU_ASSERT_EQUAL(dst[0].addr, addr);
    CU_ASSERT_EQUAL(dst[0].length, 256);

    dst_cnt = roce_sge_advance(src, 2, 256, &sge_idx, &off, dst);
    CU_ASSERT_EQUAL(dst_cnt, 1);
    CU_ASSERT_EQUAL(sge_idx, 2);
    CU_ASSERT_EQUAL(off, 0);
    CU_ASSERT_EQUAL(dst[0].addr, addr + 256);
    CU_ASSERT_EQUAL(dst[0].length, 256);
}

static void test_pages_non_power_of_two(void)
{
    uint64_t addr = 0x10000000;
    roce_sge sge[] = {{.addr = addr, .length = 4096, .lkey = 0}};
    CU_ASSERT_EQUAL(roce_sge_pages(sge, 1, 1024), 4);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS)
        return CU_get_error();

    CU_pSuite suite = CU_add_suite("SGE_Helper_Test", NULL, NULL);
    if (!suite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite, test_sge_len);
    CU_ADD_TEST(suite, test_sge_pages);
    CU_ADD_TEST(suite, test_advance_small);
    CU_ADD_TEST(suite, test_advance_large);
    CU_ADD_TEST(suite, test_advance_edge);
    CU_ADD_TEST(suite, test_advance_exact_boundary);
    CU_ADD_TEST(suite, test_pages_non_power_of_two);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return failures;
}
