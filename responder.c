/*
 * RDMA responder implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "spec/ethernet-spec.h"
#include "spec/ip-spec.h"
#include "spec/ipv6-spec.h"
#include "spec/udp-spec.h"
#include "spec/ib-spec.h"

#include "private/roce-private.h"
#include "private/iov-helper.h"
#include "private/mm.h"
#include "private/lock.h"
#include "private/log.h"
#include "private/sge-helper.h"
#include "private/crc.h"
#include "private/netpkt.h"

static int roce_net_decap(roce_ctx *ctx, struct iovec *iovs, uint32_t num_iov, ib_bth *bth,
                          void *ip, bool *is_ipv4)
{
    roce_port *port;
    ethernet_hdr ethh;
    udp_hdr udph;
    uint32_t length, pkt_icrc, cal_icrc;
    uint8_t vneth[ROCE_MAX_VNET_HDR];
    uint8_t ihlbuf[IP_IHL_MASK << 2];
    uint8_t ihllen = 0;
    uint8_t vnethdr_len = roce_vnet_hdr(ctx, 1);

    /* # decap vnet header */
    length = roce_iov_advance(iovs, num_iov, vnethdr_len, vneth);
    if (length != vnethdr_len) {
        roce_log_info(ctx, "Corrupted vnet header: %d", length);
        return -EPROTO;
    }

    /* # decap ethernet header */
    length = roce_iov_advance(iovs, num_iov, sizeof(ethh), &ethh);
    if (length != sizeof(ethh)) {
        roce_log_info(ctx, "Corrupted ethernet header: %d", length);
        return -EPROTO;
    }

    port = &ctx->ports[0];
    if (!roce_mac_matched(port->mac, ethh.dmac)) {
        char port_mac[19] = {0};
        char dmac[19] = {0};
        roce_mac_format(port->mac, port_mac);
        roce_mac_format(ethh.dmac, dmac);
        roce_log_debug(ctx, "Mismatched MAC, port %s, dmac %s\n", port_mac, dmac);
        return -EPROTO;
    }

    /* # decap IP/IPv6 header */
    switch (ethernet_hdr_get_type(&ethh)) {
    case ETHERNET_IP:
        *is_ipv4 = true;
        length = roce_iov_advance(iovs, num_iov, sizeof(ip_hdr), ip);
        if (length != sizeof(ip_hdr)) {
            roce_log_info(ctx, "Corrupted IPv4 header: %d", length);
            return -EPROTO;
        }

        ihllen = (ip_hdr_get_ihl(ip) << 2) - sizeof(ip_hdr);
        if (ihllen) {
            /* unlikely branch, discard IHL */
            roce_iov_advance(iovs, num_iov, ihllen, ihlbuf);
        }

        break;
    case ETHERNET_IPv6:
        *is_ipv4 = false;
        length = roce_iov_advance(iovs, num_iov, sizeof(ipv6_hdr), ip);
        if (length != sizeof(ipv6_hdr)) {
            roce_log_info(ctx, "Corrupted IPv6 header: %d", length);
            return -EPROTO;
        }
        /* XXX handle ipv6_hdr_get_nexthdr */
        break;
    default:
        return -EPROTO;
    }

    /* # decap UDP header */
    length = roce_iov_advance(iovs, num_iov, sizeof(udph), &udph);
    if (length != sizeof(udph)) {
        roce_log_warn(ctx, "Corrupted UDP header: %d", length);
        return -EPROTO;
    }

    if (udp_hdr_get_dport(&udph) != ROCE_V2_UDP_DPORT) {
        roce_log_info(ctx, "Not RoCE packet: port(%d)", udp_hdr_get_dport(&udph));
        return -EPROTO;
    }

    if (udp_hdr_get_length(&udph) != length + roce_iov_len(iovs, num_iov)) {
        roce_log_warn(ctx, "Corrupted UDP packet length");
        return -EPROTO;
    }

    /* # advance IB BTH, save it into @bth */
    length = roce_iov_advance(iovs, num_iov, sizeof(ib_bth), bth);
    if (length != sizeof(ib_bth)) {
        roce_log_error(ctx, "Corrupted BTH: %d", length);
        return -EPROTO;
    }

    /* # decap ICRC */
    length = roce_iov_reverse(iovs, num_iov, sizeof(pkt_icrc), &pkt_icrc);
    if (length != sizeof(pkt_icrc)) {
        roce_log_error(ctx, "ICRC no found: %d", length);
        return -EPROTO;
    }

    cal_icrc = roce_crc32_pkt(*is_ipv4, ip, ihlbuf, ihllen, &udph, bth, iovs, num_iov);
    if (cal_icrc != pkt_icrc) {
        roce_log_error(ctx, "Mismatched ICRC");
        return -EPROTO;
    }

    return 0;
}

