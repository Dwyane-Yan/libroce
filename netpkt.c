/*
 * Network packet implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "private/mm.h"
#include "private/netpkt.h"
#include "private/checksum.h"
#include "private/crc.h"
#include "private/iov-helper.h"

roce_netpkt *roce_netpkt_alloc(roce_ctx *ctx, roce_qp *qp, void *opaque, uint16_t pages,
                               uint8_t padcnt)
{
    uint8_t vnet_hdr = roce_qp_vnet_hdr(ctx, qp);
    roce_netpkt *pkt;
    struct iovec *iov;
    uint32_t num_iov = pages + 8;

    pkt = roce_calloc(ctx, 1, sizeof(roce_netpkt) + num_iov * sizeof(struct iovec));
    if (!pkt) {
        return NULL;
    }

    list_node_init(&pkt->entry);
    pkt->opaque = opaque;
    pkt->num_iov = num_iov;

    /* setup IO vectors */
    if (vnet_hdr) {
        iov = &pkt->iovs[ROCE_NETPKT_VNET_HDR];
        iov->iov_base = pkt->vneth;
        iov->iov_len = vnet_hdr;
    }

    iov = &pkt->iovs[ROCE_NETPKT_ETHERNET];
    iov->iov_base = &pkt->ethh;
    iov->iov_len = sizeof(ethernet_hdr);

    /* skip IP/IPv6, it will be initialized later */

    iov = &pkt->iovs[ROCE_NETPKT_UDPHDR];
    iov->iov_base = &pkt->udph;
    iov->iov_len = sizeof(udp_hdr);

    iov = &pkt->iovs[ROCE_NETPKT_BTH];
    iov->iov_base = &pkt->bth;
    iov->iov_len = sizeof(ib_bth);

    /* skip IB ETH, it will be initialized if has */
    /* skip IB PAYLOAD, it will be initialized later */

    if (padcnt) {
        iov = &pkt->iovs[num_iov - 2];
        iov->iov_base = pkt->pad;
        iov->iov_len = padcnt;
    }

    iov = &pkt->iovs[num_iov - 1];
    iov->iov_base = &pkt->icrc;
    iov->iov_len = sizeof(uint32_t);

    return pkt;
}

void roce_netpkt_free(roce_ctx *ctx, roce_netpkt *pkt)
{
    roce_free(ctx, pkt);
}

static void roce_netpkt_set4(roce_netpkt *pkt, uint8_t *dmac, uint8_t *smac, uint8_t hop_limit,
                             uint8_t tclass, uint8_t *sgid, uint8_t *dgid, uint16_t payload_len)
{
    ethernet_hdr *eth;
    ip_hdr *ip4;
    udp_hdr *udp;
    struct iovec *iov;

    /* #1 skip vnet: zerod vnet means no virtio-vnet feature enabled */
    /* #2 ethernet header */
    eth = roce_netpkt_ethernet(pkt);
    ethernet_hdr_set4(eth, dmac, smac);

    /* #3 ipv4 header */
    ip4 = roce_netpkt_ip4(pkt);
    ip_hdr_set_version_ihl(ip4, 4, sizeof(ip_hdr) >> 2);
    /* XXX: TOS */
    ip_hdr_set_length(ip4, sizeof(ip_hdr) + sizeof(udp_hdr) + payload_len + 4);
    /* XXX: id */
    id_hdr_set_frag_off(ip4, true, false, 0);
    ip_hdr_set_ttl(ip4, hop_limit);
    ip_hdr_set_protocol(ip4, 17); /* UDP */
    ip_hdr_set_saddr(ip4, roce_gid_to_ipv4(sgid));
    ip_hdr_set_daddr(ip4, roce_gid_to_ipv4(dgid));
    ip_hdr_set_checksum(ip4, roce_checksum((uint16_t *)ip4, sizeof(ip_hdr)));

    /* #4 UDP header */
    udp = roce_netpkt_udp(pkt);
    udp_hdr_set_sport(udp, ROCE_V2_UDP_DPORT);
    udp_hdr_set_dport(udp, ROCE_V2_UDP_DPORT);
    udp_hdr_set_length(udp, sizeof(udp_hdr) + payload_len + 4);
    /* ignore UDP checksum */

    /* #FINAL ICRC. @roce_crc32_pkt will change ip6 header, udp header and BTH */
    ip_hdr __ip4 = pkt->ip4;
    udp_hdr __udph = pkt->udph;
    ib_bth __bth = pkt->bth;
    pkt->icrc = roce_crc32_pkt(true, &__ip4, NULL, 0, &__udph, &__bth, &pkt->iovs[ROCE_NETPKT_ETH],
                               pkt->num_iov - ROCE_NETPKT_ETH - 1);

    /* #LAST update iovec */
    iov = &pkt->iovs[ROCE_NETPKT_IPHDR];
    iov->iov_base = &pkt->ip4;
    iov->iov_len = sizeof(ip_hdr);
}

