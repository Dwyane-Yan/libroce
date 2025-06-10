/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <assert.h>
#include <poll.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "roce/roce.h"

#include "private/pcap.h"
#include "private/util.h"

#define ROCE_MTU 1024
#define PAGE_SIZE 4096
#define ROCE_IMMDT 0x12345678
#define ROCE_MSG_SEGS 4
#define ROCE_MSG_DEPTH 4

#define ROCE_UD_QPKEY 0x12345678

#define ROCE_VNET_HDR_NONE 0
#define ROCE_VNET_HDR_16 16

#define ROCE_TEST_CLIENT 0
#define ROCE_TEST_SERVER 1

typedef struct test_mr {
    struct iovec iovecs[ROCE_MSG_SEGS];
    int mr;
    uint64_t iova;
    uint32_t lkey;
    uint32_t rkey;
} test_mr;

typedef struct test_ctx {
    uint32_t index;
    roce_qp_type qp_type;
    roce_mr_type mr_type;
    const char *proc;
    const char *capfile;
    int capfd;
    int netfd;
    uint8_t vnet_hdr;
    uint32_t rq_psn;
    uint32_t sq_psn;
    bool should_stop;
    test_mr send_mrs[ROCE_MSG_DEPTH];
    test_mr recv_mrs[ROCE_MSG_DEPTH];

    roce_ctx *roce_ctx;
    int pd;
    int send_cq;
    int recv_cq;
    int qp;
} test_ctx;

static test_ctx test_ctxs[2];
static int test_dma_unmaped;
static int test_netfd[2];

static void test_net_xmit(void *_ctx, uint16_t queue, struct iovec *iovs, uint32_t num_iov)
{
    test_ctx *ctx = (struct test_ctx *)_ctx;

    roce_pcap_write_frame(ctx->capfd, iovs, num_iov);
    assert(writev(ctx->netfd, iovs, num_iov) > 0);
}

static int test_cq_comp(void *ctx_opaque, roce_wc *wc, int cq, void *cq_ctx)
{
    test_ctx *ctx = ctx_opaque;
    test_ctx *remote_ctx = &test_ctxs[!ctx->index];
    int depth = wc->wr_id;

    if ((wc->opcode == ROCE_WC_RECV) && (wc->wc_flags & ROCE_WC_WITH_IMM)) {
        if (be32toh(wc->imm_data) != ROCE_IMMDT) {
            return -1;
        }
    }

    if (ctx->index == ROCE_TEST_SERVER) {
        if (depth == ROCE_MSG_DEPTH - 1) {
            ctx->should_stop = true;
            remote_ctx->should_stop = true;
        }
    }

    return 0;
}

static void *test_dma_map(void *ctx_opaque, uint64_t hwaddr, uint32_t len)
{
    __atomic_fetch_add(&test_dma_unmaped, 1, __ATOMIC_SEQ_CST);
    return (void *)(hwaddr + 0x100000000L);
}

static void test_dma_unmap(void *ctx_opaque, void *addr, uint32_t len)
{
    __atomic_fetch_sub(&test_dma_unmaped, 1, __ATOMIC_SEQ_CST);
}

static void test_log_stdout(void *ctx_opaque, char *msg)
{
}

static void test_init_dev(roce_ctx *ctx)
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

static void test_init_port(test_ctx *ctx)
{
    roce_port_attr attr = {.max_mtu = ROCE_MTU, .active_mtu = ROCE_MTU};
    uint8_t mac[ROCE_MAC_LEN] = {0xfe, 0xe1, 0xc0, 0x01, 0x00, ctx->index};
    uint8_t gid[ROCE_GID_LEN] = {0, 0, 0,    0,    0,   0,   0,   0,
                                 0, 0, 0xff, 0xff, 192, 168, 122, ctx->index + 100};
    int ret;

    ret = roce_init_port(ctx->roce_ctx, 1, &attr);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_set_port_mac(ctx->roce_ctx, 1, mac);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_set_port_vnet(ctx->roce_ctx, 1, ctx->vnet_hdr);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_add_gid(ctx->roce_ctx, 1, 0, gid);
    CU_ASSERT_EQUAL(ret, 0);
}

