/*
 * Base on RFC 768, define data structures(in big endian) and types.
 * Link: https://datatracker.ietf.org/doc/html/rfc768
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_UDP_SPEC_H
#define LIBROCE_UDP_SPEC_H

#include <endian.h>
#include <stdint.h>

typedef struct udp_hdr {
    uint16_t sport;
    uint16_t dport;
    uint16_t length;
    uint16_t checksum;
} udp_hdr;

static inline uint16_t udp_hdr_get_sport(void *arg)
{
    udp_hdr *hdr = arg;

    return be16toh(hdr->sport);
}

static inline void udp_hdr_set_sport(void *arg, uint16_t sport)
{
    udp_hdr *hdr = arg;

    hdr->sport = htobe16(sport);
}

static inline uint16_t udp_hdr_get_dport(void *arg)
{
    udp_hdr *hdr = arg;

    return be16toh(hdr->dport);
}

static inline void udp_hdr_set_dport(void *arg, uint16_t dport)
{
    udp_hdr *hdr = arg;

    hdr->dport = htobe16(dport);
}

static inline uint16_t udp_hdr_get_length(void *arg)
{
    udp_hdr *hdr = arg;

    return be16toh(hdr->length);
}

static inline void udp_hdr_set_length(void *arg, uint16_t length)
{
    udp_hdr *hdr = arg;

    hdr->length = htobe16(length);
}
static inline uint16_t udp_hdr_get_checksum(void *arg)
{
    udp_hdr *hdr = arg;

    return hdr->checksum;
}

static inline void udp_hdr_set_checksum(void *arg, uint16_t checksum)
{
    udp_hdr *hdr = arg;

    hdr->checksum = checksum;
}

#endif /* LIBROCE_UDP_SPEC_H */
