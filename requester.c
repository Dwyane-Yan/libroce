/*
 * RDMA requester implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "private/roce-private.h"
#include "private/sge-helper.h"
#include "private/mm.h"
#include "private/lock.h"
#include "private/netpkt.h"
#include "private/crc.h"
#include "private/log.h"

#define ROCE_MAX_OUTGOING_PACKETS 8

static inline uint16_t roce_get_mtu(roce_ctx *ctx, roce_qp *qp)
{
    if ((qp->qp_type == ROCE_QPT_RC) || (qp->qp_type == ROCE_QPT_UC)) {
        return qp->attr.path_mtu;
    }

    return ctx->ports[0].attr.active_mtu;
}

static inline uint32_t roce_netpkt_psn(roce_netpkt *pkt)
{
    return ib_bth_get_psn(roce_netpkt_bth(pkt));
}

static inline void roce_netpkt_pending_one(roce_ctx *ctx, roce_qp *qp, roce_netpkt *pkt)
{
    roce_spin_lock(ctx, &qp->req.lock);
    list_add_tail(&qp->req.pending_pkts, &pkt->entry);
    roce_spin_unlock(ctx, &qp->req.lock);
    roce_log_debug(ctx, "QP(0x%x) add pending packet PSN(0x%x)", qp->res.handle,
                   roce_netpkt_psn(pkt));
}

static inline void roce_netpkt_pending_multi(roce_ctx *ctx, roce_qp *qp, struct list_head *pkts)
{
    roce_netpkt *top_pkt = list_top(pkts, roce_netpkt, entry);
    roce_netpkt *tail_pkt = list_tail(pkts, roce_netpkt, entry);

    roce_spin_lock(ctx, &qp->req.lock);
    list_append_list(&qp->req.pending_pkts, pkts);
    roce_spin_unlock(ctx, &qp->req.lock);
    roce_log_debug(ctx, "QP(0x%x) add pending packet PSN(0x%x) - PSN(0x%x)", qp->res.handle,
                   roce_netpkt_psn(top_pkt), roce_netpkt_psn(tail_pkt));
}

static inline void roce_netpkt_free_multi(roce_ctx *ctx, roce_qp *qp, struct list_head *pkts)
{
    roce_netpkt *pkt, *tmp_pkt;

    list_for_each_safe (pkts, pkt, tmp_pkt, entry) {
        list_del(&pkt->entry);
        roce_netpkt_free(ctx, pkt);
    }
}

static inline void roce_netpkt_outgoing(roce_ctx *ctx, roce_qp *qp, roce_netpkt *pkt)
{
    /* pop net packet from the head of pending queue, append it to tail of outgoing queue */
    roce_spin_lock(ctx, &qp->req.lock);
    list_del(&pkt->entry);
    list_add_tail(&qp->req.outgoing_pkts, &pkt->entry);
    roce_spin_unlock(ctx, &qp->req.lock);
    roce_log_debug(ctx, "QP(0x%x) add outgoing packet PSN 0x%x", qp->res.handle,
                   roce_netpkt_psn(pkt));
}

static void roce_rc_send_only(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *send_wr, uint8_t inv,
                              uint8_t imm, uint32_t payload)
{
    roce_netpkt *pkt;
    uint8_t padcnt;
    uint8_t opcode;
    uint32_t psn = qp->req.psn;
    uint32_t destqp = qp->attr.dest_qp_num;
    struct iovec *iov;
    uint32_t pages, map_pages;
    bool dmamap;

    if (roce_send_wr_is_inline(send_wr)) {
        pages = 1;
    } else {
        pages = roce_sge_pages(send_wr->send_wr.sge, send_wr->send_wr.num_sge, ctx->para.page_size);
    }

    padcnt = (-payload) & 0x3;
    pkt = roce_netpkt_alloc(ctx, qp, send_wr, pages, padcnt);
    if (!pkt) {
        send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
        return;
    }

    if (inv) {
        opcode = IB_OPCODE_SEND_ONLY_WITH_INVALIDATE;
        ib_ieth *ieth = roce_netpkt_ieth(pkt);
        ib_ieth_set_rkey(ieth, send_wr->send_wr.invalidated_rkey);
        iov = &pkt->iovs[ROCE_NETPKT_ETH];
        iov->iov_base = ieth;
        iov->iov_len = IB_IETH_SIZE;
    } else if (imm) {
        opcode = IB_OPCODE_SEND_ONLY_WITH_IMMEDIATE;
        ib_immdt *immdt = roce_netpkt_immdt(pkt);
        ib_immdt_set_immdt(immdt, send_wr->send_wr.imm_data);
        iov = &pkt->iovs[ROCE_NETPKT_ETH];
        iov->iov_base = immdt;
        iov->iov_len = IB_IMMDT_SIZE;
    } else {
        opcode = IB_OPCODE_SEND_ONLY;
    }

    if (roce_send_wr_is_inline(send_wr)) {
        iov = roce_netpkt_payload(pkt);
        iov->iov_base = send_wr->send_wr.sge;
        iov->iov_len = payload;
    } else {
        map_pages = roce_mr_map_sge(ctx, qp, send_wr->send_wr.sge, send_wr->send_wr.num_sge,
                                    roce_netpkt_payload(pkt), true, 0, &dmamap);
        if (map_pages < 0) {
            roce_netpkt_free(ctx, pkt);
            send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
            return;
        }
        ROCE_ASSERT(pages == map_pages);
    }

    bool se = !!(send_wr->send_wr.send_flags & ROCE_SEND_SOLICITED);
    ib_bth *bth = roce_netpkt_bth(pkt);
    roce_netpkt_set_bth(bth, qp->qp_type, opcode, se, padcnt, destqp, true, psn);
    roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);

    roce_netpkt_pending_one(ctx, qp, pkt);
    qp->req.psn = roce_qp_psn_inc(psn);
}