static inline uint64_t test_to_iova(roce_mr_type mrtype, void *p)
{
    uint64_t iova = (uint64_t)p;

    switch (mrtype) {
    case ROCE_MR_MEM:
    case ROCE_MR_FRMR:
        return iova & 0xffffffff;
    case ROCE_MR_DMA:
        return iova - 0x100000000L;
    default:
        CU_FAIL("Wrong MR type");
        return 0;
    }
}

static void test_alloc_mr(test_ctx *ctx, test_mr *mr, bool fmtstr)
{
    struct iovec *iov;
    uint32_t length = PAGE_SIZE * ROCE_MSG_SEGS;
    uint32_t access;
    char *buf = NULL;
    int ret;

    for (int i = 0; i < ROCE_MSG_SEGS; i++) {
        ret = posix_memalign((void **)&buf, PAGE_SIZE, PAGE_SIZE);
        CU_ASSERT_EQUAL(ret, 0);

        memset(buf, 0x00, PAGE_SIZE);
        iov = &mr->iovecs[i];
        iov->iov_base = buf;
        iov->iov_len = PAGE_SIZE;

        if (fmtstr) {
            for (int j = 0; j < PAGE_SIZE; j += 32) {
                snprintf(buf + j, 32, "QPN%03d:IOV%02d:OFF%04d", ctx->qp, i, j);
            }
        }
    }

    access = ROCE_ACCESS_LOCAL_WRITE | ROCE_ACCESS_REMOTE_WRITE | ROCE_ACCESS_REMOTE_READ;

    mr->iova = test_to_iova(ctx->mr_type, mr->iovecs[0].iov_base);
    mr->mr = roce_create_mr(ctx->roce_ctx, ctx->pd, ctx->mr_type, mr->iova, length, access,
                            mr->iovecs, ROCE_MSG_SEGS, PAGE_SIZE, NULL);
    CU_ASSERT_TRUE(mr->mr >= 0);

    ret = roce_get_mr_key(ctx->roce_ctx, mr->mr, &mr->lkey, &mr->rkey);
    CU_ASSERT_EQUAL(ret, 0);
}

static void test_alloc_mrs(test_ctx *ctx)
{
    for (int i = 0; i < ROCE_MSG_DEPTH; i++) {
        test_alloc_mr(ctx, &ctx->send_mrs[i], true);
    }

    for (int i = 0; i < ROCE_MSG_DEPTH; i++) {
        test_alloc_mr(ctx, &ctx->recv_mrs[i], false);
    }
}

static void test_destroy_mrs(test_ctx *ctx)
{
    struct iovec *iov;
    int ret;

    for (int depth = 0; depth < ROCE_MSG_DEPTH; depth++) {
        ret = roce_destroy_mr(ctx->roce_ctx, ctx->send_mrs[depth].mr);
        CU_ASSERT_EQUAL(ret, 0);

        for (int segs = 0; segs < ROCE_MSG_SEGS; segs++) {
            iov = &ctx->send_mrs[depth].iovecs[segs];
            free(iov->iov_base);
        }
    }

    for (int depth = 0; depth < ROCE_MSG_DEPTH; depth++) {
        ret = roce_destroy_mr(ctx->roce_ctx, ctx->recv_mrs[depth].mr);
        CU_ASSERT_EQUAL(ret, 0);

        for (int segs = 0; segs < ROCE_MSG_SEGS; segs++) {
            iov = &ctx->recv_mrs[depth].iovecs[segs];
            free(iov->iov_base);
        }
    }
}

static void test_setup_qp(test_ctx *ctx)
{
    roce_qp_cap qp_cap = {.max_send_wr = 32,
                          .max_recv_wr = 32,
                          .max_send_sge = 16,
                          .max_recv_sge = 16,
                          .max_inline_data = 128};
    int ret;

    ctx->pd = roce_alloc_pd(ctx->roce_ctx, NULL);
    CU_ASSERT_TRUE(ctx->pd >= 0);

    ctx->send_cq = roce_create_cq(ctx->roce_ctx, 128, 0, ctx);
    CU_ASSERT_TRUE(ctx->send_cq >= 0);

    ctx->recv_cq = roce_create_cq(ctx->roce_ctx, 128, 0, ctx);
    CU_ASSERT_TRUE(ctx->recv_cq >= 0);

    ret = roce_create_qp(ctx->roce_ctx, ctx->pd, ctx->qp_type, -1, ctx->send_cq, ctx->recv_cq,
                         &qp_cap, ROCE_QP_SIG_ALL, NULL);
    CU_ASSERT_TRUE(ret >= 0);
    ctx->qp = ret;
}