/* Pop ETH of @size in bytes */
static inline int roce_get_eth(roce_ctx *ctx, struct iovec *iovs, uint32_t num_iov, ib_bth *bth,
                               roce_qp *qp, void *eth, uint32_t size)
{
    uint32_t buf_len = roce_iov_len(iovs, num_iov);

    if (buf_len < size) {
        return -EPROTO;
    }

    roce_iov_advance(iovs, num_iov, size, eth);
    return 0;
}

static inline _roce_recv_wr *__roce_pop_recv_wr(roce_ctx *ctx, roce_qp *qp)
{
    _roce_recv_wr *recv_wr;

    roce_spin_lock(ctx, &qp->rq.lock);
    recv_wr = list_pop(&qp->rq.wrs, _roce_recv_wr, entry);
    roce_spin_unlock(ctx, &qp->rq.lock);

    if (!recv_wr) {
        // TODO RNR NAK
        roce_log_warn(ctx, "QP(0x%x) has no recv WR", qp->res.handle);
        return NULL;
    }

    return recv_wr;
}

static inline _roce_recv_wr *roce_pop_recv_wr(roce_ctx *ctx, roce_qp *qp, bool inprocess)
{
    if (inprocess != !!qp->resp.wr) {
        const char *missing = qp->resp.wr ? "LAST" : "FIRST";
        roce_log_warn(ctx, "QP(0x%x) missing SEND %s", qp->res.handle, missing);
        return NULL;
    }

    if (qp->resp.wr) {
        return qp->resp.wr;
    }

    qp->resp.wr = __roce_pop_recv_wr(ctx, qp);

    return qp->resp.wr;
}

static inline bool roce_recv_cq_comp(roce_ctx *ctx, roce_qp *qp, uint8_t opcode, uint64_t wr_id,
                                     uint32_t byte_len, void *eth, roce_wc_status status)
{
    roce_wc wc = {0};
    bool complete;

    if (status != ROCE_WC_SUCCESS) {
        goto force_comp;
    }

    roce_recv_wr_opcode_to_wc(opcode, &wc.opcode, &wc.wc_flags, &complete);
    if (!complete) {
        return false;
    }

    if (wc.wc_flags & ROCE_WC_WITH_IMM) {
        if (eth) {
            wc.imm_data = ib_immdt_get_immdt(eth);
        } else {
            wc.status = ROCE_WC_BAD_RESP_ERR;
            goto force_comp;
        }
    }

    wc.byte_len = byte_len;
    if ((qp->qp_type == ROCE_QPT_GSI) || (qp->qp_type == ROCE_QPT_UD)) {
        wc.wc_flags |= ROCE_WC_GRH;
    }

force_comp:
    wc.wr_id = wr_id;
    wc.status = status;
    wc.local_qpn = qp->res.handle;
    wc.remote_qpn = qp->attr.dest_qp_num;
    ctx->para.cq_comp(ctx->para.ctx_opaque, &wc, qp->rcq->res.handle, qp->rcq->cq_ctx);

    return true;
}