static void roce_rc_send_multi(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *send_wr, uint8_t inv,
                               uint8_t imm, uint32_t payload, uint16_t mtu)
{
    roce_netpkt *pkt;
    ib_bth *bth;
    uint8_t qp_type = qp->qp_type;
    uint8_t opcode;
    bool ack;
    uint32_t destqp = qp->attr.dest_qp_num;
    uint32_t psn = qp->req.psn;
    roce_sge dst_sges[ROCE_MAX_SGE];
    uint32_t sge_idx = 0, off = 0, dst_num_sge;
    LIST_HEAD(pkts);
    uint32_t pages, map_pages;
    bool dmamap;

    /* # FIRST packet of request, filled by @MTU bytes */
    dst_num_sge = roce_sge_advance(send_wr->send_wr.sge, send_wr->send_wr.num_sge, mtu, &sge_idx,
                                   &off, dst_sges);
    pages = roce_sge_pages(dst_sges, dst_num_sge, ctx->para.page_size);
    pkt = roce_netpkt_alloc(ctx, qp, NULL, pages, 0);
    list_add_tail(&pkts, &pkt->entry);

    opcode = IB_OPCODE_SEND_FIRST;
    bth = roce_netpkt_bth(pkt);
    ack = !(psn % ROCE_MAX_OUTGOING_PACKETS);
    roce_netpkt_set_bth(bth, qp_type, opcode, false, 0, destqp, ack, psn);

    map_pages =
        roce_mr_map_sge(ctx, qp, dst_sges, dst_num_sge, roce_netpkt_payload(pkt), true, 0, &dmamap);
    if (map_pages < 0) {
        goto error;
    }
    ROCE_ASSERT(pages == map_pages);

    roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);
    psn = roce_qp_psn_inc(psn);
    payload -= mtu;

    /* # MIDDLE packets of request, filled by @MTU bytes */
    while (payload > mtu) {
        dst_num_sge = roce_sge_advance(send_wr->send_wr.sge, send_wr->send_wr.num_sge, mtu,
                                       &sge_idx, &off, dst_sges);
        pages = roce_sge_pages(dst_sges, dst_num_sge, ctx->para.page_size);
        pkt = roce_netpkt_alloc(ctx, qp, NULL, pages, 0);
        list_add_tail(&pkts, &pkt->entry);

        opcode = IB_OPCODE_SEND_MIDDLE;
        bth = roce_netpkt_bth(pkt);
        ack = !(psn % ROCE_MAX_OUTGOING_PACKETS);
        roce_netpkt_set_bth(bth, qp_type, opcode, false, 0, destqp, ack, psn);

        map_pages = roce_mr_map_sge(ctx, qp, dst_sges, dst_num_sge, roce_netpkt_payload(pkt), true,
                                    0, &dmamap);
        if (map_pages < 0) {
            goto error;
        }
        ROCE_ASSERT(pages == map_pages);

        roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);
        psn = roce_qp_psn_inc(psn);
        payload -= mtu;
    }

    /* # LAST packets of request, filled by remained bytes and padding bytes */
    uint8_t padcnt = (-payload) & 0x3;
    dst_num_sge = roce_sge_advance(send_wr->send_wr.sge, send_wr->send_wr.num_sge, payload,
                                   &sge_idx, &off, dst_sges);
    pages = roce_sge_pages(dst_sges, dst_num_sge, ctx->para.page_size);
    pkt = roce_netpkt_alloc(ctx, qp, send_wr, pages, padcnt);
    list_add_tail(&pkts, &pkt->entry);

    if (inv) {
        opcode = IB_OPCODE_SEND_LAST_WITH_INVALIDATE;
        ib_ieth *ieth = roce_netpkt_ieth(pkt);
        ib_ieth_set_rkey(ieth, send_wr->send_wr.invalidated_rkey);
        struct iovec *iov = &pkt->iovs[ROCE_NETPKT_ETH];
        iov->iov_base = ieth;
        iov->iov_len = IB_IETH_SIZE;
    } else if (imm) {
        opcode = IB_OPCODE_SEND_LAST_WITH_IMMEDIATE;
        ib_immdt *immdt = roce_netpkt_immdt(pkt);
        ib_immdt_set_immdt(immdt, send_wr->send_wr.imm_data);
        struct iovec *iov = &pkt->iovs[ROCE_NETPKT_ETH];
        iov->iov_base = immdt;
        iov->iov_len = IB_IMMDT_SIZE;
    } else {
        opcode = IB_OPCODE_SEND_LAST;
    }

    bool se = send_wr->send_wr.send_flags & ROCE_SEND_SOLICITED;
    bth = roce_netpkt_bth(pkt);
    roce_netpkt_set_bth(bth, qp_type, opcode, se, padcnt, destqp, true, psn);
    map_pages =
        roce_mr_map_sge(ctx, qp, dst_sges, dst_num_sge, roce_netpkt_payload(pkt), true, 0, &dmamap);
    if (map_pages < 0) {
        goto error;
    }
    ROCE_ASSERT(pages == map_pages);

    roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);
    psn = roce_qp_psn_inc(psn);
    qp->req.psn = psn;
    roce_netpkt_pending_multi(ctx, qp, &pkts);

    return;