static void test_modify_qp(test_ctx *ctx)
{
    test_ctx *remote_ctx = &test_ctxs[!ctx->index];
    roce_qp_attr qp_attr;
    int ret;

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state = ROCE_QPS_INIT;
    qp_attr.pkey_index = 0;
    qp_attr.port_num = 1;
    qp_attr.qp_access_flags = 0;
    ret = roce_modify_qp(ctx->roce_ctx, ctx->qp, &qp_attr,
                         ROCE_QP_STATE | ROCE_QP_PKEY_INDEX | ROCE_QP_PORT | ROCE_QP_ACCESS_FLAGS);
    CU_ASSERT_EQUAL(ret, 0);

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state = ROCE_QPS_RTR;
    qp_attr.path_mtu = ROCE_MTU;
    qp_attr.dest_qp_num = remote_ctx->qp;
    qp_attr.rq_psn = ctx->rq_psn;
    qp_attr.sq_psn = ctx->sq_psn;
    uint8_t dgid[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 192, 168, 122, !(ctx->index) + 100};
    memcpy(qp_attr.ah_attr.gid, dgid, ROCE_GID_LEN);
    uint8_t dmac[ROCE_MAC_LEN] = {0xfe, 0xe1, 0xc0, 0x01, 0x00, !(ctx->index)};
    memcpy(qp_attr.ah_attr.mac, dmac, ROCE_MAC_LEN);
    qp_attr.ah_attr.sgid_index = 0;
    qp_attr.ah_attr.port_num = 1;
    qp_attr.ah_attr.hop_limit = 32;
    ret = roce_modify_qp(ctx->roce_ctx, ctx->qp, &qp_attr,
                         ROCE_QP_STATE | ROCE_QP_DEST_QPN | ROCE_QP_PATH_MTU | ROCE_QP_RQ_PSN |
                             ROCE_QP_SQ_PSN | ROCE_QP_AV);
    CU_ASSERT_EQUAL(ret, 0);

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state = ROCE_QPS_RTS;
    ret = roce_modify_qp(ctx->roce_ctx, ctx->qp, &qp_attr, ROCE_QP_STATE);
    CU_ASSERT_EQUAL(ret, 0);
}

static void test_free_qp(test_ctx *ctx)
{
    int ret;

    ret = roce_destroy_qp(ctx->roce_ctx, ctx->qp);
    CU_ASSERT_EQUAL(ret, 0);

    test_destroy_mrs(ctx);

    ret = roce_destroy_cq(ctx->roce_ctx, ctx->send_cq);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_destroy_cq(ctx->roce_ctx, ctx->recv_cq);
    CU_ASSERT_EQUAL(ret, 0);

    ret = roce_dealloc_pd(ctx->roce_ctx, ctx->pd);
    CU_ASSERT_EQUAL(ret, 0);
}

static roce_ctx *test_new_roce(test_ctx *ctx)
{
    roce_ctx_para para = {.version = ROCE_V2,
                          .ctx_opaque = ctx,
                          .page_size = PAGE_SIZE,
                          .log_level = roce_log_warn,
                          .log = test_log_stdout,
                          .net_xmit = test_net_xmit,
                          .cq_comp = test_cq_comp,
                          .dma_map = test_dma_map,
                          .dma_unmap = test_dma_unmap,
                          .spin = {.lock_size = sizeof(pthread_spinlock_t),
                                   .init = (int (*)(void *, int))pthread_spin_init,
                                   .lock = (int (*)(void *))pthread_spin_lock,
                                   .trylock = (int (*)(void *))pthread_spin_trylock,
                                   .unlock = (int (*)(void *))pthread_spin_unlock}};

    roce_ctx *rctx = roce_new_ctx(&para);
    CU_ASSERT_PTR_NOT_NULL(rctx);
    return rctx;
}

