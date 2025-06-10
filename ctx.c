/*
 * roce context management.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "spec/ib-spec.h"

#include "private/mm.h"
#include "private/roce-private.h"
#include "private/util.h"

static inline void roce_spec_verify_on_compiling(void)
{
    /* IPv4 */
    ROCE_BUILD_BUG_ON(sizeof(ip_hdr) != IP_HDR_SIZE);

    /* IPv6 */
    ROCE_BUILD_BUG_ON(sizeof(ipv6_hdr) != IPV6_HDR_SIZE);

    /* IB */
    ROCE_BUILD_BUG_ON(sizeof(ib_bth) != IB_BTH_SIZE);
    ROCE_BUILD_BUG_ON(sizeof(ib_rdeth) != IB_RDETH_SIZE);
    ROCE_BUILD_BUG_ON(sizeof(ib_deth) != IB_DETH_SIZE);
    ROCE_BUILD_BUG_ON(sizeof(ib_reth) != IB_RETH_SIZE);
    ROCE_BUILD_BUG_ON(sizeof(ib_atomiceth) != IB_ATOMICETH_SIZE);
    ROCE_BUILD_BUG_ON(sizeof(ib_aeth) != IB_AETH_SIZE);
    ROCE_BUILD_BUG_ON(sizeof(ib_atomicacketh) != IB_ATOMICACKETH_SIZE);
    ROCE_BUILD_BUG_ON(sizeof(ib_immdt) != IB_IMMDT_SIZE);
    ROCE_BUILD_BUG_ON(sizeof(ib_ieth) != IB_IETH_SIZE);
}

/* This should be used only before creating roce context */
#define roce_early_log(para, fmt, ...)                                                             \
    if (para->log) {                                                                               \
        char log[256] = {0};                                                                       \
        snprintf(log, sizeof(log), fmt, ##__VA_ARGS__);                                            \
        para->log(para->ctx_opaque, log);                                                          \
    }

roce_ctx *roce_new_ctx(roce_ctx_para *para)
{
    roce_ctx *ctx;
    int n;

    roce_spec_verify_on_compiling();

    /* multiple contexts overwrite seed, but it's OK */
    srandom(time(NULL) * gettid());

    if (para->version != ROCE_V2) {
        roce_early_log(para, "Only RoCE v2 is supported");
        return NULL;
    }

    if (!roce_power_of_2(para->page_size)) {
        roce_early_log(para, "Page size is not power of 2");
        return NULL;
    }

    if (!para->net_xmit) {
        roce_early_log(para, "Missing *net_xmit*");
        return NULL;
    }

    if (!para->cq_comp) {
        roce_early_log(para, "Missing *cq_comp*");
        return NULL;
    }

    if (!para->dma_map || !para->dma_unmap) {
        roce_early_log(para, "Missing *dma_map* or *dma_unmap*");
        return NULL;
    }

    /* mm functions must set together */
    n = !!para->malloc + !!para->free + !!para->calloc + !!para->realloc;
    if (n && (n != 4)) {
        roce_early_log(para, "Malloc family must be set together");
        return NULL;
    }

    /* lock functions must set together */
    n = !!para->spin.lock_size + !!para->spin.init + !!para->spin.lock + !!para->spin.trylock +
        !!para->spin.unlock;
    if (n && (n != 5)) {
        roce_early_log(para, "Spin family must be set together");
        return NULL;
    }

    if (para->calloc) {
        ctx = para->calloc(1, sizeof(roce_ctx));
    } else {
        ctx = calloc(1, sizeof(roce_ctx));
    }

    if (!ctx) {
        roce_early_log(para, "No enough memory for roce context");
        return NULL;
    }

    ctx->para = *para;
    /* use libc malloc by default */
    if (!ctx->para.malloc) {
        ctx->para.malloc = malloc;
        ctx->para.free = free;
        ctx->para.calloc = calloc;
        ctx->para.realloc = realloc;
    }

    if (!ctx->para.spin.lock_size) {
        ctx->para.spin.lock_size = sizeof(pthread_spinlock_t);
        ctx->para.spin.init = (int (*)(void *, int))pthread_spin_init;
        ctx->para.spin.lock = (int (*)(void *))pthread_spin_trylock;
        ctx->para.spin.trylock = (int (*)(void *))pthread_spin_trylock;
        ctx->para.spin.unlock = (int (*)(void *))pthread_spin_unlock;
    }

    return ctx;
}

void roce_free_ctx(roce_ctx *ctx)
{
    roce_free_dev(ctx);
    roce_free(ctx, ctx);
}
