/*
 * Log implementations.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_LOG_H
#define LIBROCE_LOG_H

#include "private/roce-private.h"

#define roce_log(ctx, level, prefix, fmt, ...)                                                     \
    do {                                                                                           \
        if ((level <= ctx->para.log_level) && ctx->para.log) {                                     \
            snprintf(ctx->logbuf, sizeof(ctx->logbuf), prefix fmt, ##__VA_ARGS__);                 \
            ctx->para.log(ctx->para.ctx_opaque, ctx->logbuf);                                      \
        }                                                                                          \
    } while (0);

#define roce_log_error(ctx, fmt, ...) roce_log(ctx, roce_log_error, "error: ", fmt, ##__VA_ARGS__)

#define roce_log_warn(ctx, fmt, ...) roce_log(ctx, roce_log_warn, "warn: ", fmt, ##__VA_ARGS__)

#define roce_log_notice(ctx, fmt, ...)                                                             \
    roce_log(ctx, roce_log_notice, "notice: ", fmt, ##__VA_ARGS__)

#define roce_log_info(ctx, fmt, ...) roce_log(ctx, roce_log_info, "info: ", fmt, ##__VA_ARGS__)

#define roce_log_debug(ctx, fmt, ...) roce_log(ctx, roce_log_debug, "debug: ", fmt, ##__VA_ARGS__)

#endif /* LIBROCE_LOG_H */