static int roce_recv_payload(roce_ctx *ctx, roce_qp *qp, struct iovec *iovs, uint32_t num_iov,
                             ib_bth *bth, void *eth, bool inprocess, uint8_t opcode)
{
    uint32_t buf_len = roce_iov_len(iovs, num_iov);
    uint32_t sge_len;
    bool dmamap;

    /* # recv WR */
    _roce_recv_wr *recv_wr = roce_pop_recv_wr(ctx, qp, inprocess);
    if (!recv_wr) {
        roce_log_warn(ctx, "QP(0x%x) has no recv WR", qp->res.handle);
        return -EAGAIN;
    }

    /* # verify recv WR length */
    sge_len = roce_sge_len(recv_wr->recv_wr.sge, recv_wr->recv_wr.num_sge, recv_wr->sge_idx,
                           recv_wr->sge_off);
    if (sge_len < buf_len) {
        roce_log_warn(ctx, "QP(0x%x) recv buf(%d) exceeds recv WR(%d of %d)", qp->res.handle,
                      buf_len, sge_len,
                      roce_sge_len(recv_wr->recv_wr.sge, recv_wr->recv_wr.num_sge, 0, 0));
        return -EFAULT;
    }

    /* # map SGE to local iovec by MR */
    roce_sge dst_sges[ROCE_MAX_SGE];
    uint32_t dst_num_sge = roce_sge_advance(recv_wr->recv_wr.sge, recv_wr->recv_wr.num_sge, buf_len,
                                            &recv_wr->sge_idx, &recv_wr->sge_off, dst_sges);

    /* struct iovec dst_iovs[roce_sge_pages(dst_sges, dst_num_sge, ctx->para.page_size)];
     * this would be better, but compiler does not allow */
    struct iovec dst_iovs[ROCE_MAX_SGE * 2];
    int dst_num_iov = roce_mr_map_sge(ctx, qp, dst_sges, dst_num_sge, dst_iovs, true,
                                      ROCE_ACCESS_LOCAL_WRITE, &dmamap);
    if (dst_num_iov < 0) {
        roce_log_error(ctx, "QP(0x%x) map recv SGE failed", qp->res.handle);
        return -EACCES;
    }

    ROCE_ASSERT(dst_num_iov == roce_sge_pages(dst_sges, dst_num_sge, ctx->para.page_size));
    /* # copy payload */
    for (uint32_t i = 0; i < dst_num_iov; i++) {
        struct iovec *dst_iov = &dst_iovs[i];
        roce_iov_advance(iovs, num_iov, dst_iov->iov_len, dst_iov->iov_base);
    }

    roce_log_debug(ctx, "QP(0x%x) received buf(%d), new buf(%d)", qp->res.handle, recv_wr->byte_len,
                   buf_len);
    roce_mr_unmap_sge(ctx, dmamap, dst_iovs, dst_num_iov);
    recv_wr->byte_len += buf_len;

    bool complete = roce_recv_cq_comp(ctx, qp, opcode, recv_wr->recv_wr.wr_id, recv_wr->byte_len,
                                      eth, ROCE_WC_SUCCESS);
    if (complete) {
        roce_free_recv_wr(ctx, &recv_wr->recv_wr);
        qp->resp.wr = NULL;
    }

    return 0;
}