error:
    roce_netpkt_free_multi(ctx, qp, &pkts);
    send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
}

static inline void roce_rc_send_pkts(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *send_wr,
                                     uint8_t inv, uint8_t imm)
{
    uint16_t mtu = roce_get_mtu(ctx, qp);
    uint32_t payload;

    ROCE_ASSERT(!(inv && imm));

    if (roce_send_wr_is_inline(send_wr)) {
        payload = sizeof(roce_sge) * send_wr->send_wr.num_sge;
    } else {
        payload = roce_sge_len(send_wr->send_wr.sge, send_wr->send_wr.num_sge, 0, 0);
    }

    if (payload <= mtu) {
        /* one single packet is enough, for send only */
        roce_rc_send_only(ctx, qp, send_wr, inv, imm, payload);
    } else {
        /* ROCE_MAX_INLINE_DATA is limited to a single MTU */
        ROCE_ASSERT(!roce_send_wr_is_inline(send_wr));
        roce_rc_send_multi(ctx, qp, send_wr, inv, imm, payload, mtu);
    }
}

static void roce_rc_write_only(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *send_wr, uint8_t imm,
                               uint32_t payload)
{
    roce_netpkt *pkt;
    uint8_t padcnt;
    uint8_t opcode;
    uint32_t psn = qp->req.psn;
    uint32_t destqp = qp->attr.dest_qp_num;
    struct iovec *iov;
    uint32_t pages, map_pages;
    bool dmamap;

    if (roce_send_wr_is_inline(send_wr)) {
        pages = 1;
    } else {
        pages = roce_sge_pages(send_wr->send_wr.sge, send_wr->send_wr.num_sge, ctx->para.page_size);
    }

    padcnt = (-payload) & 0x3;
    pkt = roce_netpkt_alloc(ctx, qp, send_wr, pages, padcnt);
    if (!pkt) {
        send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
        return;
    }

    ib_reth *reth = roce_netpkt_write_reth(pkt);
    ib_reth_set_va(reth, send_wr->send_wr.wr.rdma.remote_addr);
    ib_reth_set_rkey(reth, send_wr->send_wr.wr.rdma.rkey);
    ib_reth_set_dmalen(reth, payload);
    iov = &pkt->iovs[ROCE_NETPKT_ETH];
    iov->iov_base = reth;
    iov->iov_len = IB_RETH_SIZE;

    if (imm) {
        opcode = IB_OPCODE_RDMA_WRITE_ONLY_WITH_IMMEDIATE;
        ib_immdt *immdt = roce_netpkt_write_immdt(pkt);
        ib_immdt_set_immdt(immdt, send_wr->send_wr.imm_data);
        iov->iov_len += IB_IMMDT_SIZE;
    } else {
        opcode = IB_OPCODE_RDMA_WRITE_ONLY;
    }

    if (roce_send_wr_is_inline(send_wr)) {
        iov = roce_netpkt_payload(pkt);
        iov->iov_base = send_wr->send_wr.sge;
        iov->iov_len = payload;
    } else {
        map_pages = roce_mr_map_sge(ctx, qp, send_wr->send_wr.sge, send_wr->send_wr.num_sge,
                                    roce_netpkt_payload(pkt), true, 0, &dmamap);
        if (map_pages < 0) {
            roce_netpkt_free(ctx, pkt);
            send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
            return;
        }
        ROCE_ASSERT(pages == map_pages);
    }

    bool se = !!(send_wr->send_wr.send_flags & ROCE_SEND_SOLICITED);
    ib_bth *bth = roce_netpkt_bth(pkt);
    roce_netpkt_set_bth(bth, qp->qp_type, opcode, se, padcnt, destqp, true, psn);
    roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);

    roce_netpkt_pending_one(ctx, qp, pkt);
    qp->req.psn = roce_qp_psn_inc(psn);
}