static void test_recv_qp(test_ctx *ctx)
{
    roce_recv_wr *recv_wr;
    roce_sge *sge;
    test_mr *mr;
    int ret;

    for (int depth = 0; depth < ROCE_MSG_DEPTH; depth++) {
        ret = roce_alloc_recv_wr(ctx->roce_ctx, ROCE_MSG_SEGS, &recv_wr);
        CU_ASSERT_EQUAL(ret, 0);

        mr = &ctx->recv_mrs[depth];
        recv_wr->wr_id = (uint64_t)depth;

        for (int i = 0; i < recv_wr->num_sge; i++) {
            sge = &recv_wr->sge[i];
            sge->addr = test_to_iova(ctx->mr_type, mr->iovecs[0].iov_base);
            sge->length = PAGE_SIZE;
            sge->lkey = mr->lkey;
        }

        ret = roce_post_recv(ctx->roce_ctx, ctx->qp, recv_wr);
        CU_ASSERT_EQUAL(ret, 0);
    }
}

static void test_new_ctx(test_ctx *ctx)
{
    ctx->roce_ctx = test_new_roce(ctx);
    test_init_dev(ctx->roce_ctx);
    test_init_port(ctx);
    test_setup_qp(ctx);
}

static void test_start_ctx(test_ctx *ctx)
{
    test_modify_qp(ctx);
    test_alloc_mrs(ctx);
    test_recv_qp(ctx);
}

static void test_recv_net(test_ctx *ctx)
{
    ctx->should_stop = false;

    while (!ctx->should_stop) {
        uint8_t buf[2048];
        struct pollfd pfd = {.fd = ctx->netfd, .events = POLLIN};
        if (poll(&pfd, 1, 10) < 1) {
            continue;
        }

        int len = read(ctx->netfd, buf, sizeof(buf));
        assert(len > 0);

        struct iovec iov = {.iov_base = buf, .iov_len = len};
        int ret = roce_net_recv(ctx->roce_ctx, 0, &iov, 1);
        assert(ret >= 0);
    }
}

static int test_create_ah(test_ctx *ctx)
{
    roce_ah_attr ah_attr;

    if ((ctx->qp_type != ROCE_QPT_UD) && (ctx->qp_type != ROCE_QPT_GSI)) {
        return -1;
    }

    memset(&ah_attr, 0, sizeof(ah_attr));
    uint8_t dgid[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 192, 168, 122, !(ctx->index) + 100};
    memcpy(ah_attr.gid, dgid, ROCE_GID_LEN);
    uint8_t dmac[ROCE_MAC_LEN] = {0xfe, 0xe1, 0xc0, 0x01, 0x00, !(ctx->index)};
    memcpy(ah_attr.mac, dmac, ROCE_MAC_LEN);
    ah_attr.sgid_index = 0;
    ah_attr.port_num = 1;
    ah_attr.hop_limit = 32;

    int ah = roce_create_ah(ctx->roce_ctx, ctx->pd, &ah_attr, NULL);
    CU_ASSERT_TRUE(ah >= 0);
    return ah;
}

static void test_send_qp(test_ctx *ctx, uint32_t size)
{
    test_ctx *remote_ctx = &test_ctxs[!ctx->index];
    roce_send_wr *send_wr;
    roce_sge *sge;
    int ah = test_create_ah(ctx);
    test_mr *mr;
    int ret;

    for (int depth = 0; depth < ROCE_MSG_DEPTH; depth++) {
        ret = roce_alloc_send_wr(ctx->roce_ctx, ROCE_MSG_SEGS, &send_wr);
        CU_ASSERT_EQUAL(ret, 0);

        mr = &ctx->send_mrs[depth];
        send_wr->wr_id = (uint64_t)depth;
        send_wr->opcode = ROCE_WR_SEND_WITH_IMM;
        send_wr->send_flags = ROCE_SEND_SIGNALED;
        send_wr->imm_data = htobe32(ROCE_IMMDT);

        for (int i = 0; i < send_wr->num_sge; i++) {
            sge = &send_wr->sge[i];
            if (ctx->mr_type == ROCE_MR_DMA) {
                sge->addr = test_to_iova(ctx->mr_type, mr->iovecs[i].iov_base);
            } else {
                sge->addr = test_to_iova(ctx->mr_type, mr->iovecs[0].iov_base) + i * PAGE_SIZE;
            }
            sge->length = size;
            sge->lkey = mr->lkey;
        }

        if ((ctx->qp_type == ROCE_QPT_UD) || (ctx->qp_type == ROCE_QPT_GSI)) {
            send_wr->wr.ud.remote_qpn = remote_ctx->qp;
            send_wr->wr.ud.remote_qkey = ROCE_UD_QPKEY;
            send_wr->wr.ud.ah = ah;
        }

        ret = roce_post_send(ctx->roce_ctx, ctx->qp, send_wr);
        CU_ASSERT_EQUAL(ret, 0);
    }

    if (ah >= 0) {
        ret = roce_destroy_ah(ctx->roce_ctx, ah);
        CU_ASSERT_EQUAL(ret, 0);
    }
}