static void roce_netpkt_set6(roce_netpkt *pkt, uint8_t *dmac, uint8_t *smac, uint8_t hop_limit,
                             uint8_t tclass, uint8_t *sgid, uint8_t *dgid, uint16_t payload_len)
{
    ethernet_hdr *eth;
    ipv6_hdr *ip6;
    udp_hdr *udp;
    struct iovec *iov;

    /* #1 skip vnet: zerod vnet means no virtio-vnet feature enabled */
    /* #2 ethernet header */
    eth = roce_netpkt_ethernet(pkt);
    ethernet_hdr_set6(eth, dmac, smac);

    /* #3 ipv6 header */
    ip6 = roce_netpkt_ip6(pkt);
    ipv6_hdr_set_version_tc_fl(ip6, 6, tclass, 0);
    ipv6_hdr_set_payload_len(ip6, sizeof(udp_hdr) + payload_len + 4);
    ipv6_hdr_set_nexthdr(ip6, 17); /* UDP */
    ipv6_hdr_set_hop_limit(ip6, hop_limit);
    ipv6_hdr_set_saddr(ip6, sgid);
    ipv6_hdr_set_daddr(ip6, dgid);

    /* #4 UDP header */
    udp = roce_netpkt_udp(pkt);
    udp_hdr_set_sport(udp, ROCE_V2_UDP_DPORT);
    udp_hdr_set_dport(udp, ROCE_V2_UDP_DPORT);
    udp_hdr_set_length(udp, sizeof(udp_hdr) + payload_len + 4);

    /* #FINAL ICRC. @roce_crc32_pkt will change ip6 header, udp header and BTH */
    ipv6_hdr __ip6 = pkt->ip6;
    udp_hdr __udph = pkt->udph;
    ib_bth __bth = pkt->bth;
    pkt->icrc = roce_crc32_pkt(false, &__ip6, NULL, 0, &__udph, &__bth, &pkt->iovs[ROCE_NETPKT_ETH],
                               pkt->num_iov - ROCE_NETPKT_ETH - 1);

    /* #LAST update iovec */
    iov = &pkt->iovs[ROCE_NETPKT_IPHDR];
    iov->iov_base = &pkt->ip6;
    iov->iov_len = sizeof(ipv6_hdr);
}

void roce_netpkt_set_eth(roce_netpkt *pkt, roce_port *port, roce_ah_attr *ah_attr)
{
    uint8_t *smac = port->mac;
    uint8_t *sgid = port->gids[ah_attr->sgid_index];
    uint8_t *dmac = ah_attr->mac;
    uint8_t *dgid = ah_attr->gid;
    uint32_t payload_len =
        roce_iov_len(&pkt->iovs[ROCE_NETPKT_BTH], pkt->num_iov - ROCE_NETPKT_UDPHDR - 2);
    uint8_t hop_limit = ah_attr->hop_limit;
    uint8_t tclass = ah_attr->traffic_class;
    bool is_ipv4 = roce_gid_is_ipv4(ah_attr->gid);

    if (is_ipv4) {
        roce_netpkt_set4(pkt, dmac, smac, hop_limit, tclass, sgid, dgid, payload_len);
    } else {
        roce_netpkt_set6(pkt, dmac, smac, hop_limit, tclass, sgid, dgid, payload_len);
    }
}

void roce_netpkt_set_bth(ib_bth *bth, uint8_t qp_type, uint8_t opcode, bool se, uint8_t padcnt,
                         uint32_t destqp, bool ack, uint32_t psn)
{
    ib_bth_set_opcode(bth, roce_qp_type_to_ib(qp_type), opcode);
    ib_bth_set_se(bth, se);
    ib_bth_set_mig(bth, false);
    ib_bth_set_padcnt(bth, padcnt);
    ib_bth_set_tver(bth, 0);
    ib_bth_set_pkey(bth, ROCE_PKEY);
    ib_bth_set_fecn(bth, false);
    ib_bth_set_becn(bth, false);
    ib_bth_clear_resv1(bth);
    ib_bth_set_destqp(bth, destqp);
    ib_bth_set_ack(bth, ack);
    ib_bth_clear_resv2(bth);
    ib_bth_set_psn(bth, psn);
}

void roce_netpkt_acknowledge(roce_ctx *ctx, roce_qp *qp, ib_aeth_syndrome_type syndrome,
                             uint8_t val, uint32_t psn)
{
    uint32_t destqp = qp->attr.dest_qp_num;
    roce_netpkt *pkt;
    ib_bth *bth;
    ib_aeth *aeth;
    struct iovec *iov;

    ROCE_ASSERT(qp->qp_type == ROCE_QPT_RC);
    pkt = roce_netpkt_alloc(ctx, qp, NULL, 0, 0);
    ROCE_ASSERT(pkt);

    bth = roce_netpkt_bth(pkt);
    roce_netpkt_set_bth(bth, qp->qp_type, IB_OPCODE_ACKNOWLEDGE, false, 0, destqp, false, psn);

    aeth = roce_netpkt_aeth(pkt);
    ib_aeth_set(aeth, syndrome, val, qp->resp.msn++);
    iov = &pkt->iovs[ROCE_NETPKT_ETH];
    iov->iov_base = &pkt->aeth;
    iov->iov_len = sizeof(ib_aeth);

    roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);
    roce_netpkt_xmit(ctx, pkt);

    roce_netpkt_free(ctx, pkt);
}