static void roce_rc_write_multi(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *send_wr, uint8_t imm,
                                uint32_t payload, uint16_t mtu)
{
    roce_netpkt *pkt;
    ib_bth *bth;
    uint8_t qp_type = qp->qp_type;
    uint8_t opcode;
    bool ack;
    uint32_t destqp = qp->attr.dest_qp_num;
    uint32_t psn = qp->req.psn;
    roce_sge dst_sges[ROCE_MAX_SGE];
    uint32_t sge_idx = 0, off = 0, dst_num_sge;
    LIST_HEAD(pkts);
    uint32_t pages, map_pages;
    struct iovec *iov;
    bool dmamap;

    /* # FIRST packet of request, RETH + @MTU bytes */
    dst_num_sge = roce_sge_advance(send_wr->send_wr.sge, send_wr->send_wr.num_sge, mtu, &sge_idx,
                                   &off, dst_sges);
    pages = roce_sge_pages(dst_sges, dst_num_sge, ctx->para.page_size);
    pkt = roce_netpkt_alloc(ctx, qp, NULL, pages, 0);
    list_add_tail(&pkts, &pkt->entry);

    opcode = IB_OPCODE_RDMA_WRITE_FIRST;
    bth = roce_netpkt_bth(pkt);
    ack = !(psn % ROCE_MAX_OUTGOING_PACKETS);
    roce_netpkt_set_bth(bth, qp_type, opcode, false, 0, destqp, ack, psn);

    ib_reth *reth = roce_netpkt_write_reth(pkt);
    ib_reth_set_va(reth, send_wr->send_wr.wr.rdma.remote_addr);
    ib_reth_set_rkey(reth, send_wr->send_wr.wr.rdma.rkey);
    ib_reth_set_dmalen(reth, payload);
    iov = &pkt->iovs[ROCE_NETPKT_ETH];
    iov->iov_base = reth;
    iov->iov_len = IB_RETH_SIZE;

    map_pages =
        roce_mr_map_sge(ctx, qp, dst_sges, dst_num_sge, roce_netpkt_payload(pkt), true, 0, &dmamap);
    if (map_pages < 0) {
        goto error;
    }
    ROCE_ASSERT(pages == map_pages);

    roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);
    psn = roce_qp_psn_inc(psn);
    payload -= mtu;

    /* # MIDDLE packets of request, filled by @MTU bytes */
    while (payload > mtu) {
        dst_num_sge = roce_sge_advance(send_wr->send_wr.sge, send_wr->send_wr.num_sge, mtu,
                                       &sge_idx, &off, dst_sges);
        pages = roce_sge_pages(dst_sges, dst_num_sge, ctx->para.page_size);
        pkt = roce_netpkt_alloc(ctx, qp, NULL, pages, 0);
        list_add_tail(&pkts, &pkt->entry);

        opcode = IB_OPCODE_RDMA_WRITE_MIDDLE;
        bth = roce_netpkt_bth(pkt);
        ack = !(psn % ROCE_MAX_OUTGOING_PACKETS);
        roce_netpkt_set_bth(bth, qp_type, opcode, false, 0, destqp, ack, psn);

        map_pages = roce_mr_map_sge(ctx, qp, dst_sges, dst_num_sge, roce_netpkt_payload(pkt), true,
                                    0, &dmamap);
        if (map_pages < 0) {
            goto error;
        }
        ROCE_ASSERT(pages == map_pages);

        roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);
        psn = roce_qp_psn_inc(psn);
        payload -= mtu;
    }

    /* # LAST packets of request, filled by remained bytes and padding bytes */
    uint8_t padcnt = (-payload) & 0x3;
    dst_num_sge = roce_sge_advance(send_wr->send_wr.sge, send_wr->send_wr.num_sge, payload,
                                   &sge_idx, &off, dst_sges);
    pages = roce_sge_pages(dst_sges, dst_num_sge, ctx->para.page_size);
    pkt = roce_netpkt_alloc(ctx, qp, send_wr, pages, padcnt);
    list_add_tail(&pkts, &pkt->entry);

    if (imm) {
        opcode = IB_OPCODE_RDMA_WRITE_LAST_WITH_IMMEDIATE;
        ib_immdt *immdt = roce_netpkt_immdt(pkt);
        ib_immdt_set_immdt(immdt, send_wr->send_wr.imm_data);
        iov = &pkt->iovs[ROCE_NETPKT_ETH];
        iov->iov_base = immdt;
        iov->iov_len = IB_IMMDT_SIZE;
    } else {
        opcode = IB_OPCODE_RDMA_WRITE_LAST;
    }

    bool se = send_wr->send_wr.send_flags & ROCE_SEND_SOLICITED;
    bth = roce_netpkt_bth(pkt);
    roce_netpkt_set_bth(bth, qp_type, opcode, se, padcnt, destqp, true, psn);
    map_pages =
        roce_mr_map_sge(ctx, qp, dst_sges, dst_num_sge, roce_netpkt_payload(pkt), true, 0, &dmamap);
    if (map_pages < 0) {
        goto error;
    }
    ROCE_ASSERT(pages == map_pages);

    roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &qp->attr.ah_attr);
    psn = roce_qp_psn_inc(psn);
    qp->req.psn = psn;
    roce_netpkt_pending_multi(ctx, qp, &pkts);

    return;

