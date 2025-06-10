/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "roce/roce.h"

#include "private/resource.h"
#include "private/util.h"

static roce_ctx *test_ctx;
static roce_resource test_resource;

static int setup_resource(void)
{
    int ret = roce_resource_init(test_ctx, &test_resource, 10, "TST");
    return ret;
}

static void teardown_resource(void)
{
    if (test_resource.size > 0) {
        roce_resource_destroy(&test_resource);
    }
    memset(&test_resource, 0, sizeof(test_resource));
}

static void test_resource_init_normal(void)
{
    roce_resource res;
    int ret = roce_resource_init(test_ctx, &res, 5, "INIT");

    CU_ASSERT_EQUAL(ret, 0);
    CU_ASSERT_EQUAL(res.size, 5);
    CU_ASSERT_PTR_NOT_NULL(res.addr);
    CU_ASSERT_STRING_EQUAL(res.name, "INIT");

    roce_resource_destroy(&res);
}

static void test_resource_init_zero_size(void)
{
    roce_resource res;
    int ret = roce_resource_init(test_ctx, &res, 0, "ZERO");
    CU_ASSERT_NOT_EQUAL(ret, 0);
}

static void test_resource_alloc_single(void)
{
    setup_resource();

    roce_res res = {0};
    int idx = roce_resource_alloc(&test_resource, &res);

    CU_ASSERT_TRUE(idx >= 0 && idx < 10);
    void *ptr = roce_resource_get(&test_resource, idx);
    CU_ASSERT_EQUAL(ptr, &res);

    teardown_resource();
}

static void test_resource_alloc_multiple(void)
{
    setup_resource();
    int indices[5] = {0};
    roce_res res[5] = {0};

    for (int i = 0; i < ARRAY_SIZE(indices); i++) {
        indices[i] = roce_resource_alloc(&test_resource, &res[i]);
        CU_ASSERT_TRUE(indices[i] >= 0 && indices[i] < 10);
    }

    for (int i = 0; i < 5; i++) {
        void *ptr = roce_resource_get(&test_resource, indices[i]);
        CU_ASSERT_EQUAL(ptr, &res[i]);
    }

    teardown_resource();
}

static void test_resource_alloc_full(void)
{
    setup_resource();

    roce_res res[11] = {0};
    int idx;

    for (int i = 0; i < 10; i++) {
        idx = roce_resource_alloc(&test_resource, &res[i]);
        CU_ASSERT_EQUAL(idx, i);
    }

    idx = roce_resource_alloc(&test_resource, &res[10]);
    CU_ASSERT_EQUAL(idx, -EBUSY);

    teardown_resource();
}

static void test_resource_alloc_at_normal(void)
{
    setup_resource();

    roce_res res = {0};
    uint32_t target_idx = 5;
    int ret = roce_resource_alloc_at(&test_resource, &res, target_idx);

    CU_ASSERT_EQUAL(ret, target_idx);
    void *ptr = roce_resource_get(&test_resource, target_idx);
    CU_ASSERT_EQUAL(ptr, &res);

    teardown_resource();
}

static void test_resource_alloc_at_invalid_index(void)
{
    setup_resource();

    roce_res res = {0};
    int ret = roce_resource_alloc_at(&test_resource, &res, 10);
    CU_ASSERT_EQUAL(ret, -EINVAL);

    teardown_resource();
}

static void test_resource_alloc_at_conflict(void)
{
    setup_resource();
    uint32_t target_idx = 3;
    roce_res res = {0}, res1 = {0};

    int ret1 = roce_resource_alloc_at(&test_resource, &res, target_idx);
    CU_ASSERT_EQUAL(ret1, target_idx);

    int ret2 = roce_resource_alloc_at(&test_resource, &res1, target_idx);
    CU_ASSERT_EQUAL(ret2, -EBUSY);

    teardown_resource();
}

static void test_resource_get_invalid_index(void)
{
    setup_resource();
    void *ptr = roce_resource_get(&test_resource, 10);
    CU_ASSERT_PTR_NULL(ptr);

    teardown_resource();
}

static void test_resource_get_empty_slot(void)
{
    setup_resource();
    void *ptr = roce_resource_get(&test_resource, 0);
    CU_ASSERT_PTR_NULL(ptr);

    teardown_resource();
}