static int roce_rdma_write_payload(roce_ctx *ctx, roce_qp *qp, struct iovec *iovs, uint32_t num_iov,
                                   ib_bth *bth, ib_reth *reth, void *eth, bool inprocess,
                                   uint8_t opcode)
{
    uint32_t buf_len = roce_iov_len(iovs, num_iov);
    int ret = -EACCES;
    bool dmamap;

    /* # incoming RDMA WRITE MR */
    if (inprocess != !!qp->resp.writtenlen) {
        const char *missing = qp->resp.writtenlen ? "LAST" : "FIRST";

        roce_log_warn(ctx, "QP(0x%x) missing WRITE %s", qp->res.handle, missing);
        roce_netpkt_acknowledge(ctx, qp, IB_AETH_SYNDROME_NAK, IB_NAK_INVALID_REQUEST,
                                qp->attr.rq_psn);
        ret = -EPROTO;
        goto error;
    }

    if (!qp->resp.writtenlen) {
        qp->resp.iova = ib_reth_get_va(reth);
        qp->resp.rkey = ib_reth_get_rkey(reth);
        qp->resp.dmalen = ib_reth_get_dmalen(reth);
        qp->resp.writtenlen = 0;
    }

    struct iovec dst_iovs[ROCE_MAX_SGE * 2];
    int dst_num_iov;
    roce_sge sge = {.addr = qp->resp.iova + qp->resp.writtenlen,
                    .length = buf_len,
                    .lkey = qp->resp.rkey};
    dst_num_iov =
        roce_mr_map_sge(ctx, qp, &sge, 1, dst_iovs, false, ROCE_ACCESS_REMOTE_WRITE, &dmamap);
    if (dst_num_iov < 0) {
        roce_log_error(ctx, "QP(0x%x) map RDMA WRITE SGE failed", qp->res.handle);
        goto error;
    }

    ROCE_ASSERT(dst_num_iov == roce_sge_pages(&sge, 1, ctx->para.page_size));
    for (uint32_t i = 0; i < dst_num_iov; i++) {
        struct iovec *dst_iov = &dst_iovs[i];
        roce_iov_advance(iovs, num_iov, dst_iov->iov_len, dst_iov->iov_base);
    }

    roce_log_debug(ctx, "QP(0x%x) RDMA WRITE buf(%d), new buf(%d)", qp->res.handle,
                   qp->resp.writtenlen, buf_len);
    roce_mr_unmap_sge(ctx, dmamap, dst_iovs, dst_num_iov);
    qp->resp.writtenlen += buf_len;

    uint64_t wr_id = 0;
    _roce_recv_wr *recv_wr;
    if ((opcode == IB_OPCODE_RDMA_WRITE_ONLY_WITH_IMMEDIATE) ||
        (opcode == IB_OPCODE_RDMA_WRITE_LAST_WITH_IMMEDIATE)) {
        recv_wr = __roce_pop_recv_wr(ctx, qp);
        wr_id = recv_wr->recv_wr.wr_id;
    }
    bool complete =
        roce_recv_cq_comp(ctx, qp, opcode, wr_id, qp->resp.writtenlen, eth, ROCE_WC_SUCCESS);
    if (complete) {
        roce_free_recv_wr(ctx, &recv_wr->recv_wr);
        qp->resp.wr = NULL;
        qp->resp.writtenlen = 0;
    }

    return 0;

error:
    roce_set_qp_state(ctx, qp, ROCE_QPS_ERR);
    return ret;
}

static inline void roce_out_of_seq(roce_ctx *ctx, roce_qp *qp, uint32_t psn)
{
    roce_log_info(ctx, "QP(0x%x) out of sequence. RQ PSN(0x%x), PSN(0x%x)", qp->res.handle,
                  qp->attr.rq_psn, psn);
    if (!qp->resp.in_psn_nak) {
        roce_log_notice(ctx, "QP(0x%x) send NAK at RQ PSN(0x%x)", qp->res.handle, qp->attr.rq_psn);
        roce_netpkt_acknowledge(ctx, qp, IB_AETH_SYNDROME_NAK, IB_NAK_PSN_SEQUENCE_ERROR,
                                qp->attr.rq_psn);
        qp->resp.in_psn_nak = true;
    }
}

static inline void roce_duplicated_request(roce_ctx *ctx, roce_qp *qp, uint32_t psn)
{
    roce_log_info(ctx, "QP(0x%x) duplicated request. RQ PSN(0x%x), PSN(0x%x)", qp->res.handle,
                  qp->attr.rq_psn, psn);
}

