/*
 * AH implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "private/mm.h"
#include "private/roce-private.h"
#include "private/util.h"
#include "private/log.h"
#include "private/atomic.h"

int roce_create_ah(roce_ctx *ctx, int pd_handle, roce_ah_attr *attr, void *ah_ctx)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_AH];
    roce_pd *pd;
    roce_ah *ah;
    int ret;

    pd = roce_get_pd(ctx, pd_handle);
    if (!pd) {
        roce_log_error(ctx, "invalid PD handle for new AH");
        return -EINVAL;
    }

    ah = roce_calloc(ctx, 1, sizeof(roce_ah));
    if (!ah) {
        roce_log_error(ctx, "failed to allocate memory for AH");
        ret = -ENOMEM;
        goto put_pd;
    }

    ret = roce_resource_alloc(resource, &ah->res);
    if (ret < 0) {
        roce_log_error(ctx, "failed to allocate resource for AH");
        goto free_ah;
    }

    ah->res.handle = ret;
    ah->pd = pd;
    ah->attr = *attr;
    ah->ah_ctx = ah_ctx;

    return ah->res.handle;

free_ah:
    roce_free(ctx, ah);

put_pd:
    ROCE_ASSERT(!roce_put_pd(ctx, pd_handle));

    return ret;
}

static inline roce_ah *roce_get_ah_noref(roce_ctx *ctx, int ah_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_AH];
    roce_res *res = roce_resource_get(resource, ah_handle);

    return container_of(res, roce_ah, res);
}

roce_ah *roce_get_ah(roce_ctx *ctx, int ah_handle)
{
    roce_ah *ah = roce_get_ah_noref(ctx, ah_handle);

    if (!ah) {
        return NULL;
    }

    roce_atomic_inc(&ah->res.ref);
    return ah;
}

int roce_put_ah(roce_ctx *ctx, int ah_handle)
{
    roce_ah *ah = roce_get_ah_noref(ctx, ah_handle);

    if (!ah) {
        return -EINVAL;
        ;
    }

    roce_atomic_dec(&ah->res.ref);
    return 0;
}

int roce_destroy_ah(roce_ctx *ctx, int ah_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_AH];
    roce_ah *ah = roce_get_ah_noref(ctx, ah_handle);
    int ret;

    if (!ah) {
        roce_log_error(ctx, "failed to destroy invalid AH handle: %d", ah_handle);
        return -EINVAL;
    }

    if (roce_atomic_read(&ah->res.ref)) {
        roce_log_error(ctx, "AH handle %d in use: Ref %d", ah->res.handle,
                       roce_atomic_read(&ah->res.ref));
        return -EBUSY;
    }

    ret = roce_resource_free(resource, ah_handle);
    if (ret) {
        roce_log_error(ctx, "failed to destroy AH resource");
        return ret;
    }

    ROCE_ASSERT(!roce_put_pd(ctx, ah->pd->res.handle));
    roce_free(ctx, ah);
    return 0;
}