static void test_rc_write_qp(test_ctx *ctx)
{
    test_ctx *remote_ctx = &test_ctxs[!ctx->index];
    test_mr *remote_recv_mr;
    test_mr *mr;
    roce_send_wr *send_wr;
    roce_sge *sge;
    int ret;

    for (int depth = 0; depth < ROCE_MSG_DEPTH; depth++) {
        ret = roce_alloc_send_wr(ctx->roce_ctx, ROCE_MSG_SEGS, &send_wr);
        CU_ASSERT_EQUAL(ret, 0);

        remote_recv_mr = &remote_ctx->recv_mrs[depth];
        mr = &ctx->send_mrs[depth];
        send_wr->wr_id = (uint64_t)depth;
        send_wr->opcode = ROCE_WR_RDMA_WRITE_WITH_IMM;
        send_wr->send_flags = ROCE_SEND_SIGNALED;
        send_wr->imm_data = htobe32(ROCE_IMMDT);
        send_wr->wr.rdma.remote_addr = remote_recv_mr->iova;
        send_wr->wr.rdma.rkey = remote_recv_mr->rkey;

        for (int i = 0; i < send_wr->num_sge; i++) {
            sge = &send_wr->sge[i];
            sge->addr = mr->iova + i * PAGE_SIZE;
            sge->length = PAGE_SIZE;
            sge->lkey = mr->lkey;
        }

        ret = roce_post_send(ctx->roce_ctx, ctx->qp, send_wr);
        CU_ASSERT_EQUAL(ret, 0);
    }
}

static void *test_rc_client_send(void *arg)
{
    test_ctx *ctx = arg;

    test_send_qp(ctx, PAGE_SIZE);
    test_recv_net(ctx);
    test_free_qp(ctx);
    roce_free_ctx(ctx->roce_ctx);

    return NULL;
}

static void *test_client_write(void *arg)
{
    test_ctx *ctx = arg;

    test_rc_write_qp(ctx);
    test_recv_net(ctx);
    test_free_qp(ctx);
    roce_free_ctx(ctx->roce_ctx);

    return NULL;
}

static void *test_ud_client_send(void *arg)
{
    test_ctx *ctx = arg;

    test_send_qp(ctx, 128);
    test_recv_net(ctx);
    test_free_qp(ctx);
    roce_free_ctx(ctx->roce_ctx);

    return NULL;
}

static void *test_server(void *arg)
{
    test_ctx *ctx = arg;

    test_recv_net(ctx);
    test_free_qp(ctx);
    roce_free_ctx(ctx->roce_ctx);

    return NULL;
}

static void test_capfile(test_ctx *ctx)
{
    unlink(ctx->capfile);
    ctx->capfd = roce_pcap_create(ctx->capfile);
    assert(ctx->capfd >= 0);
}

