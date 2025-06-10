/*
 * AH implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "private/roce-private.h"
#include "private/mm.h"
#include "private/lock.h"
#include "private/log.h"

int roce_resource_init(roce_ctx *ctx, roce_resource *resource, uint32_t size, const char *name)
{
    int ret;

    if (!size || (size > ROCE_MAX_GENERAL)) {
        return -EINVAL;
    }

    ret = roce_spin_init(ctx, &resource->lock);
    if (ret) {
        return ret;
    }

    resource->ctx = ctx;
    resource->size = size;
    resource->addr = roce_calloc(ctx, size, sizeof(void *));
    snprintf(resource->name, sizeof(resource->name), "%s", name);
    if (!resource->addr) {
        return -ENOMEM;
    }

    return 0;
}

void roce_resource_destroy(roce_resource *resource)
{
    roce_ctx *ctx = resource->ctx;

    if (!resource->size) {
        return;
    }

    roce_free(ctx, resource->addr);
}

int roce_resource_alloc(roce_resource *resource, roce_res *res)
{
    roce_ctx *ctx = resource->ctx;
    int ret = -EBUSY;

    if (!res) {
        return -EINVAL;
    }

    roce_spin_lock(ctx, &resource->lock);
    for (uint32_t i = 0; i < resource->size; i++) {
        if (resource->addr[i]) {
            continue;
        }

        resource->addr[i] = res;
        ret = i;
        res->handle = i;
        break;
    }
    roce_spin_unlock(ctx, &resource->lock);

    return ret;
}

int roce_resource_alloc_at(roce_resource *resource, roce_res *res, uint32_t index)
{
    roce_ctx *ctx = resource->ctx;
    int ret = -EBUSY;

    if (!res) {
        return -EINVAL;
    }

    if (index >= resource->size) {
        return -EINVAL;
    }

    roce_spin_lock(ctx, &resource->lock);
    do {
        if (resource->addr[index]) {
            break;
        }

        resource->addr[index] = res;
        ret = index;
        res->handle = index;
    } while (0);
    roce_spin_unlock(ctx, &resource->lock);

    return ret;
}

roce_res *roce_resource_get(roce_resource *resource, uint32_t index)
{
    roce_ctx *ctx = resource->ctx;
    roce_res *res;

    if (index >= resource->size) {
        return NULL;
    }

    roce_spin_lock(ctx, &resource->lock);
    res = resource->addr[index];
    roce_spin_unlock(ctx, &resource->lock);
    if (res) {
        ROCE_ASSERT(res->handle == index);
    }

    return res;
}

int roce_resource_free(roce_resource *resource, uint32_t index)
{
    roce_ctx *ctx = resource->ctx;
    int ret = -EINVAL;

    if (index >= resource->size) {
        return -EINVAL;
    }

    roce_spin_lock(ctx, &resource->lock);
    do {
        if (!resource->addr[index]) {
            break;
        }

        resource->addr[index] = NULL;
        ret = 0;
    } while (0);
    roce_spin_unlock(ctx, &resource->lock);

    return ret;
}
