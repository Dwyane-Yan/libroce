/*
 * Resource management.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_RESOURCE_H
#define LIBROCE_RESOURCE_H

#include "private/types.h"

/* roce_res MUST be the first field of any resource */
typedef struct roce_res {
    uint32_t handle;
    uint32_t ref;
} roce_res;

typedef enum roce_resource_type {
    ROCE_RESOURCE_PD,
    ROCE_RESOURCE_CQ,
    ROCE_RESOURCE_MR,
    ROCE_RESOURCE_MW,
    ROCE_RESOURCE_QP,
    ROCE_RESOURCE_SRQ,
    ROCE_RESOURCE_AH,

    ROCE_RESOURCE_MAX /* always keep last enumeration */
} roce_resource_type;

typedef struct roce_resource {
    roce_ctx *ctx;
    roce_spin_lock_t lock;
    uint32_t size;
    char name[8];
    void **addr;
} roce_resource;

/*
 * return 0 on success
 * return -EINVAL if size is zero or exceeds ROCE_MAX_GENERAL
 * return -ENOMEM if memory allocation fails
 * return negative errno on spinlock init failure */
int roce_resource_init(roce_ctx *ctx, roce_resource *resource, uint32_t size, const char *name);

/* destroy resource manager and free internal memory
 * note: caller must free all allocated resources before calling this */
void roce_resource_destroy(roce_resource *resource);

/* allocate a resource at any free slot
 * return index (>= 0) on success
 * return -EINVAL if res is NULL
 * return -EBUSY if no free slot available */
int roce_resource_alloc(roce_resource *resource, roce_res *res);

/* allocate a resource at specific index
 * return index (>= 0) on success
 * return -EINVAL if res is NULL or index exceeds size
 * return -EBUSY if slot at index is already occupied */
int roce_resource_alloc_at(roce_resource *resource, roce_res *res, uint32_t index);

/* get resource by index
 * return pointer to resource on success
 * return NULL if index exceeds size or slot is empty */
roce_res *roce_resource_get(roce_resource *resource, uint32_t index);

/* free resource at given index
 * return 0 on success
 * return -EINVAL if index exceeds size or slot is already empty */
int roce_resource_free(roce_resource *resource, uint32_t index);

#endif /* LIBROCE_RESOURCE_H */
