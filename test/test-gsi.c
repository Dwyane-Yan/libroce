/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "roce/roce.h"

#define PAGE_SIZE 4096
#define MR_SIZE (4 * 1024 * 1024)

static pthread_spinlock_t test_lock;

static void test_net_xmit(void *ctx_opaque, uint16_t queue, struct iovec *iovs, uint32_t num_iov)
{
}

static int test_cq_comp(void *ctx_opaque, roce_wc *wc, int cq, void *cq_ctx)
{
    return 0;
}

static void *test_dma_map(void *ctx_opaque, uint64_t hwaddr, uint32_t len)
{
    return (void *)(uintptr_t)hwaddr;
}

static void test_dma_unmap(void *ctx_opaque, void *addr, uint32_t len)
{
}

static void test_log_stdout(void *ctx_opaque, char *msg)
{
}

static void *test_malloc(size_t size)
{
    return malloc(size);
}

static void *test_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

static void test_free(void *ptr)
{
    free(ptr);
}

static void *test_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static int test_spin_init(void *lock, int shared)
{
    return pthread_spin_init((pthread_spinlock_t *)lock, shared);
}

static int test_spin_lock(void *lock)
{
    return pthread_spin_lock((pthread_spinlock_t *)lock);
}

static int test_spin_trylock(void *lock)
{
    return pthread_spin_trylock((pthread_spinlock_t *)lock);
}

static int test_spin_unlock(void *lock)
{
    return pthread_spin_unlock((pthread_spinlock_t *)lock);
}

static roce_ctx *test_create_ctx(void)
{
    roce_ctx_para para = {.version = ROCE_V2,
                          .ctx_opaque = NULL,
                          .page_size = PAGE_SIZE,
                          .log_level = roce_log_warn,
                          .log = test_log_stdout,
                          .net_xmit = test_net_xmit,
                          .cq_comp = test_cq_comp,
                          .dma_map = test_dma_map,
                          .dma_unmap = test_dma_unmap,
                          .malloc = test_malloc,
                          .free = test_free,
                          .calloc = test_calloc,
                          .realloc = test_realloc,
                          .spin = {
                              .lock_size = sizeof(pthread_spinlock_t),
                              .init = test_spin_init,
                              .lock = test_spin_lock,
                              .trylock = test_spin_trylock,
                              .unlock = test_spin_unlock,
                          }};
    roce_ctx *ctx = roce_new_ctx(&para);

    CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);
    return ctx;
}

static void test_init_device(roce_ctx *ctx)
{
    roce_device_attr attr = {
        .max_pd = 128,
        .max_ah = 128,
        .max_mr_size = 16 * 1024 * 1024,
        .max_qp = 1024,
        .max_qp_wr = 32,
        .max_sge = 16,
        .max_cq = 1024,
        .max_cqe = 4096,
        .max_mr = 4096,
        .max_inline_data = 128,
        .max_pkeys = 1,
        .phys_port_cnt = 1,
    };
    int ret = roce_init_device(ctx, &attr);

    CU_ASSERT_EQUAL(ret, 0);
}

static void test_init_port_ipv4(roce_ctx *ctx)
{
    roce_port_attr attr = {
        .max_mtu = 1024,
        .active_mtu = 1024,
    };
    uint8_t mac[ROCE_MAC_LEN] = {0xfe, 0xe1, 0xc0, 0x01, 0x00, 0x01};
    uint8_t gid[ROCE_GID_LEN] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 192, 168, 122, 101};
    int ret;

    ret = roce_init_port(ctx, 1, &attr);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_set_port_mac(ctx, 1, mac);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_add_gid(ctx, 1, 0, gid);
    CU_ASSERT_EQUAL(ret, 0);
}

static void test_init_port_ipv6(roce_ctx *ctx)
{
    roce_port_attr attr = {
        .max_mtu = 1024,
        .active_mtu = 1024,
    };
    uint8_t mac[ROCE_MAC_LEN] = {0xfe, 0xe1, 0xc0, 0x01, 0x00, 0x02};
    uint8_t gid[ROCE_GID_LEN] = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    int ret;

    ret = roce_init_port(ctx, 1, &attr);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_set_port_mac(ctx, 1, mac);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_add_gid(ctx, 1, 0, gid);
    CU_ASSERT_EQUAL(ret, 0);
}