error:
    roce_netpkt_free_multi(ctx, qp, &pkts);
    send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
}

static inline void roce_rc_write_pkts(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *send_wr,
                                      uint8_t imm)
{
    uint16_t mtu = roce_get_mtu(ctx, qp);
    uint32_t payload;

    if (roce_send_wr_is_inline(send_wr)) {
        payload = sizeof(roce_sge) * send_wr->send_wr.num_sge;
    } else {
        payload = roce_sge_len(send_wr->send_wr.sge, send_wr->send_wr.num_sge, 0, 0);
    }

    if (payload <= mtu) {
        /* one single packet is enough, for write only */
        roce_rc_write_only(ctx, qp, send_wr, imm, payload);
    } else {
        roce_rc_write_multi(ctx, qp, send_wr, imm, payload, mtu);
    }
}

static inline void roce_rc_build_pkts(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *send_wr)
{
    uint8_t opcode = send_wr->send_wr.opcode;
    uint8_t inv = 0, imm = 0;

    if (roce_send_wr_is_inline(send_wr)) {
        /* not a limitation from IB specification, keep libroce simple */
        uint32_t payload = sizeof(roce_sge) * send_wr->send_wr.num_sge;
        if (payload > qp->attr.cap.max_inline_data) {
            send_wr->error = ROCE_WC_LOC_LEN_ERR;
            return;
        }
    }

    switch (opcode) {
    case ROCE_WR_SEND_WITH_IMM:
        imm = IB_IMMDT_SIZE;
        goto send_pkts;

    case ROCE_WR_SEND_WITH_INV:
        inv = IB_IETH_SIZE;
        goto send_pkts;

    send_pkts:
    case ROCE_WR_SEND:
        roce_rc_send_pkts(ctx, qp, send_wr, inv, imm);
        break;

    case ROCE_WR_RDMA_WRITE_WITH_IMM:
        imm = IB_IMMDT_SIZE; /* fallthrough */

    case ROCE_WR_RDMA_WRITE:
        roce_rc_write_pkts(ctx, qp, send_wr, imm);
        break;
    }
}