static void test_ctxs_init(uint8_t vnet_hdr)
{
    memset(test_ctxs, 0, sizeof(test_ctxs));

    test_ctxs[ROCE_TEST_SERVER].index = ROCE_TEST_SERVER;
    test_ctxs[ROCE_TEST_SERVER].qp_type = ROCE_QPT_RC;
    test_ctxs[ROCE_TEST_SERVER].mr_type = ROCE_MR_MEM;
    test_ctxs[ROCE_TEST_SERVER].proc = "echo-server";
    test_ctxs[ROCE_TEST_SERVER].capfile = "echo-server.cap";
    test_ctxs[ROCE_TEST_SERVER].vnet_hdr = vnet_hdr;
    test_ctxs[ROCE_TEST_SERVER].rq_psn = 0x5678;
    test_ctxs[ROCE_TEST_SERVER].sq_psn = 0x1234;

    test_ctxs[ROCE_TEST_CLIENT].index = ROCE_TEST_CLIENT;
    test_ctxs[ROCE_TEST_CLIENT].qp_type = ROCE_QPT_RC;
    test_ctxs[ROCE_TEST_CLIENT].mr_type = ROCE_MR_MEM;
    test_ctxs[ROCE_TEST_CLIENT].proc = "echo-client";
    test_ctxs[ROCE_TEST_CLIENT].capfile = "echo-client.cap";
    test_ctxs[ROCE_TEST_CLIENT].vnet_hdr = vnet_hdr;
    test_ctxs[ROCE_TEST_CLIENT].rq_psn = 0x1234;
    test_ctxs[ROCE_TEST_CLIENT].sq_psn = 0x5678;
}

static void run_test(test_ctx *c_ctx, test_ctx *s_ctx, void *(*client_func)(void *))
{
    pthread_t threads[2];

    test_capfile(s_ctx);
    test_capfile(c_ctx);

    test_new_ctx(s_ctx);
    test_new_ctx(c_ctx);
    test_start_ctx(s_ctx);
    test_start_ctx(c_ctx);

    pthread_create(&threads[ROCE_TEST_SERVER], NULL, test_server, s_ctx);
    usleep(10);
    pthread_create(&threads[ROCE_TEST_CLIENT], NULL, client_func, c_ctx);

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    close(s_ctx->capfd);
    close(c_ctx->capfd);
}

static void test_rc_send_vnet_none(void)
{
    test_ctxs_init(ROCE_VNET_HDR_NONE);

    test_ctxs[ROCE_TEST_SERVER].netfd = test_netfd[ROCE_TEST_SERVER];
    test_ctxs[ROCE_TEST_CLIENT].netfd = test_netfd[ROCE_TEST_CLIENT];

    run_test(&test_ctxs[ROCE_TEST_CLIENT], &test_ctxs[ROCE_TEST_SERVER], test_rc_client_send);
}

static void test_rc_send_vnet_16(void)
{
    test_ctxs_init(ROCE_VNET_HDR_16);

    test_ctxs[ROCE_TEST_SERVER].netfd = test_netfd[ROCE_TEST_SERVER];
    test_ctxs[ROCE_TEST_CLIENT].netfd = test_netfd[ROCE_TEST_CLIENT];

    run_test(&test_ctxs[ROCE_TEST_CLIENT], &test_ctxs[ROCE_TEST_SERVER], test_rc_client_send);
}

static void test_rc_write_vnet_none(void)
{
    test_ctxs_init(ROCE_VNET_HDR_NONE);

    test_ctxs[ROCE_TEST_SERVER].netfd = test_netfd[ROCE_TEST_SERVER];
    test_ctxs[ROCE_TEST_CLIENT].netfd = test_netfd[ROCE_TEST_CLIENT];

    run_test(&test_ctxs[ROCE_TEST_CLIENT], &test_ctxs[ROCE_TEST_SERVER], test_client_write);
}

static void test_rc_write_vnet_16(void)
{
    test_ctxs_init(ROCE_VNET_HDR_16);

    test_ctxs[ROCE_TEST_SERVER].netfd = test_netfd[ROCE_TEST_SERVER];
    test_ctxs[ROCE_TEST_CLIENT].netfd = test_netfd[ROCE_TEST_CLIENT];

    run_test(&test_ctxs[ROCE_TEST_CLIENT], &test_ctxs[ROCE_TEST_SERVER], test_client_write);
}

static void test_ud_send_vnet_none(void)
{
    test_ctxs_init(ROCE_VNET_HDR_NONE);

    test_ctxs[ROCE_TEST_SERVER].qp_type = ROCE_QPT_UD;
    test_ctxs[ROCE_TEST_CLIENT].qp_type = ROCE_QPT_UD;

    test_ctxs[ROCE_TEST_SERVER].netfd = test_netfd[ROCE_TEST_SERVER];
    test_ctxs[ROCE_TEST_CLIENT].netfd = test_netfd[ROCE_TEST_CLIENT];

    run_test(&test_ctxs[ROCE_TEST_CLIENT], &test_ctxs[ROCE_TEST_SERVER], test_ud_client_send);
}

