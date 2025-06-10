/*
 * Utilities of libroce.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_UTIL_H
#define LIBROCE_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member)                                                            \
    ({                                                                                             \
        const typeof(((type *)0)->member) *__mptr = (ptr);                                         \
        (type *)((char *)__mptr - offsetof(type, member));                                         \
    })
#endif

#define ROCE_BUILD_BUG_ON(cond) ((void)sizeof(char[1 - 2 * !!(cond)]))

#define ROCE_MIN(x, y) (x > y ? y : x)

static inline bool roce_power_of_2(uint64_t val)
{
    if (!val) {
        return false;
    }

    return !(val & (val - 1));
}

static inline bool roce_valid_mtu(uint16_t mtu)
{
    switch (mtu) {
    case 1024:
    case 2048:
    case 4096:
        return true;

    /* the small MTU on a morden network interface leads poor performance */
    case 256:
    case 512:
    default:
        return false;
    };
}

#define ROCE_ASSERT(x) assert(x)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define ROCE_PICK_STRUCT(d, s, off)                                                                \
    {                                                                                              \
        uint8_t *_d = (uint8_t *)s + off;                                                          \
        d = (typeof(d))_d;                                                                         \
    }

static inline bool roce_gid_is_ipv4(const uint8_t *gid)
{
    static uint8_t ipv4_prefix[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

    return !memcmp(gid, ipv4_prefix, sizeof(ipv4_prefix));
}

static inline uint32_t roce_gid_to_ipv4(uint8_t *gid)
{
    uint32_t ip = gid[12];
    return ((ip << 24)) | (gid[13] << 16) | (gid[14] << 8) | gid[15];
}

static inline bool roce_mac_matched(uint8_t *src, uint8_t *dst)
{
    return !memcmp(src, dst, 6);
}

static inline void roce_mac_format(uint8_t *mac, char *str)
{
    sprintf(str, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline void roce_hex_format(uint8_t *hex, unsigned int len, char *str)
{
    /* 32 bytes per line */
    for (unsigned int lines = 0; lines < len / 32; lines++) {
        for (unsigned int i = 0; i < 32; i++) {
            sprintf(str, "%.2x", *hex);
            str += 2;
            hex++;
        }
        sprintf(str++, "\n");
    }

    for (unsigned int i = 0; i < len % 32; i++) {
        sprintf(str, "%.2x", *hex);
        str += 2;
        hex++;
    }
    sprintf(str++, "\n");
}

/*
 * Return >0 if psn0 > psn1
 *         0 if psn0 == psn1
 *        <0 if psn0 < psn1
 */
static inline int roce_compare_psn(uint32_t psn0, uint32_t psn1)
{
    int32_t diff;

    diff = (psn0 - psn1) << 8;
    return diff;
}

#endif /* LIBROCE_UTIL_H */
