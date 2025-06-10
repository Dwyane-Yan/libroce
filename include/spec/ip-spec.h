/*
 * Base on RFC 791, define data structures(in big endian) and types.
 * Link: https://datatracker.ietf.org/doc/html/rfc791
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_IP_SPEC_H
#define LIBROCE_IP_SPEC_H

#include <endian.h>
#include <stdbool.h>
#include <stdint.h>

#define IP_HDR_SIZE 20

typedef struct ip_hdr {
#define IP_VERSION_MASK (0xf0)
#define IP_IHL_MASK (0xf)
    uint8_t u8_0;
#define IP_TOS_FLAG_D (0x10)
#define IP_TOS_FLAG_T (0x08)
#define IP_TOS_FLAG_R (0x04)
    uint8_t tos;
    uint16_t length;
    uint16_t id;
#define IP_FRAG_FLAG_DF (0x4000)
#define IP_FRAG_FLAG_MF (0x2000)
#define IP_FRAG_OFFSET_MASK (0x1fff)
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t saddr;
    uint32_t daddr;
} ip_hdr;

static inline uint8_t ip_hdr_get_version(void *arg)
{
    ip_hdr *hdr = arg;

    return (hdr->u8_0 & IP_VERSION_MASK) >> 4;
}

static inline uint8_t ip_hdr_get_ihl(void *arg)
{
    ip_hdr *hdr = arg;

    return hdr->u8_0 & IP_IHL_MASK;
}

static inline void ip_hdr_set_version_ihl(void *arg, uint8_t version, uint8_t ihl)
{
    ip_hdr *hdr = arg;

    hdr->u8_0 = version << 4 | (ihl & IP_IHL_MASK);
}

typedef enum ip_precedence_type {
    IP_PRECEDENCE_ROUTINE = 0,
    IP_PRECEDENCE_PRIORITY = 1,
    IP_PRECEDENCE_IMMEDIATE = 2,
    IP_PRECEDENCE_FLASH = 3,
    IP_PRECEDENCE_FLASH_OVERRIDE = 4,
    IP_PRECEDENCE_CRITIC = 5,
    IP_PRECEDENCE_INTERNETWORK_CONTROL = 6,
    IP_PRECEDENCE_NETWORK_CONTROL = 7
} ip_precedence_type;

static inline const char *ip_precedence_string(ip_precedence_type precedence)
{
    static const char *precedence_strings[] = {"Routine",
                                               "Priority",
                                               "Immediate",
                                               "Flash",
                                               "Flash Override",
                                               "CRITIC/ECP",
                                               "Internetwork Control",
                                               "Network Control"};

    return precedence_strings[precedence];
}

static inline ip_precedence_type ip_hdr_get_precedence(void *arg)
{
    ip_hdr *hdr = arg;

    /* bit 7:5 */
    return hdr->tos >> 5;
}

static inline bool ip_hdr_get_tos_d(void *arg)
{
    ip_hdr *hdr = arg;

    return !!(hdr->tos & IP_TOS_FLAG_D);
}

static inline bool ip_hdr_get_tos_t(void *arg)
{
    ip_hdr *hdr = arg;

    return !!(hdr->tos & IP_TOS_FLAG_T);
}

static inline bool ip_hdr_get_tos_r(void *arg)
{
    ip_hdr *hdr = arg;

    return !!(hdr->tos & IP_TOS_FLAG_R);
}

static inline void ip_hdr_set_tos(void *arg, ip_precedence_type precedence, bool d, bool t, bool r)
{
    ip_hdr *hdr = arg;

    hdr->tos = (precedence << 5) | (d << 4) | (t << 3) | (r << 2);
}

static inline uint16_t ip_hdr_get_length(void *arg)
{
    ip_hdr *hdr = arg;

    return be16toh(hdr->length);
}

static inline void ip_hdr_set_length(void *arg, uint16_t length)
{
    ip_hdr *hdr = arg;

    hdr->length = htobe16(length);
}

static inline uint16_t ip_hdr_get_id(void *arg)
{
    ip_hdr *hdr = arg;

    return be16toh(hdr->id);
}

static inline void ip_hdr_set_id(void *arg, uint16_t id)
{
    ip_hdr *hdr = arg;

    hdr->id = htobe16(id);
}

static inline bool ip_hdr_get_frag_df(void *arg)
{
    ip_hdr *hdr = arg;

    return !!(be16toh(hdr->frag_off) & IP_FRAG_FLAG_DF);
}

static inline bool ip_hdr_get_frag_mf(void *arg)
{
    ip_hdr *hdr = arg;

    return !!(be16toh(hdr->frag_off) & IP_FRAG_FLAG_MF);
}

static inline uint16_t ip_hdr_get_offset(void *arg)
{
    ip_hdr *hdr = arg;

    return be16toh(hdr->frag_off) & IP_FRAG_OFFSET_MASK;
}

static inline void id_hdr_set_frag_off(void *arg, bool df, bool mf, uint16_t offset)
{
    ip_hdr *hdr = arg;
    uint16_t frag_off;

    /* bit 15 is reserved as 0 */
    frag_off = (df << 14) | (mf << 13) | (offset & IP_FRAG_OFFSET_MASK);
    hdr->frag_off = htobe16(frag_off);
}

static inline uint8_t ip_hdr_get_ttl(void *arg)
{
    ip_hdr *hdr = arg;

    return hdr->ttl;
}

static inline void ip_hdr_set_ttl(void *arg, uint8_t ttl)
{
    ip_hdr *hdr = arg;

    hdr->ttl = ttl;
}

static inline uint8_t ip_hdr_get_protocol(void *arg)
{
    ip_hdr *hdr = arg;

    return hdr->protocol;
}

static inline void ip_hdr_set_protocol(void *arg, uint8_t protocol)
{
    ip_hdr *hdr = arg;

    hdr->protocol = protocol;
}

static inline uint16_t ip_hdr_get_checksum(void *arg)
{
    ip_hdr *hdr = arg;

    return hdr->checksum;
}

static inline void ip_hdr_set_checksum(void *arg, uint16_t checksum)
{
    ip_hdr *hdr = arg;

    hdr->checksum = checksum;
}

static inline uint32_t ip_hdr_get_saddr(void *arg)
{
    ip_hdr *hdr = arg;

    return be32toh(hdr->saddr);
}

static inline void ip_hdr_set_saddr(void *arg, uint32_t saddr)
{
    ip_hdr *hdr = arg;

    hdr->saddr = htobe32(saddr);
}

static inline uint32_t ip_hdr_get_daddr(void *arg)
{
    ip_hdr *hdr = arg;

    return be32toh(hdr->daddr);
}

static inline void ip_hdr_set_daddr(void *arg, uint32_t daddr)
{
    ip_hdr *hdr = arg;

    hdr->daddr = htobe32(daddr);
}

#endif /* LIBROCE_IP_SPEC_H */
