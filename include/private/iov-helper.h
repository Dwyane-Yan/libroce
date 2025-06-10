/*
 * IO vector helper functions.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_IOV_HELPER_H
#define LIBROCE_IOV_HELPER_H

#include <sys/uio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Count total bytes of @iovs array. null @iovs and zero @num_iov are not expected */
static inline uint32_t roce_iov_len(const struct iovec *iovs, uint32_t num_iov)
{
    const struct iovec *iov;
    uint32_t total = 0;

    assert(iovs && num_iov);

    for (uint32_t i = 0; i < num_iov; i++) {
        iov = &iovs[i];
        total += iov->iov_len;
    }

    return total;
}

/* Duplicate iovec */
static inline void roce_iov_dup(const struct iovec *src_iovs, struct iovec *dst_iovs,
                                uint32_t num_iov)
{
    assert(src_iovs != NULL);
    assert(dst_iovs != NULL);

    memcpy(dst_iovs, src_iovs, sizeof(struct iovec) * num_iov);
}

#ifndef ROCE_MIN
#define ROCE_MIN(x, y) (x > y ? y : x)
#endif

/* Skip @offset, build from @src_iovs to @dst_iovs of @bytes.
 * Don't worry on the performance. The iov->iov_len usually has 4K bytes, @offset is usually 1K.
 */
static inline uint32_t roce_iov_copy(struct iovec *src_iovs, uint32_t src_num_iov, uint32_t offset,
                                     uint32_t bytes, struct iovec *dst_iovs)
{
    struct iovec *src_iov = NULL, *dst_iov;
    uint32_t dst_num_iov = 0, i;

    assert(src_iovs && src_num_iov);
    assert(dst_iovs);

    if (bytes == 0) {
        return 0;
    }

    /* skip @offset bytes */
    for (i = 0; i < src_num_iov; i++) {
        src_iov = &src_iovs[i];
        if (src_iov->iov_len > offset) {
            break;
        }

        offset -= src_iov->iov_len;
    }

    /* no more space? */
    if ((i == src_num_iov) || !src_iov) {
        return 0;
    }

    /* any remaining bytes in the first iov? */
    dst_iov = &dst_iovs[dst_num_iov++];
    dst_iov->iov_base = (uint8_t *)src_iov->iov_base + offset;
    dst_iov->iov_len = ROCE_MIN(src_iov->iov_len - offset, bytes);
    bytes -= dst_iov->iov_len;
    if (!bytes) {
        return dst_num_iov;
    }

    /* copy the remaining iov */
    for (i++; i < src_num_iov; i++, dst_num_iov++) {
        src_iov = &src_iovs[i];
        dst_iov = &dst_iovs[dst_num_iov];
        dst_iov->iov_base = src_iov->iov_base;
        dst_iov->iov_len = ROCE_MIN(src_iov->iov_len, bytes);
        bytes -= dst_iov->iov_len;
        if (!bytes) {
            break;
        }
    }

    return dst_num_iov + 1;
}

/*
 * Copy @bytes from @iovs to @buf. If @buf is NULL, discard buffer without memcpy.
 * Note that @iovs is changed.
 */
static inline uint32_t roce_iov_advance(struct iovec *iovs, uint32_t num_iov, uint32_t bytes,
                                        void *_buf)
{
    uint8_t *buf = _buf;
    struct iovec *iov;
    uint32_t copied = 0, tocopy;
    uint32_t idx;

    assert(iovs && num_iov);

    if (bytes == 0) {
        return 0;
    }

    for (idx = 0; idx < num_iov; idx++) {
        iov = &iovs[idx];
        if (!iov->iov_len) {
            continue;
        }

        tocopy = bytes - copied;
        if (iov->iov_len > tocopy) {
            if (buf) {
                memcpy(buf + copied, iov->iov_base, tocopy);
            }
            copied += tocopy;
            iov->iov_base = (uint8_t *)iov->iov_base + tocopy;
            iov->iov_len -= tocopy;
            break;
        } else if (iov->iov_len == tocopy) {
            if (buf) {
                memcpy(buf + copied, iov->iov_base, tocopy);
            }
            copied += tocopy;
            iov->iov_base = NULL;
            iov->iov_len = 0;
            break;
        } else {
            if (buf) {
                memcpy(buf + copied, iov->iov_base, iov->iov_len);
            }
            copied += iov->iov_len;
            iov->iov_base = NULL;
            iov->iov_len = 0;
        }
    }

    return copied;
}

/*
 * Copy @bytes from @iovs to @buf from last. If @buf is NULL, discard buffer without memcpy
 * Note that @iovs is changed.
 */
static inline uint32_t roce_iov_reverse(struct iovec *iovs, uint32_t num_iov, uint32_t bytes,
                                        void *_buf)
{
    uint8_t *buf = _buf;
    struct iovec *iov;
    uint32_t copied = 0, tocopy;
    int32_t idx;

    assert(iovs && num_iov);

    for (idx = num_iov - 1; idx >= 0; idx--) {
        iov = &iovs[idx];
        if (!iov->iov_len) {
            continue;
        }

        tocopy = bytes - copied;
        if (iov->iov_len > tocopy) {
            if (buf) {
                memcpy(buf + bytes - copied - tocopy, iov->iov_base + iov->iov_len - tocopy,
                       tocopy);
            }
            copied += tocopy;
            iov->iov_len -= tocopy;
            break;
        } else if (iov->iov_len == tocopy) {
            if (buf) {
                memcpy(buf + bytes - copied - tocopy, iov->iov_base + iov->iov_len - tocopy,
                       tocopy);
            }
            copied += tocopy;
            iov->iov_base = NULL;
            iov->iov_len = 0;
            break;
        } else {
            if (buf) {
                memcpy(buf + bytes - copied - iov->iov_len, iov->iov_base, iov->iov_len);
            }
            copied += iov->iov_len;
            iov->iov_base = NULL;
            iov->iov_len = 0;
        }
    }

    return copied;
}

#endif /* LIBROCE_IOV_HELPER_H */