static inline void roce_xmit_pkts(roce_ctx *ctx, roce_qp *qp)
{
    roce_netpkt *pkt, *tmp_pkt;
    uint32_t psn;
    int32_t diff;

    /* NAK(out of sequence) leads performance drop on too many outgoing packets,
     * the last outgoing packet should between [SQ PSN, SQ PSN + MAX OUTGOING].
     */
    pkt = list_tail(&qp->req.outgoing_pkts, roce_netpkt, entry);
    if (pkt) {
        psn = roce_netpkt_psn(pkt);
        ROCE_ASSERT((psn >= qp->attr.sq_psn) && (psn <= qp->req.psn));
        diff = roce_compare_psn(qp->attr.sq_psn + ROCE_MAX_OUTGOING_PACKETS, psn);
        ROCE_ASSERT(diff >= 0);
        if (!diff) {
            roce_log_debug(ctx,
                           "QP(0x%x) outgoing queue is full. SQ PSN(0x%x), last outgoing PSN(0x%x)",
                           qp->res.handle, qp->attr.sq_psn, psn);
            return;
        }
    }

    /* pick packet from pending queue to outgoing queue, send it via net device */
    list_for_each_safe (&qp->req.pending_pkts, pkt, tmp_pkt, entry) {
        roce_netpkt_outgoing(ctx, qp, pkt);
        roce_netpkt_xmit(ctx, pkt);
        psn = roce_netpkt_psn(pkt);
        diff = roce_compare_psn(qp->attr.sq_psn + ROCE_MAX_OUTGOING_PACKETS, psn);
        if (!diff) {
            break;
        }
    }
}

/* # complete local WR/error WR from head of send WR list */
static void roce_complete_head(roce_ctx *ctx, roce_qp *qp)
{
    _roce_send_wr *send_wr, *tmp_send_wr;

    roce_spin_lock(ctx, &qp->sq.lock);
    list_for_each_safe (&qp->sq.wrs, send_wr, tmp_send_wr, entry) {
        if (roce_send_wr_is_local(send_wr)) {
            list_del(&send_wr->entry);
            roce_spin_unlock(ctx, &qp->sq.lock);
            // TODO handle local WR
            roce_spin_lock(ctx, &qp->sq.lock);
        } else if (send_wr->error) {
            list_del(&send_wr->entry);
            roce_spin_unlock(ctx, &qp->sq.lock);
            // TODO handle error WR
            roce_spin_lock(ctx, &qp->sq.lock);
        } else {
            break;
        }
    }

    roce_spin_unlock(ctx, &qp->sq.lock);
}

static void roce_rc_complete_send_wr(roce_ctx *ctx, roce_qp *qp, roce_netpkt *pkt)
{
    _roce_send_wr *send_wr;

    roce_complete_head(ctx, qp);

    roce_log_debug(ctx, "QP(0x%x) SQ PSN(0x%x) to PSN(0x%x) acknowledged", qp->res.handle,
                   qp->attr.sq_psn, roce_netpkt_psn(pkt));
    qp->attr.sq_psn = roce_qp_psn_inc(qp->attr.sq_psn);

    if (!pkt->opaque) {
        /* this pkt belongs to XXX_FIRST or XXX_MIDDLE */
        return;
    }

    roce_spin_lock(ctx, &qp->sq.lock);
    send_wr = list_pop(&qp->sq.wrs, _roce_send_wr, entry);
    roce_spin_unlock(ctx, &qp->sq.lock);
    ROCE_ASSERT(send_wr == pkt->opaque);

    if (qp->sq_sig_all || (send_wr->send_wr.send_flags & ROCE_SEND_SIGNALED)) {
        roce_wc wc = {0};
        wc.wr_id = send_wr->send_wr.wr_id;
        wc.status = ROCE_WC_SUCCESS;
        wc.byte_len = roce_sge_len(send_wr->send_wr.sge, send_wr->send_wr.num_sge, 0, 0);
        wc.local_qpn = qp->res.handle;
        wc.remote_qpn = qp->attr.dest_qp_num;
        wc.vendor_err = 0;
        wc.pkey_index = 0;
        roce_send_wr_opcode_to_wc(send_wr->send_wr.opcode, &wc.opcode, &wc.wc_flags);
        ctx->para.cq_comp(ctx->para.ctx_opaque, &wc, qp->scq->res.handle, qp->scq->cq_ctx);
    }

    roce_free_send_wr(ctx, &send_wr->send_wr);

    /* the first SEND-WR completes, the next one is local operation? */
    roce_complete_head(ctx, qp);
}