static void test_resource_free_normal(void)
{
    setup_resource();
    roce_res res = {0};

    int idx = roce_resource_alloc(&test_resource, &res);
    CU_ASSERT_TRUE(idx >= 0);

    void *ptr = roce_resource_get(&test_resource, idx);
    CU_ASSERT_PTR_NOT_NULL(ptr);

    int ret = roce_resource_free(&test_resource, idx);
    CU_ASSERT_EQUAL(ret, 0);

    ptr = roce_resource_get(&test_resource, idx);
    CU_ASSERT_PTR_NULL(ptr);

    teardown_resource();
}

static void test_resource_free_invalid_index(void)
{
    setup_resource();
    int ret = roce_resource_free(&test_resource, 10);
    CU_ASSERT_EQUAL(ret, -EINVAL);

    teardown_resource();
}

static void test_resource_free_empty_slot(void)
{
    setup_resource();
    int ret = roce_resource_free(&test_resource, 0);
    CU_ASSERT_EQUAL(ret, -EINVAL);

    teardown_resource();
}

static void test_resource_alloc_after_free(void)
{
    setup_resource();
    roce_res res = {0};
    int idx1 = roce_resource_alloc(&test_resource, &res);
    CU_ASSERT_EQUAL(idx1, 0);

    int ret = roce_resource_free(&test_resource, idx1);
    CU_ASSERT_EQUAL(ret, 0);

    int idx2 = roce_resource_alloc(&test_resource, &res);
    CU_ASSERT_EQUAL(idx2, 0);

    teardown_resource();
}

static void test_resource_get_concurrent_style(void)
{
    setup_resource();
    roce_res res = {0};
    int idx = roce_resource_alloc(&test_resource, &res);
    CU_ASSERT_TRUE(idx >= 0);

    void *ptr1 = roce_resource_get(&test_resource, idx);
    void *ptr2 = roce_resource_get(&test_resource, idx);

    CU_ASSERT_EQUAL(ptr1, ptr2);

    teardown_resource();
}

static void test_net_xmit(void *ctx, uint16_t queue, struct iovec *iovs, uint32_t num_iov)
{
}

static int test_cq_comp(void *ctx_opaque, roce_wc *wc, int cq, void *cq_ctx)
{
    return 0;
}

static void *test_dma_map(void *ctx_opaque, uint64_t hwaddr, uint32_t len)
{
    return NULL;
}

static void test_dma_unmap(void *ctx_opaque, void *addr, uint32_t len)
{
}

static void roce_log_stdout(void *ctx_opaque, char *msg)
{
}

static int setup_suite(void)
{
    roce_ctx_para para = {
        .version = ROCE_V2,
        .ctx_opaque = NULL,
        .page_size = 4096,
        .log_level = roce_log_warn,
        .log = roce_log_stdout,
        .net_xmit = test_net_xmit,
        .cq_comp = test_cq_comp,
        .dma_map = test_dma_map,
        .dma_unmap = test_dma_unmap,
    };
    test_ctx = roce_new_ctx(&para);
    assert(test_ctx);

    return 0;
}
static int teardown_suite(void)
{
    roce_free_ctx(test_ctx);
    return 0;
}

void add_resource_tests(void)
{
    CU_pSuite suite = CU_add_suite("Resource Tests", setup_suite, teardown_suite);

    CU_add_test(suite, "test_resource_init_normal", test_resource_init_normal);
    CU_add_test(suite, "test_resource_init_zero_size", test_resource_init_zero_size);
    CU_add_test(suite, "test_resource_alloc_single", test_resource_alloc_single);
    CU_add_test(suite, "test_resource_alloc_multiple", test_resource_alloc_multiple);
    CU_add_test(suite, "test_resource_alloc_full", test_resource_alloc_full);
    CU_add_test(suite, "test_resource_alloc_at_normal", test_resource_alloc_at_normal);
    CU_add_test(suite, "test_resource_alloc_at_invalid_index",
                test_resource_alloc_at_invalid_index);
    CU_add_test(suite, "test_resource_alloc_at_conflict", test_resource_alloc_at_conflict);
    CU_add_test(suite, "test_resource_get_invalid_index", test_resource_get_invalid_index);
    CU_add_test(suite, "test_resource_get_empty_slot", test_resource_get_empty_slot);
    CU_add_test(suite, "test_resource_free_normal", test_resource_free_normal);
    CU_add_test(suite, "test_resource_free_invalid_index", test_resource_free_invalid_index);
    CU_add_test(suite, "test_resource_free_empty_slot", test_resource_free_empty_slot);
    CU_add_test(suite, "test_resource_alloc_after_free", test_resource_alloc_after_free);
    CU_add_test(suite, "test_resource_get_concurrent_style", test_resource_get_concurrent_style);
}

int main(void)
{
    CU_initialize_registry();
    add_resource_tests();
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return failures;
}