static int roce_recv_rc(roce_ctx *ctx, struct iovec *iovs, uint32_t num_iov, ib_bth *bth,
                        roce_qp *qp)
{
    uint32_t psn = ib_bth_get_psn(bth);
    uint8_t opcode = ib_bth_get_opcode(bth);
    ib_reth reth;
    ib_immdt immdt;
    ib_aeth aeth;
    int32_t psn_diff;
    int ret;

    if (opcode != IB_OPCODE_ACKNOWLEDGE) {
        psn_diff = roce_compare_psn(qp->attr.rq_psn, psn);
        if (psn_diff > 0) {
            roce_duplicated_request(ctx, qp, psn);
            return 0;
        } else if (psn_diff < 0) {
            /* Requester Fault Behavior Class A */
            roce_out_of_seq(ctx, qp, psn);
            return 0;
        }

        qp->resp.in_psn_nak = false;
    }

    roce_log_debug(ctx, "QP(0x%x) recv %s, PSN(0x%x)", qp->res.handle, ib_opcode_str(opcode), psn);
    switch (opcode) {
    case IB_OPCODE_SEND_FIRST:
        ret = roce_recv_payload(ctx, qp, iovs, num_iov, bth, NULL, false, opcode);
        break;

    case IB_OPCODE_SEND_MIDDLE:
        ret = roce_recv_payload(ctx, qp, iovs, num_iov, bth, NULL, true, opcode);
        break;

    case IB_OPCODE_SEND_LAST_WITH_IMMEDIATE:
        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &immdt, IB_IMMDT_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) has corrupted IMMDT from packet(%s)", qp->res.handle,
                           ib_opcode_str(opcode));
            break; // XXX handle error
        }
        ret = roce_recv_payload(ctx, qp, iovs, num_iov, bth, &immdt, true, opcode);
        break;

    case IB_OPCODE_SEND_LAST:
        ret = roce_recv_payload(ctx, qp, iovs, num_iov, bth, NULL, true, opcode);
        break;

    case IB_OPCODE_SEND_ONLY_WITH_IMMEDIATE:
        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &immdt, IB_IMMDT_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) has corrupted IMMDT (%s)", qp->res.handle,
                           ib_opcode_str(opcode));
            break; // XXX handle error
        }
        ret = roce_recv_payload(ctx, qp, iovs, num_iov, bth, &immdt, false, opcode);
        break;

    case IB_OPCODE_SEND_ONLY:
        ret = roce_recv_payload(ctx, qp, iovs, num_iov, bth, NULL, false, opcode);
        break;

    case IB_OPCODE_RDMA_WRITE_ONLY:
        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &reth, IB_RETH_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) corrupted RETH (%s)", qp->res.handle,
                           ib_opcode_str(opcode));
            break; // XXX handle error
        }
        ret = roce_rdma_write_payload(ctx, qp, iovs, num_iov, bth, &reth, NULL, false, opcode);
        break;

    case IB_OPCODE_RDMA_WRITE_ONLY_WITH_IMMEDIATE:
        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &reth, IB_RETH_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) corrupted RETH (%s)", qp->res.handle,
                           ib_opcode_str(opcode));
            break; // XXX handle error
        }

        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &immdt, IB_IMMDT_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) corrupted IMMDT (%s)", qp->res.handle,
                           ib_opcode_str(opcode));
            break; // XXX handle error
        }

        ret = roce_rdma_write_payload(ctx, qp, iovs, num_iov, bth, &reth, &immdt, false, opcode);
        break;

    case IB_OPCODE_RDMA_WRITE_FIRST:
        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &reth, IB_RETH_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) corrupted RETH (%s)", qp->res.handle,
                           ib_opcode_str(opcode));
            break; // XXX handle error
        }
        ret = roce_rdma_write_payload(ctx, qp, iovs, num_iov, bth, &reth, NULL, false, opcode);
        break;

    case IB_OPCODE_RDMA_WRITE_MIDDLE:
        ret = roce_rdma_write_payload(ctx, qp, iovs, num_iov, bth, NULL, NULL, true, opcode);
        break;

    case IB_OPCODE_RDMA_WRITE_LAST:
        ret = roce_rdma_write_payload(ctx, qp, iovs, num_iov, bth, NULL, NULL, true, opcode);
        break;

    case IB_OPCODE_RDMA_WRITE_LAST_WITH_IMMEDIATE:
        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &immdt, IB_IMMDT_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) has corrupted IMMDT (%s)", qp->res.handle,
                           ib_opcode_str(opcode));
            break; // XXX handle error
        }
        ret = roce_rdma_write_payload(ctx, qp, iovs, num_iov, bth, NULL, &immdt, true, opcode);
        break;

    case IB_OPCODE_ACKNOWLEDGE:
        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &aeth, IB_AETH_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) has corrupted AETH", qp->res.handle);
            break; // XXX handle error
        }

        ib_aeth_syndrome_type syndrome = ib_aeth_get_syndrome_type(&aeth);
        uint8_t val = ib_aeth_get_syndrome_val(&aeth);
        uint32_t msn = ib_aeth_get_msn(&aeth);
        roce_handle_acknowledge(ctx, qp, syndrome, val, psn, msn);
        return 0;

    default:
        roce_log_warn(ctx, "QP(0x%x) unsupported opcode %x", qp->res.handle, opcode);
        ret = -EPROTO;
    }

    if (ret) {
        return ret;
    }

    if (ib_bth_get_ack(bth)) {
        roce_netpkt_acknowledge(ctx, qp, IB_AETH_SYNDROME_ACK, 0x1f, psn);
    }

    qp->attr.rq_psn = roce_qp_psn_inc(qp->attr.rq_psn);

    return ret;
}

