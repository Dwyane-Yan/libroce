/*
 * RoCE device implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "private/roce-private.h"
#include "private/mm.h"
#include "private/lock.h"
#include "private/log.h"
#include "private/util.h"

static inline void roce_destroy_all_resources(roce_ctx *ctx)
{
    for (int i = 0; i < ARRAY_SIZE(ctx->dev.resources); i++) {
        roce_resource_destroy(&ctx->dev.resources[i]);
    }
}

int roce_init_device(roce_ctx *ctx, roce_device_attr *para)
{
    roce_device *dev = &ctx->dev;

    if ((para->max_mr_size < ctx->para.page_size) || !roce_power_of_2(para->max_mr_size)) {
        roce_log_error(ctx, "invalid max-mr-size");
        return -EINVAL;
    }

    if (para->max_sge >= ROCE_MAX_SGE) {
        roce_log_error(ctx, "invalid max-sge");
        return -EINVAL;
    }

    if (para->max_inline_data >= ROCE_MAX_INLINE_DATA) {
        roce_log_error(ctx, "invalid max-inline-data");
        return -EINVAL;
    }
    // TODO check attr

    if (!para->max_pkeys || para->max_pkeys > ROCE_MAX_PKEY) {
        roce_log_error(ctx, "only %d PKEY(s) are supported", ROCE_MAX_PKEY);
        return -EINVAL;
    }

    if (!para->phys_port_cnt || para->phys_port_cnt > ROCE_MAX_PORTS) {
        roce_log_error(ctx, "only %d port(s) are supported", ROCE_MAX_PORTS);
        return -EINVAL;
    }

    struct roce_dev_res {
        const char *name;
        uint32_t size;
        uint32_t min;
        uint32_t max;
    } resources[] = {[ROCE_RESOURCE_PD] = {.name = "PD",
                                           .size = para->max_pd,
                                           .min = ROCE_MIN_GENERAL,
                                           .max = ROCE_MAX_GENERAL},
                     [ROCE_RESOURCE_CQ] = {.name = "CQ",
                                           .size = para->max_cq,
                                           .min = ROCE_MIN_GENERAL,
                                           .max = ROCE_MAX_GENERAL},
                     [ROCE_RESOURCE_MR] = {.name = "MR",
                                           .size = para->max_mr,
                                           .min = ROCE_MIN_GENERAL,
                                           .max = ROCE_MAX_MR},
                     [ROCE_RESOURCE_MW] = {.name = "MW",
                                           .size = para->max_mw,
                                           .min = 0,
                                           .max = 0 /* not support yet */},
                     [ROCE_RESOURCE_QP] = {.name = "QP",
                                           .size = para->max_qp,
                                           .min = 3, /* at least SMI, GSI, 1 QP */
                                           .max = ROCE_MAX_QP},
                     [ROCE_RESOURCE_SRQ] = {.name = "SRQ",
                                            .size = para->max_srq,
                                            .min = 0,
                                            .max = 0 /* not support yet */},
                     [ROCE_RESOURCE_AH] = {.name = "AH",
                                           .size = para->max_ah,
                                           .min = ROCE_MIN_GENERAL,
                                           .max = ROCE_MAX_GENERAL}};

    for (int i = 0; i < ARRAY_SIZE(resources); i++) {
        if ((resources[i].size < resources[i].min) || (resources[i].size > resources[i].max)) {
            roce_log_error(ctx, "failed to init resource %s, size %d should be [%d, %d]",
                           resources[i].name, resources[i].size, resources[i].min,
                           resources[i].max);
            goto free_res;
        }

        if (!resources[i].size) {
            roce_log_debug(ctx, "ignore resource %s", resources[i].name);
            continue;
        }

        roce_resource *resource = &ctx->dev.resources[i];
        const char *name = resources[i].name;
        uint32_t size = resources[i].size;
        if (roce_resource_init(ctx, resource, size, name)) {
            roce_log_error(ctx, "failed to init resource %s with size %d", name, size);
            goto free_res;
        }
    }

    roce_alloc_special_qps(ctx);
    roce_spin_init(ctx, &dev->lock);
    memcpy(&dev->attr, para, sizeof(dev->attr));
    dev->attr.device_cap_flags = ROCE_DEVICE_BAD_PKEY_CNTR | ROCE_DEVICE_BAD_QKEY_CNTR;

    return 0;

free_res:
    roce_destroy_all_resources(ctx);

    return -ENOMEM;
}

void roce_free_dev(roce_ctx *ctx)
{
    roce_destroy_special_qps(ctx);

    for (int i = 0; i < ARRAY_SIZE(ctx->dev.resources); i++) {
        roce_resource *resource = &ctx->dev.resources[i];

        if (!resource->size) {
            continue;
        }

        for (int j = 0; j < resource->size; j++) {
            if (resource->addr[j]) {
                const char *name = resource->name;
                roce_log_error(ctx, "resource %s has %d in use", name, j);
                // ROCE_ASSERT(0);
            }
        }

        roce_resource_destroy(&ctx->dev.resources[i]);
    }
}
