/*
 * Spin lock implementations.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_LOCK_H
#define LIBROCE_LOCK_H

#include "private/roce-private.h"

static inline int roce_spin_init(roce_ctx *ctx, roce_spin_lock_t *lock)
{
    return ctx->para.spin.init(lock, 0);
}

static inline int roce_spin_lock(roce_ctx *ctx, roce_spin_lock_t *lock)
{
    return ctx->para.spin.lock(lock);
}

static inline int roce_spin_trylock(roce_ctx *ctx, roce_spin_lock_t *lock)
{
    return ctx->para.spin.trylock(lock);
}

static inline int roce_spin_unlock(roce_ctx *ctx, roce_spin_lock_t *lock)
{
    return ctx->para.spin.unlock(lock);
}

#endif /* LIBROCE_LOCK_H */