static void test_create_and_destroy_gsi_ipv4(void)
{
    roce_ctx *ctx = test_create_ctx();
    roce_qp_cap cap = {
        .max_send_wr = 32,
        .max_recv_wr = 32,
        .max_send_sge = 16,
        .max_recv_sge = 16,
        .max_inline_data = 128,
    };
    int pd, cq, gsi, mr;
    uint32_t lkey, rkey;

    test_init_device(ctx);
    test_init_port_ipv4(ctx);

    /* allocate PD */
    pd = roce_alloc_pd(ctx, NULL);
    CU_ASSERT_TRUE(pd >= 0);

    /* allocate CQ */
    cq = roce_create_cq(ctx, 128, 0, NULL);
    CU_ASSERT_TRUE(cq >= 0);

    /* create GSI QP */
    gsi = roce_create_qp(ctx, pd, ROCE_QPT_GSI, -1, cq, cq, &cap, ROCE_QP_SIG_ALL, NULL);
    CU_ASSERT_TRUE(gsi >= 0);

    /* create DMA MR for GSI */
    mr = roce_create_mr(ctx, pd, ROCE_MR_DMA, 0, MR_SIZE, ROCE_ACCESS_LOCAL_WRITE, NULL, 0,
                        PAGE_SIZE, NULL);
    CU_ASSERT_TRUE(mr >= 0);

    /* get MR keys */
    CU_ASSERT_EQUAL(roce_get_mr_key(ctx, mr, &lkey, &rkey), 0);

    /* allocate and post recv WR */
    roce_recv_wr *recv_wr;
    CU_ASSERT_EQUAL(roce_alloc_recv_wr(ctx, 1, &recv_wr), 0);

    recv_wr->wr_id = 0;
    recv_wr->sge[0].addr = 4096;
    recv_wr->sge[0].length = 256;
    recv_wr->sge[0].lkey = lkey;

    CU_ASSERT_EQUAL(roce_post_recv(ctx, gsi, recv_wr), 0);

    /* cleanup */
    CU_ASSERT_EQUAL(roce_destroy_mr(ctx, mr), 0);
    CU_ASSERT_EQUAL(roce_destroy_qp(ctx, gsi), 0);
    CU_ASSERT_EQUAL(roce_destroy_cq(ctx, cq), 0);
    CU_ASSERT_EQUAL(roce_dealloc_pd(ctx, pd), 0);

    roce_free_ctx(ctx);
}

static void test_create_and_destroy_gsi_ipv6(void)
{
    roce_ctx *ctx = test_create_ctx();
    roce_qp_cap cap = {
        .max_send_wr = 32,
        .max_recv_wr = 32,
        .max_send_sge = 16,
        .max_recv_sge = 16,
        .max_inline_data = 128,
    };
    int pd, cq, gsi, mr;
    uint32_t lkey, rkey;

    test_init_device(ctx);
    test_init_port_ipv6(ctx);

    /* allocate PD */
    pd = roce_alloc_pd(ctx, NULL);
    CU_ASSERT_TRUE(pd >= 0);

    /* allocate CQ */
    cq = roce_create_cq(ctx, 128, 0, NULL);
    CU_ASSERT_TRUE(cq >= 0);

    /* create GSI QP */
    gsi = roce_create_qp(ctx, pd, ROCE_QPT_GSI, -1, cq, cq, &cap, ROCE_QP_SIG_ALL, NULL);
    CU_ASSERT_TRUE(gsi >= 0);

    /* create DMA MR for GSI */
    mr = roce_create_mr(ctx, pd, ROCE_MR_DMA, 0, MR_SIZE, ROCE_ACCESS_LOCAL_WRITE, NULL, 0,
                        PAGE_SIZE, NULL);
    CU_ASSERT_TRUE(mr >= 0);

    /* get MR keys */
    CU_ASSERT_EQUAL(roce_get_mr_key(ctx, mr, &lkey, &rkey), 0);

    /* allocate and post recv WR */
    roce_recv_wr *recv_wr;
    CU_ASSERT_EQUAL(roce_alloc_recv_wr(ctx, 1, &recv_wr), 0);

    recv_wr->wr_id = 0;
    recv_wr->sge[0].addr = 4096;
    recv_wr->sge[0].length = 256;
    recv_wr->sge[0].lkey = lkey;

    CU_ASSERT_EQUAL(roce_post_recv(ctx, gsi, recv_wr), 0);

    /* cleanup */
    CU_ASSERT_EQUAL(roce_destroy_mr(ctx, mr), 0);
    CU_ASSERT_EQUAL(roce_destroy_qp(ctx, gsi), 0);
    CU_ASSERT_EQUAL(roce_destroy_cq(ctx, cq), 0);
    CU_ASSERT_EQUAL(roce_dealloc_pd(ctx, pd), 0);

    roce_free_ctx(ctx);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS) {
        return CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("GSI_QP_Test", NULL, NULL);
    if (!suite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite, test_create_and_destroy_gsi_ipv4);
    CU_ADD_TEST(suite, test_create_and_destroy_gsi_ipv6);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return failures;
}