static void test_ud_send_vnet_16(void)
{
    test_ctxs_init(ROCE_VNET_HDR_16);

    test_ctxs[ROCE_TEST_SERVER].qp_type = ROCE_QPT_UD;
    test_ctxs[ROCE_TEST_CLIENT].qp_type = ROCE_QPT_UD;

    test_ctxs[ROCE_TEST_SERVER].netfd = test_netfd[ROCE_TEST_SERVER];
    test_ctxs[ROCE_TEST_CLIENT].netfd = test_netfd[ROCE_TEST_CLIENT];

    run_test(&test_ctxs[ROCE_TEST_CLIENT], &test_ctxs[ROCE_TEST_SERVER], test_ud_client_send);
}

static void test_ud_send_dma_vnet_none(void)
{
    test_ctxs_init(ROCE_VNET_HDR_NONE);

    test_ctxs[ROCE_TEST_SERVER].qp_type = ROCE_QPT_GSI;
    test_ctxs[ROCE_TEST_SERVER].mr_type = ROCE_MR_DMA;
    test_ctxs[ROCE_TEST_CLIENT].qp_type = ROCE_QPT_GSI;
    test_ctxs[ROCE_TEST_CLIENT].mr_type = ROCE_MR_DMA;

    test_ctxs[ROCE_TEST_SERVER].netfd = test_netfd[ROCE_TEST_SERVER];
    test_ctxs[ROCE_TEST_CLIENT].netfd = test_netfd[ROCE_TEST_CLIENT];

    run_test(&test_ctxs[ROCE_TEST_CLIENT], &test_ctxs[ROCE_TEST_SERVER], test_ud_client_send);
}

static void test_ud_send_dma_vnet_16(void)
{
    test_ctxs_init(ROCE_VNET_HDR_16);

    test_ctxs[ROCE_TEST_SERVER].qp_type = ROCE_QPT_GSI;
    test_ctxs[ROCE_TEST_SERVER].mr_type = ROCE_MR_DMA;
    test_ctxs[ROCE_TEST_CLIENT].qp_type = ROCE_QPT_GSI;
    test_ctxs[ROCE_TEST_CLIENT].mr_type = ROCE_MR_DMA;

    test_ctxs[ROCE_TEST_SERVER].netfd = test_netfd[ROCE_TEST_SERVER];
    test_ctxs[ROCE_TEST_CLIENT].netfd = test_netfd[ROCE_TEST_CLIENT];

    run_test(&test_ctxs[ROCE_TEST_CLIENT], &test_ctxs[ROCE_TEST_SERVER], test_ud_client_send);
}

static int setup_suite(void)
{
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, test_netfd)) {
        return -1;
    }

    fcntl(test_netfd[0], F_SETFL, fcntl(test_netfd[0], F_GETFL) | O_NONBLOCK);
    fcntl(test_netfd[1], F_SETFL, fcntl(test_netfd[1], F_GETFL) | O_NONBLOCK);

    test_dma_unmaped = 0;

    return 0;
}

static int teardown_suite(void)
{
    if (test_dma_unmaped != 0) {
        return -1;
    }

    close(test_netfd[0]);
    close(test_netfd[1]);

    return 0;
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS) {
        return CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("QP_Integration_Test", setup_suite, teardown_suite);
    if (!suite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite, test_rc_send_vnet_none);
    CU_ADD_TEST(suite, test_rc_send_vnet_16);
    CU_ADD_TEST(suite, test_rc_write_vnet_none);
    CU_ADD_TEST(suite, test_rc_write_vnet_16);
    CU_ADD_TEST(suite, test_ud_send_vnet_none);
    CU_ADD_TEST(suite, test_ud_send_vnet_16);
    CU_ADD_TEST(suite, test_ud_send_dma_vnet_none);
    CU_ADD_TEST(suite, test_ud_send_dma_vnet_16);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return failures;
}
