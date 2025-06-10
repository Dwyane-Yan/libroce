/*
 * PD implementation.
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

int roce_alloc_pd(roce_ctx *ctx, void *pd_ctx)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_PD];
    roce_pd *pd;
    int ret;

    pd = roce_calloc(ctx, 1, sizeof(roce_pd));
    if (!pd) {
        roce_log_error(ctx, "failed to allocate memory for PD");
        return -ENOMEM;
    }

    ret = roce_resource_alloc(resource, &pd->res);
    if (ret < 0) {
        roce_log_error(ctx, "failed to allocate resource for PD");
        goto free_pd;
    }

    pd->res.handle = ret;
    pd->pd_ctx = pd_ctx;

    return pd->res.handle;

free_pd:
    roce_free(ctx, pd);

    return ret;
}

static inline roce_pd *roce_get_pd_noref(roce_ctx *ctx, int pd_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_PD];
    roce_res *res = roce_resource_get(resource, pd_handle);

    if (!res) {
        return NULL;
    }

    return container_of(res, roce_pd, res);
}

roce_pd *roce_get_pd(roce_ctx *ctx, int pd_handle)
{
    roce_pd *pd = roce_get_pd_noref(ctx, pd_handle);

    if (!pd) {
        return NULL;
    }

    roce_atomic_inc(&pd->res.ref);
    return pd;
}

int roce_put_pd(roce_ctx *ctx, int pd_handle)
{
    roce_pd *pd = roce_get_pd_noref(ctx, pd_handle);

    if (!pd) {
        return -EINVAL;
    }

    roce_atomic_dec(&pd->res.ref);
    return 0;
}

int roce_dealloc_pd(roce_ctx *ctx, int pd_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_PD];
    roce_pd *pd = roce_get_pd_noref(ctx, pd_handle);
    int ret;

    if (!pd) {
        roce_log_error(ctx, "failed to destroy invalid PD handle: %d", pd_handle);
        return -EINVAL;
    }

    if (roce_atomic_read(&pd->res.ref)) {
        roce_log_error(ctx, "PD handle %d in use: Ref %d", pd->res.handle,
                       roce_atomic_read(&pd->res.ref));
        return -EBUSY;
    }

    ret = roce_resource_free(resource, pd_handle);
    if (ret) {
        roce_log_error(ctx, "failed to destroy PD resource");
        return ret;
    }

    roce_free(ctx, pd);
    return 0;
}
