/*
 * Network packet management, encap, decap.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_NETPKT_H
#define LIBROCE_NETPKT_H

#include "spec/ethernet-spec.h"
#include "spec/ip-spec.h"
#include "spec/ipv6-spec.h"
#include "spec/udp-spec.h"
#include "spec/ib-spec.h"

#include "private/roce-private.h"
#include "private/list.h"

typedef struct roce_netpkt {
    struct list_node entry; /* list entry for QP packets management */
    void *opaque;           /* XXX_ONLY, XXX_LAST should use this only */
    uint8_t vneth[ROCE_MAX_VNET_HDR];
    ethernet_hdr ethh;
    union {
        ip_hdr ip4;
        ipv6_hdr ip6;
    };
    udp_hdr udph;
    ib_bth bth;
    union {
        ib_immdt immdt;
        ib_ieth ieth;
        ib_aeth aeth;
        struct {
            ib_reth reth;
            ib_immdt immdt;
        } write;
        struct {
            ib_deth deth;
            ib_immdt immdt;
        } ud;
    };
    uint8_t pad[4];
    uint32_t icrc;
#define ROCE_NETPKT_VNET_HDR 0
#define ROCE_NETPKT_ETHERNET 1
#define ROCE_NETPKT_IPHDR 2
#define ROCE_NETPKT_UDPHDR 3
#define ROCE_NETPKT_BTH 4
#define ROCE_NETPKT_ETH 5
    /* IB PAYLOAD has at least 1 iovec */
#define ROCE_NETPKT_PAYLOAD 6
    /* IB PAD: num_iov - 2 */
    /* ICRC: num_iov - 1 */
    uint32_t num_iov;
    struct iovec iovs[0];
} roce_netpkt;

static inline ethernet_hdr *roce_netpkt_ethernet(roce_netpkt *pkt)
{
    return &pkt->ethh;
}

static inline ip_hdr *roce_netpkt_ip4(roce_netpkt *pkt)
{
    return &pkt->ip4;
}

static inline ipv6_hdr *roce_netpkt_ip6(roce_netpkt *pkt)
{
    return &pkt->ip6;
}

static inline udp_hdr *roce_netpkt_udp(roce_netpkt *pkt)
{
    return &pkt->udph;
}

static inline ib_bth *roce_netpkt_bth(roce_netpkt *pkt)
{
    return &pkt->bth;
}

static inline ib_immdt *roce_netpkt_immdt(roce_netpkt *pkt)
{
    return &pkt->immdt;
}

static inline ib_ieth *roce_netpkt_ieth(roce_netpkt *pkt)
{
    return &pkt->ieth;
}

static inline ib_aeth *roce_netpkt_aeth(roce_netpkt *pkt)
{
    return &pkt->aeth;
}

static inline ib_reth *roce_netpkt_write_reth(roce_netpkt *pkt)
{
    return &pkt->write.reth;
}

static inline ib_immdt *roce_netpkt_write_immdt(roce_netpkt *pkt)
{
    return &pkt->write.immdt;
}

static inline ib_deth *roce_netpkt_ud_deth(roce_netpkt *pkt)
{
    return &pkt->ud.deth;
}

static inline ib_immdt *roce_netpkt_ud_immdt(roce_netpkt *pkt)
{
    return &pkt->ud.immdt;
}

static inline struct iovec *roce_netpkt_payload(roce_netpkt *pkt)
{
    return &pkt->iovs[ROCE_NETPKT_PAYLOAD];
}

roce_netpkt *roce_netpkt_alloc(roce_ctx *ctx, roce_qp *qp, void *opaque, uint16_t pages,
                               uint8_t padcnt);

void roce_netpkt_free(roce_ctx *ctx, roce_netpkt *pkt);

static inline void roce_netpkt_xmit(roce_ctx *ctx, roce_netpkt *pkt)
{
    ctx->para.net_xmit(ctx->para.ctx_opaque, 0, pkt->iovs, pkt->num_iov);
}

void roce_netpkt_set_bth(ib_bth *bth, uint8_t qp_type, uint8_t opcode, bool se, uint8_t padcnt,
                         uint32_t destqp, bool ack, uint32_t psn);

void roce_netpkt_set_eth(roce_netpkt *pkt, roce_port *port, roce_ah_attr *ah_attr);

void roce_netpkt_acknowledge(roce_ctx *ctx, roce_qp *qp, ib_aeth_syndrome_type syndrome,
                             uint8_t val, uint32_t psn);

#endif /* LIBROCE_NETPKT_H */
