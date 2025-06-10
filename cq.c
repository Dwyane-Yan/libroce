/*
 * CQ implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <string.h>

#include "private/mm.h"
#include "private/roce-private.h"
#include "private/util.h"
#include "private/log.h"
#include "private/atomic.h"

int roce_create_cq(roce_ctx *ctx, int cqe, int comp_vector, void *cq_ctx)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_CQ];
    roce_cq *cq;
    int ret;

    if (cqe > ctx->dev.attr.max_cqe) {
        roce_log_error(ctx, "CQE exceeds(%d)", ctx->dev.attr.max_cqe);
        return -EINVAL;
    }

    cq = roce_calloc(ctx, 1, sizeof(roce_cq));
    if (!cq) {
        roce_log_error(ctx, "failed to allocate memory for CQ");
        return -ENOMEM;
    }

    ret = roce_resource_alloc(resource, &cq->res);
    if (ret < 0) {
        roce_log_error(ctx, "failed to allocate resource for CQ");
        goto free_cq;
    }

    cq->res.handle = ret;
    cq->cqe = cqe;
    cq->comp_vector = comp_vector;
    cq->cq_ctx = cq_ctx;

    return cq->res.handle;

free_cq:
    roce_free(ctx, cq);

    return ret;
}

static inline roce_cq *roce_get_cq_noref(roce_ctx *ctx, int cq_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_CQ];
    roce_res *res = roce_resource_get(resource, cq_handle);

    return container_of(res, roce_cq, res);
}

int roce_cq_ctx(roce_ctx *ctx, int cq_handle, void **cq_ctx)
{
    roce_cq *cq = roce_get_cq_noref(ctx, cq_handle);

    if (!cq) {
        return -EINVAL;
    }

    *cq_ctx = cq->cq_ctx;
    return 0;
}

roce_cq *roce_get_cq(roce_ctx *ctx, int cq_handle)
{
    roce_cq *cq = roce_get_cq_noref(ctx, cq_handle);

    if (!cq) {
        return NULL;
    }

    roce_atomic_inc(&cq->res.ref);
    return cq;
}

int roce_put_cq(roce_ctx *ctx, int cq_handle)
{
    roce_cq *cq = roce_get_cq_noref(ctx, cq_handle);

    if (!cq) {
        return -EINVAL;
    }

    roce_atomic_dec(&cq->res.ref);
    return 0;
}

int roce_destroy_cq(roce_ctx *ctx, int cq_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_CQ];
    roce_cq *cq = roce_get_cq_noref(ctx, cq_handle);
    int ret;

    if (!cq) {
        roce_log_error(ctx, "failed to destroy invalid CQ handle: %d", cq_handle);
        return -EINVAL;
    }

    if (roce_atomic_read(&cq->res.ref)) {
        roce_log_error(ctx, "CQ handle %d in use: Ref %d", cq->res.handle,
                       roce_atomic_read(&cq->res.ref));
        return -EBUSY;
    }

    ret = roce_resource_free(resource, cq_handle);
    if (ret) {
        roce_log_error(ctx, "failed to destroy CQ resource");
        return ret;
    }

    roce_free(ctx, cq);
    return 0;
}
