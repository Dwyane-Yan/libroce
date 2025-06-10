/*
 * Memory management for libroce. Use ptmalloc(libc malloc) by default.
 * Allow uplayer set memory management handler. For example:
 *   - QEMU may use glib style handler like g_malloc/g_free.
 *   - DPDK may use RTE functions.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_MM_H
#define LIBROCE_MM_H

#include "private/roce-private.h"

static inline void *roce_malloc(roce_ctx *ctx, size_t size)
{
    return ctx->para.malloc(size);
}

static inline void roce_free(roce_ctx *ctx, void *ptr)
{
    ctx->para.free(ptr);
}

static inline void *roce_calloc(roce_ctx *ctx, size_t nmemb, size_t size)
{
    return ctx->para.calloc(nmemb, size);
    ;
}

static inline void *roce_realloc(roce_ctx *ctx, void *ptr, size_t size)
{
    return ctx->para.realloc(ptr, size);
    ;
}

#endif /* LIBROCE_MM_H */