static void roce_handle_ack(roce_ctx *ctx, roce_qp *qp, uint32_t psn)
{
    int32_t diff = roce_compare_psn(qp->attr.sq_psn, psn);
    roce_netpkt *pkt, *tmp_pkt;

    if (diff > 0) {
        roce_log_info(ctx, "QP(0x%x) duplicate ACK PSN(0x%x) less than SQ PSN(0x%x)",
                      qp->res.handle, psn, qp->attr.sq_psn);
        return;
    }

    diff = roce_compare_psn(qp->req.psn, psn);
    if (diff < 0) {
        roce_log_info(ctx, "QP(0x%x) unexpected ACK PSN(0x%x) less than requester PSN(0x%x)",
                      qp->res.handle, psn, qp->req.psn);
        return;
    }

    roce_spin_lock(ctx, &qp->req.lock);
    list_for_each_safe (&qp->req.outgoing_pkts, pkt, tmp_pkt, entry) {
        uint32_t pktpsn = roce_netpkt_psn(pkt);
        diff = roce_compare_psn(pktpsn, psn);
        if (diff > 0) {
            break;
        }

        list_del(&pkt->entry);
        roce_spin_unlock(ctx, &qp->req.lock);

        roce_rc_complete_send_wr(ctx, qp, pkt);
        roce_netpkt_free(ctx, pkt);

        roce_spin_lock(ctx, &qp->req.lock);
    }
    roce_spin_unlock(ctx, &qp->req.lock);
}

static void roce_handle_nak(roce_ctx *ctx, roce_qp *qp, ib_nak_code code, uint32_t psn)
{
    roce_netpkt *pkt;

    roce_log_info(ctx, "QP(0x%x) recv NAK(%s) PSN(0x%x) SQ PSN(0x%x)", qp->res.handle,
                  ib_nak_string(code), psn, qp->attr.sq_psn);
    /* packets of outgoing queue: PSNa, PSNb ... PSNm, PSNn ... PSNy, PSNz
     *                                                  |
     *                                               NAK psn
     * Then: a, PSNa ... PSNm are already received by the remote side.
     *       b, PSNm ... PSNz should be retransferred again.
     *       c, append more packets until ROCE_MAX_OUTGOING_PACKETS.
     */
    roce_handle_ack(ctx, qp, (psn - 1) & IB_BTH_PSN_MASK);

    roce_spin_lock(ctx, &qp->req.lock);
    list_for_each (&qp->req.outgoing_pkts, pkt, entry) {
        roce_netpkt_xmit(ctx, pkt);
        roce_log_info(ctx, "QP(0x%x) retransfer PSN(0x%x)", qp->res.handle,
                      roce_netpkt_psn(pkt)); // XXX remove
    }
    roce_spin_unlock(ctx, &qp->req.lock);

    roce_xmit_pkts(ctx, qp);
}

void roce_handle_acknowledge(roce_ctx *ctx, roce_qp *qp, ib_aeth_syndrome_type syndrome,
                             uint8_t val, uint32_t psn, uint32_t msn)
{

    roce_log_debug(ctx, "QP(0x%x) acknowledge(%s) val(0x%x) MSN(0x%x) PSN(0x%x) SQ PSN(0x%x)",
                   qp->res.handle, ib_syndrome_type_string(syndrome), val, msn, psn,
                   qp->attr.sq_psn);
    switch (syndrome) {
    case IB_AETH_SYNDROME_ACK:
        roce_handle_ack(ctx, qp, psn);
        roce_xmit_pkts(ctx, qp); /* continue the pending net packets */
        break;
    case IB_AETH_SYNDROME_NAK:
        roce_handle_nak(ctx, qp, val, psn);
        break;
    }
}

void roce_request_rc(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *send_wr)
{
    roce_queue_t *sq = &qp->sq;

    roce_spin_lock(ctx, &sq->lock);
    list_add_tail(&sq->wrs, &send_wr->entry);
    roce_spin_unlock(ctx, &sq->lock);

    /* build net packets sequencely */
    if (!send_wr->error) {
        roce_rc_build_pkts(ctx, qp, send_wr);
    }

    /* try to send net packets via net device */
    roce_xmit_pkts(ctx, qp);

    /* try to complete send WR */
    roce_complete_head(ctx, qp);
}

