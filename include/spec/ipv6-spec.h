/*
 * Base on RFC 2460, define data structures(in big endian) and types.
 * Link: https://datatracker.ietf.org/doc/html/rfc2460
 *
 * Author: zhenwei pi zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_IPV6_SPEC_H
#define LIBROCE_IPV6_SPEC_H

#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define IPV6_HDR_SIZE 40

typedef struct ipv6_hdr {
#define IPV6_VERSION_MASK (0xf0000000)
#define IPV6_TCLASS_MASK (0x0ff00000)
#define IPV6_FLOW_LABEL_MASK (0x000fffff)
    uint32_t u32_0;
    uint16_t payload_len;
    uint8_t nexthdr;
    uint8_t hop_limit;
    uint8_t saddr[16];
    uint8_t daddr[16];
} ipv6_hdr;

static inline uint8_t ipv6_hdr_get_version(void *arg)
{
    ipv6_hdr *hdr = arg;

    return be32toh(hdr->u32_0) >> 28;
}

static inline uint8_t ipv6_hdr_get_tclass(void *arg)
{
    ipv6_hdr *hdr = arg;

    return (be32toh(hdr->u32_0) & IPV6_TCLASS_MASK) >> 20;
}

static inline uint32_t ipv6_hdr_get_flow_lable(void *arg)
{
    ipv6_hdr *hdr = arg;

    return be32toh(hdr->u32_0) & IPV6_FLOW_LABEL_MASK;
}

static inline void ipv6_hdr_set_version_tc_fl(void *arg, uint8_t version, uint8_t tclass,
                                              uint32_t flow_lable)
{
    ipv6_hdr *hdr = arg;
    uint32_t u32val;

    u32val = ((version & 0xf) << 28) | (tclass << 20) | (flow_lable & IPV6_FLOW_LABEL_MASK);
    hdr->u32_0 = htobe32(u32val);
}

static inline uint16_t ipv6_hdr_get_payload_len(void *arg)
{
    ipv6_hdr *hdr = arg;

    return be16toh(hdr->payload_len);
}

static inline void ipv6_hdr_set_payload_len(void *arg, uint16_t payload_len)
{
    ipv6_hdr *hdr = arg;

    hdr->payload_len = htobe16(payload_len);
}

static inline uint8_t ipv6_hdr_get_nexthdr(void *arg)
{
    ipv6_hdr *hdr = arg;

    return hdr->nexthdr;
}

static inline void ipv6_hdr_set_nexthdr(void *arg, uint8_t nexthdr)
{
    ipv6_hdr *hdr = arg;

    hdr->nexthdr = nexthdr;
}

static inline uint8_t ipv6_hdr_get_hop_limit(void *arg)
{
    ipv6_hdr *hdr = arg;

    return hdr->hop_limit;
}

static inline void ipv6_hdr_set_hop_limit(void *arg, uint8_t hop_limit)
{
    ipv6_hdr *hdr = arg;

    hdr->hop_limit = hop_limit;
}

static inline void ipv6_hdr_set_saddr(void *arg, uint8_t *saddr)
{
    ipv6_hdr *hdr = arg;
    uint8_t *dst = (uint8_t *)hdr->saddr;

    memcpy(dst, saddr, sizeof(hdr->saddr));
}

static inline void ipv6_hdr_set_daddr(void *arg, uint8_t *daddr)
{
    ipv6_hdr *hdr = arg;
    uint8_t *dst = (uint8_t *)hdr->daddr;

    memcpy(dst, daddr, sizeof(hdr->daddr));
}

#endif /* LIBROCE_IPV6_SPEC_H */