static void roce_build_grh(struct iovec *iov, void *ip, bool is_ipv4)
{
    uint8_t *grh = ip;

    if (is_ipv4) {
        /* move IPv4 from head 20 bytes to bottom 20 bytes, then clear head 20 bytes */
        memmove(grh + (IPV6_HDR_SIZE - IP_HDR_SIZE), grh, IP_HDR_SIZE);
        memset(grh, 0x00, (IPV6_HDR_SIZE - IP_HDR_SIZE));
    } else {
        /* do nothing */
    }

    iov->iov_len = sizeof(ipv6_hdr);
    iov->iov_base = ip;
}

static int roce_recv_ud(roce_ctx *ctx, struct iovec *iovs, uint32_t num_iov, ib_bth *bth,
                        roce_qp *qp, void *ip, bool is_ipv4)
{
    uint32_t psn = ib_bth_get_psn(bth);
    uint8_t opcode = ib_bth_get_opcode(bth);
    ib_deth deth;
    ib_immdt immdt;
    int ret;

    roce_log_debug(ctx, "QP(0x%x) recv %s, PSN(0x%x)", qp->res.handle, ib_opcode_str(opcode), psn);
    ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &deth, IB_DETH_SIZE);
    if (ret) {
        roce_log_error(ctx, "QP(0x%x) has corrupted DETH", qp->res.handle);
        return -EPROTO;
    }

    switch (opcode) {
    case IB_OPCODE_SEND_ONLY_WITH_IMMEDIATE:
        ret = roce_get_eth(ctx, iovs, num_iov, bth, qp, &immdt, IB_IMMDT_SIZE);
        if (ret) {
            roce_log_error(ctx, "QP(0x%x) has corrupted IMMDT (%s)", qp->res.handle,
                           ib_opcode_str(opcode));
            break;
        }
        roce_build_grh(--iovs, ip, is_ipv4);
        ret = roce_recv_payload(ctx, qp, iovs, num_iov + 1, bth, &immdt, false, opcode);
        break;

    case IB_OPCODE_SEND_ONLY:
        roce_build_grh(--iovs, ip, is_ipv4);
        ret = roce_recv_payload(ctx, qp, iovs, num_iov + 1, bth, NULL, false, opcode);
        break;

    default:
        roce_log_warn(ctx, "QP(0x%x) unsupported opcode %x", qp->res.handle, opcode);
        return -EPROTO;
    }

    qp->attr.dest_qp_num = ib_deth_get_srcqp(&deth);
    if (ret) {
        return ret;
    }

    qp->attr.rq_psn = roce_qp_psn_inc(qp->attr.rq_psn);

    return ret;
}