static void roce_ud_send_pkt(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *_send_wr, uint8_t imm,
                             uint32_t payload)
{
    roce_send_wr *send_wr = &_send_wr->send_wr;
    roce_netpkt *pkt;
    uint8_t padcnt;
    uint8_t opcode;
    uint32_t psn = qp->req.psn;
    struct iovec *iov;
    uint32_t pages, map_pages = 0;
    roce_ah *ah;
    uint32_t destqp = send_wr->wr.ud.remote_qpn;
    bool dmamap = false;

    ah = roce_get_ah(ctx, send_wr->wr.ud.ah);
    if (!ah) {
        roce_log_error(ctx, "QP(0x%x) UD send with invalid AH(%d)", qp->res.handle,
                       send_wr->wr.ud.ah);
        return;
    }

    if (roce_send_wr_is_inline(_send_wr)) {
        pages = 1;
    } else {
        pages = roce_sge_pages(send_wr->sge, send_wr->num_sge, ctx->para.page_size);
    }

    padcnt = (-payload) & 0x3;
    pkt = roce_netpkt_alloc(ctx, qp, _send_wr, pages, padcnt);
    if (!pkt) {
        _send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
        goto put_ah;
    }

    ib_deth *deth = roce_netpkt_ud_deth(pkt);
    ib_deth_set_qkey(deth, send_wr->wr.ud.remote_qkey);
    ib_deth_set_srcqp(deth, qp->res.handle);
    iov = &pkt->iovs[ROCE_NETPKT_ETH];
    iov->iov_base = deth;
    iov->iov_len = IB_DETH_SIZE;

    if (imm) {
        opcode = IB_OPCODE_SEND_ONLY_WITH_IMMEDIATE;
        ib_immdt *immdt = roce_netpkt_ud_immdt(pkt);
        ib_immdt_set_immdt(immdt, send_wr->imm_data);
        iov->iov_len += IB_IMMDT_SIZE; /* see roce_netpkt::ud */
    } else {
        opcode = IB_OPCODE_SEND_ONLY;
    }

    if (roce_send_wr_is_inline(_send_wr)) {
        iov = roce_netpkt_payload(pkt);
        iov->iov_base = send_wr->sge;
        iov->iov_len = payload;
    } else {
        map_pages = roce_mr_map_sge(ctx, qp, send_wr->sge, send_wr->num_sge,
                                    roce_netpkt_payload(pkt), true, 0, &dmamap);
        if (map_pages < 0) {
            roce_netpkt_free(ctx, pkt);
            _send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
            goto free_pkt;
        }
        ROCE_ASSERT(pages == map_pages);
    }

    bool se = !!(send_wr->send_flags & ROCE_SEND_SOLICITED);
    ib_bth *bth = roce_netpkt_bth(pkt);
    roce_netpkt_set_bth(bth, qp->qp_type, opcode, se, padcnt, destqp, true, psn);
    roce_netpkt_set_eth(pkt, &ctx->ports[qp->attr.port_num - 1], &ah->attr);

    qp->req.psn = roce_qp_psn_inc(psn);
    roce_netpkt_xmit(ctx, pkt);
    roce_mr_unmap_sge(ctx, dmamap, roce_netpkt_payload(pkt), map_pages);

free_pkt:
    roce_netpkt_free(ctx, pkt);

put_ah:
    roce_put_ah(ctx, send_wr->wr.ud.ah);
}

void roce_request_ud(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *_send_wr)
{
    roce_send_wr *send_wr = &_send_wr->send_wr;
    uint8_t opcode = _send_wr->send_wr.opcode;
    uint8_t imm = 0;
    uint16_t mtu = roce_get_mtu(ctx, qp);
    uint32_t payload;
    roce_wc wc = {0};

    if (_send_wr->error) {
        goto done;
    }

    if (roce_send_wr_is_inline(_send_wr)) {
        payload = sizeof(roce_sge) * send_wr->num_sge;
    } else {
        payload = roce_sge_len(send_wr->sge, send_wr->num_sge, 0, 0);
    }

    if (payload > mtu) {
        _send_wr->error = ROCE_WC_LOC_LEN_ERR;
        roce_log_error(ctx, "QP(0x%x) UD send payload(%d) exceeds MTU(%d)", qp->res.handle, payload,
                       mtu);
        goto done;
    }

    switch (opcode) {
    case ROCE_WR_SEND_WITH_IMM:
        imm = IB_IMMDT_SIZE;
        /* fallthrough */

    case ROCE_WR_SEND:
        roce_ud_send_pkt(ctx, qp, _send_wr, imm, payload);
        wc.byte_len = payload;
        break;

    default:
        roce_log_error(ctx, "QP(0x%x) UD request invalid opcode %s(0x%x)", qp->res.handle,
                       roce_wc_opcode_str(opcode), opcode);
        _send_wr->error = ROCE_WC_LOC_QP_OP_ERR;
    }

done:
    if (qp->sq_sig_all || (send_wr->send_flags & ROCE_SEND_SIGNALED) || _send_wr->error) {
        wc.wr_id = send_wr->wr_id;
        wc.local_qpn = qp->res.handle;
        wc.remote_qpn = qp->attr.dest_qp_num;
        wc.vendor_err = 0;
        wc.pkey_index = 0;
        wc.status = _send_wr->error;
        roce_send_wr_opcode_to_wc(send_wr->opcode, &wc.opcode, &wc.wc_flags);
        ctx->para.cq_comp(ctx->para.ctx_opaque, &wc, qp->scq->res.handle, qp->scq->cq_ctx);
    }

    roce_free_send_wr(ctx, send_wr);
}
