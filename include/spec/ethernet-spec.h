/*
 * Define data structures(in big endian) and types for Ethernet II.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_ETHERNET_SPEC_H
#define LIBROCE_ETHERNET_SPEC_H

#include <endian.h>
#include <stdint.h>
#include <string.h>

typedef struct ethernet_hdr {
    uint8_t dmac[6];
    uint8_t smac[6];
#define ETHERNET_IP 0x0800
#define ETHERNET_IPv6 0x86DD
    uint16_t type;
} ethernet_hdr;

static inline void ethernet_hdr_set(void *arg, uint8_t *dmac, uint8_t *smac, uint16_t type)
{
    ethernet_hdr *hdr = (ethernet_hdr *)arg;

    memcpy(hdr->dmac, dmac, sizeof(hdr->dmac));
    memcpy(hdr->smac, smac, sizeof(hdr->smac));
    hdr->type = htobe16(type);
}

static inline void ethernet_hdr_set4(void *arg, uint8_t *dmac, uint8_t *smac)
{
    ethernet_hdr_set(arg, dmac, smac, ETHERNET_IP);
}

static inline void ethernet_hdr_set6(void *arg, uint8_t *dmac, uint8_t *smac)
{
    ethernet_hdr_set(arg, dmac, smac, ETHERNET_IPv6);
}

static inline uint16_t ethernet_hdr_get_type(void *arg)
{
    ethernet_hdr *hdr = (ethernet_hdr *)arg;

    return be16toh(hdr->type);
}

#endif /* LIBROCE_ETHERNET_SPEC_H */