static int roce_recv_bth(roce_ctx *ctx, struct iovec *iovs, uint32_t num_iov, ib_bth *bth, void *ip,
                         bool is_ipv4)
{
    roce_qp *qp;
    uint32_t destqp;
    uint8_t qp_type;
    int ret = 0;

    destqp = ib_bth_get_destqp(bth);
    qp = roce_get_qp(ctx, destqp);
    if (!qp) {
        roce_log_warn(ctx, "QP(0x%x) not found", destqp);
        return -EPROTO;
    }

    if (destqp == ROCE_QPT_SMI) {
        roce_log_warn(ctx, "SMI is not supported");
        return -ENOTSUP;
    } else if (destqp == ROCE_QPT_GSI) {
        if (qp->qp_type == ROCE_QPT_MAX) {
            roce_log_warn(ctx, "GSI is unavailable");
            return -EAGAIN;
        }
    }

    ROCE_ASSERT(destqp == qp->res.handle);

    switch (qp->attr.qp_state) {
    case ROCE_QPS_RTR:
    case ROCE_QPS_RTS:
    case ROCE_QPS_SQD:
    case ROCE_QPS_SQE:
        break;
    case ROCE_QPS_RESET:
    case ROCE_QPS_INIT:
    case ROCE_QPS_ERR:
    default:
        roce_log_warn(ctx, "QP(0x%x) can't recv because of %s state", qp->res.handle,
                      roce_qp_state_str(qp->attr.qp_state));
        ret = -EACCES;
        goto put_qp;
    }

    if (ib_bth_get_pkey(bth) != ROCE_PKEY) {
        roce_log_warn(ctx, "PKEY 0x%x is not supported", ib_bth_get_pkey(bth));
        ret = -ENOTSUP;
        goto put_qp;
    }

    /* verify QP type */
    qp_type = ib_bth_get_qp_type(bth);
    if (qp_type != roce_qp_type_to_ib(qp->qp_type)) {
        roce_log_warn(ctx, "QP type(%s) mismatch (%s)",
                      roce_qp_type_str(roce_qp_type_to_ib(qp->qp_type)), roce_qp_type_str(qp_type));
        ret = -EPROTO;
        goto put_qp;
    }

    /* # discard pad payload from the tail */
    uint8_t padcnt = ib_bth_get_padcnt(bth);
    if (padcnt) {
        uint32_t buf_len = roce_iov_len(iovs, num_iov);
        if (buf_len < padcnt) {
            roce_log_warn(ctx, "QP(0x%x) has corrupted PAD", qp->res.handle);
            ret = -EPROTO;
            goto put_qp;
        }

        roce_iov_reverse(iovs, num_iov, padcnt, NULL);
    }

    switch (qp->qp_type) {
    case ROCE_QPT_RC:
        ret = roce_recv_rc(ctx, iovs, num_iov, bth, qp);
        break;

    case ROCE_QPT_UD:
    case ROCE_QPT_GSI:
        ret = roce_recv_ud(ctx, iovs, num_iov, bth, qp, ip, is_ipv4);
        break;

    default:
        ROCE_ASSERT(!"BUG on unsupported QP type from inbound request");
    }

put_qp:
    roce_put_qp(ctx, destqp);
    return ret;
}

int roce_net_recv(roce_ctx *ctx, uint8_t queue, const struct iovec *_iovs, uint32_t num_iov)
{
    struct iovec iovs[num_iov + 1];
    ipv6_hdr ip; /* because sizeof(ipv6_hdr) > sizeof(ip_hdr), this may contain IPv4 or IPv6 */
    ib_bth bth;
    bool is_ipv4;
    int ret;

    roce_log_debug(ctx, "recv net: %d", roce_iov_len(_iovs, num_iov));

    /* reserver iovs[0], it's used by GRH for GSI/UD */
    iovs[0].iov_base = NULL;
    iovs[0].iov_len = 0;
    roce_iov_dup(_iovs, &iovs[1], num_iov);
    ret = roce_net_decap(ctx, &iovs[1], num_iov, &bth, &ip, &is_ipv4);
    if (ret) {
        return ret;
    }

    ret = roce_recv_bth(ctx, &iovs[1], num_iov, &bth, &ip, is_ipv4);
    if (ret < 0) {
        return ret;
    }

    return roce_iov_len(_iovs, num_iov);
}
