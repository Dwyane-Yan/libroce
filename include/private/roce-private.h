/*
 * Internal structures/API of libroce.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_ROCE_PRIVATE_H
#define LIBROCE_ROCE_PRIVATE_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/uio.h>

#include "spec/ethernet-spec.h"
#include "spec/ip-spec.h"
#include "spec/ipv6-spec.h"
#include "spec/udp-spec.h"
#include "spec/ib-spec.h"

#include "roce/roce.h"

#include "private/types.h"
#include "private/list.h"
#include "private/resource.h"

typedef struct roce_pd {
    roce_res res;
    void *pd_ctx;
} roce_pd;

typedef struct roce_ah {
    roce_res res;
    roce_pd *pd;
    roce_ah_attr attr;
    void *ah_ctx;
} roce_ah;

typedef struct roce_cq {
    roce_res res;
    uint32_t cqe;
    uint32_t comp_vector;
    void *cq_ctx;
} roce_cq;

typedef struct roce_srq {
    roce_res res;
    void *srq_ctx;
} roce_srq;

typedef enum roce_mr_state_t {
    ROCE_MR_STATE_INVALID,
    ROCE_MR_STATE_FREE,
    ROCE_MR_STATE_VALID,

    ROCE_MR_STATE_DESTROYED,
    ROCE_MR_STATE_MAX
} roce_mr_state_t;

typedef struct roce_mr {
    roce_res res;
    void *mr_ctx;
    roce_pd *pd;
    uint8_t state; /* roce_mr_state_t */
    uint8_t type;  /* roce_mr_type */
    uint64_t iova;
    uint32_t length;
    uint32_t lkey;
    uint32_t rkey;
    uint32_t access; /* mask of roce_access_flags */
    /* larger page size(huge page) is supported, the page size of MR must
     * power of context page size. Use context page size by default on 0.
     */
    uint32_t page_size;

    uint32_t num_iov;
    struct iovec iovs[0];
} roce_mr;

static inline uint32_t roce_mr_size(uint32_t num_iov)
{
    return sizeof(roce_mr) + sizeof(struct iovec) * num_iov;
}

static inline const char *roce_mr_type_str(uint8_t mr_type)
{
    static const char *mr_type_str[] = {"MEM", "FRMR", "DMA"};

    if (mr_type >= ARRAY_SIZE(mr_type_str)) {
        return "UNKNOWN";
    }

    return mr_type_str[mr_type];
}

typedef struct _roce_recv_wr {
    struct list_node entry;
    uint32_t byte_len;
    uint32_t sge_idx;
    uint32_t sge_off;
    roce_recv_wr recv_wr; /* keep last */
} _roce_recv_wr;

static inline _roce_recv_wr *roce_recv_wr_to_priv(roce_recv_wr *recv_wr)
{
    return container_of(recv_wr, _roce_recv_wr, recv_wr);
}

typedef struct _roce_send_wr {
    roce_spin_lock_t lock;
    uint8_t error;
    bool mapped;
    struct list_node entry; /* list entry for SQ */
    /* keep last because of mutable length array sge[0] */
    roce_send_wr send_wr;
} _roce_send_wr;

static inline _roce_send_wr *roce_send_wr_to_priv(roce_send_wr *send_wr)
{
    return container_of(send_wr, _roce_send_wr, send_wr);
}

static inline bool roce_send_wr_is_inline(_roce_send_wr *_send_wr)
{
    return !!(_send_wr->send_wr.send_flags & ROCE_SEND_INLINE);
}

static inline bool roce_send_wr_is_local(_roce_send_wr *_send_wr)
{
    switch (_send_wr->send_wr.opcode) {
    case ROCE_WR_LOCAL_INV:
    case ROCE_WR_REG_MR:
    case ROCE_WR_BIND_MW:
        return true;
    default:
        return false;
    }
}

static inline void roce_send_wr_opcode_to_wc(roce_wr_opcode wr_opcode, roce_wc_opcode *wc_opcode,
                                             uint16_t *wc_flags)
{
    typedef struct roce_send_wr_to_wc {
        roce_wc_opcode opcode;
        roce_wc_flags flags;
    } roce_send_wr_to_wc;

    static roce_send_wr_to_wc roce_send_wr_to_wc_tbl[] = {
        [ROCE_WR_SEND] = {.opcode = ROCE_WC_SEND, .flags = 0},
        [ROCE_WR_SEND_WITH_IMM] = {.opcode = ROCE_WC_SEND, .flags = ROCE_WC_WITH_IMM},
        [ROCE_WR_SEND_WITH_INV] = {.opcode = ROCE_WC_SEND, .flags = ROCE_WC_WITH_INVALIDATE},
        [ROCE_WR_RDMA_WRITE] = {.opcode = ROCE_WC_RDMA_WRITE, .flags = 0},
        [ROCE_WR_RDMA_WRITE_WITH_IMM] = {.opcode = ROCE_WC_RDMA_WRITE, .flags = ROCE_WC_WITH_IMM},
        [ROCE_WR_RDMA_READ] = {.opcode = ROCE_WC_RDMA_READ, .flags = 0},
        [ROCE_WR_ATOMIC_CMP_AND_SWP] = {.opcode = ROCE_WC_COMP_SWAP, .flags = 0},
        [ROCE_WR_ATOMIC_FETCH_AND_ADD] = {.opcode = ROCE_WC_FETCH_ADD, .flags = 0},
        [ROCE_WR_ATOMIC_WRITE] = {.opcode = ROCE_WC_ATOMIC_WRITE, .flags = 0},
        [ROCE_WR_FLUSH] = {.opcode = ROCE_WC_FLUSH, .flags = 0},
        [ROCE_WR_LOCAL_INV] = {.opcode = ROCE_WC_LOCAL_INV, .flags = 0},
        [ROCE_WR_REG_MR] = {.opcode = ROCE_WC_REG_MR, .flags = 0},
        [ROCE_WR_BIND_MW] = {.opcode = ROCE_WC_BIND_MW, .flags = 0},
    };

    *wc_opcode = roce_send_wr_to_wc_tbl[wr_opcode].opcode;
    *wc_flags |= roce_send_wr_to_wc_tbl[wr_opcode].flags;
}

static inline void roce_recv_wr_opcode_to_wc(uint8_t ib_opcode, roce_wc_opcode *wc_opcode,
                                             uint16_t *wc_flags, bool *complete)
{
    typedef struct _roce_recv_wr_to_wc {
        uint8_t opcode; /* roce_wc_opcode */
        bool complete;
        roce_wc_flags flags;
    } _roce_recv_wr_to_wc;

    static _roce_recv_wr_to_wc _roce_recv_wr_to_wc_tbl[IB_OPCODE_MASK] = {
        [IB_OPCODE_SEND_LAST] = {.opcode = ROCE_WC_RECV, .complete = true, .flags = 0},
        [IB_OPCODE_SEND_LAST_WITH_IMMEDIATE] = {.opcode = ROCE_WC_RECV,
                                                .complete = true,
                                                .flags = ROCE_WC_WITH_IMM},
        [IB_OPCODE_SEND_ONLY] = {.opcode = ROCE_WC_RECV, .complete = true, .flags = 0},
        [IB_OPCODE_SEND_ONLY_WITH_IMMEDIATE] = {.opcode = ROCE_WC_RECV,
                                                .complete = true,
                                                .flags = ROCE_WC_WITH_IMM},
        [IB_OPCODE_RDMA_WRITE_LAST_WITH_IMMEDIATE] = {.opcode = ROCE_WC_RECV_RDMA_WITH_IMM,
                                                      .complete = true,
                                                      .flags = ROCE_WC_WITH_IMM},
        [IB_OPCODE_RDMA_WRITE_ONLY_WITH_IMMEDIATE] = {.opcode = ROCE_WC_RECV_RDMA_WITH_IMM,
                                                      .complete = true,
                                                      .flags = ROCE_WC_WITH_IMM},
    };

    *wc_opcode = _roce_recv_wr_to_wc_tbl[ib_opcode].opcode;
    *wc_flags |= _roce_recv_wr_to_wc_tbl[ib_opcode].flags;
    *complete = _roce_recv_wr_to_wc_tbl[ib_opcode].complete;
}

typedef struct roce_queue_t {
    roce_spin_lock_t lock;
    struct list_head wrs; /* queued WRs */
                          /* uint32_t nr_wr; XXX: if strict check is needed  */
} roce_queue_t;

typedef struct roce_qp {
    roce_res res;
    uint8_t qp_type; /* roce_qp_type */
    bool sq_sig_all;
    bool use_srq;
    roce_pd *pd;
    roce_cq *scq;
    roce_cq *rcq;
    roce_srq *srq;
    void *qp_ctx;
    roce_qp_attr attr;
    roce_queue_t sq;
    roce_queue_t rq;
    struct roce_requester_t {
        uint32_t psn;
        roce_spin_lock_t lock;
        struct list_head pending_pkts;  /* pending netpkts */
        struct list_head outgoing_pkts; /* outgoing netpkts */
    } req;
    struct roce_responder_t {
        union {
            _roce_recv_wr *wr; /* incoming WR */
            struct {
                uint64_t iova;
                uint32_t rkey;
                uint32_t dmalen;
                uint32_t writtenlen;
            }; /* incoming RDMA WRITE MR */
        };
        bool in_psn_nak; /* previous PSN NAK in process */
        uint32_t msn;
    } resp;
} roce_qp;

static inline const char *roce_qp_type_str(uint8_t qp_type)
{
    static const char *qp_type_str[] = {"RC", "UC", "RD", "UD", "CNP", "XRC"};

    if (qp_type >= ARRAY_SIZE(qp_type_str)) {
        return "UNKNOWN";
    }

    return qp_type_str[qp_type];
}

static inline uint8_t roce_qp_type_to_ib(uint8_t qp_type)
{
    static uint8_t qp_type_to_ib[ROCE_QPT_MAX] = {
        [ROCE_QPT_SMI] = IB_OPCODE_UD, [ROCE_QPT_GSI] = IB_OPCODE_UD, [ROCE_QPT_RC] = IB_OPCODE_RC,
        [ROCE_QPT_UC] = IB_OPCODE_UC,  [ROCE_QPT_RD] = IB_OPCODE_RD,  [ROCE_QPT_UD] = IB_OPCODE_UD};

    return qp_type_to_ib[qp_type];
}

static inline const char *roce_qp_state_str(roce_qp_state state)
{
    static const char *qp_state_str[] = {"RESET", "INIT", "RTR", "RTS", "SQD", "SQE", "ERR"};

    if (state >= ARRAY_SIZE(qp_state_str)) {
        return "UNKNOWN";
    }

    return qp_state_str[state];
}

static inline uint32_t roce_qp_psn_inc(uint32_t psn)
{
    return (psn + 1) & IB_BTH_PSN_MASK;
}

typedef struct roce_device {
#define ROCE_MAX_SGE 64
#define ROCE_MAX_INLINE_DATA 1024
    roce_device_attr attr;
    /* a single PKEY is supported, no need to declare any field */
#define ROCE_MAX_PKEY 1
#define ROCE_PKEY 0xFFFF

#define ROCE_MIN_GENERAL (1)
#define ROCE_MAX_GENERAL (1 << 24)
#define ROCE_MAX_MR (1 << 24)
#define ROCE_MAX_QP (1 << 24)
    roce_resource resources[ROCE_RESOURCE_MAX];
    roce_qp *smi;
    roce_qp *gsi;
    roce_spin_lock_t lock;
} roce_device;

typedef struct roce_port {
    roce_port_attr attr;
#define ROCE_MAX_VNET_HDR 16
    uint8_t vnet_hdr; /* bytes of vnet hdr. see virtio-net spec */
#define ROCE_GID_TBL_LEN 8
    uint8_t gids[ROCE_GID_TBL_LEN][ROCE_GID_LEN];
    uint8_t mac[ROCE_MAC_LEN];
} roce_port;

typedef struct roce_ctx {
    roce_ctx_para para;
    roce_device dev;
#define ROCE_MAX_PORTS 1
    roce_port ports[ROCE_MAX_PORTS];

    roce_spin_lock_t loglock;
    char logbuf[256];
} roce_ctx;

void roce_free_dev(roce_ctx *ctx);

void roce_request_rc(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *_send_wr);
void roce_handle_acknowledge(roce_ctx *ctx, roce_qp *qp, ib_aeth_syndrome_type syndrome,
                             uint8_t val, uint32_t psn, uint32_t msn);

void roce_request_ud(roce_ctx *ctx, roce_qp *qp, _roce_send_wr *_send_wr);

roce_mr *roce_get_mr(roce_ctx *ctx, uint32_t lkey);
void roce_put_mr(roce_ctx *ctx, uint32_t mr);

int roce_get_mrs(roce_ctx *ctx, roce_sge *sges, uint32_t num_sge);
void roce_put_mrs(roce_ctx *ctx, roce_sge *sges, uint32_t num_sge);

int roce_mr_map_sge(roce_ctx *ctx, roce_qp *qp, roce_sge *sges, uint32_t num_sge,
                    struct iovec *iovs, bool local, uint32_t access, bool *dmamap);

static inline void roce_mr_unmap_sge(roce_ctx *ctx, bool dmamap, struct iovec *iovs,
                                     uint32_t num_iov)
{
    if (!dmamap) {
        return;
    }

    for (uint32_t i = 0; i < num_iov; i++) {
        struct iovec *iov = &iovs[i];
        ctx->para.dma_unmap(ctx->para.ctx_opaque, iov->iov_base, iov->iov_len);
    }
}

roce_qp *roce_get_qp(roce_ctx *ctx, uint32_t qpn);
int roce_put_qp(roce_ctx *ctx, uint32_t qpn);
int roce_set_qp_state(roce_ctx *ctx, roce_qp *qp, roce_qp_state state);

/* Pre-allocate SMI & GSI */
int roce_alloc_special_qps(roce_ctx *ctx);
void roce_destroy_special_qps(roce_ctx *ctx);

static inline uint8_t roce_vnet_hdr(roce_ctx *ctx, uint8_t port_num)
{
    roce_port *port = &ctx->ports[port_num - 1];

    return port->vnet_hdr;
}

static inline uint8_t roce_qp_vnet_hdr(roce_ctx *ctx, roce_qp *qp)
{
    uint8_t port_num = qp->attr.port_num;

    return roce_vnet_hdr(ctx, port_num);
}

roce_ah *roce_get_ah(roce_ctx *ctx, int ah_handle);
int roce_put_ah(roce_ctx *ctx, int ah_handle);

roce_cq *roce_get_cq(roce_ctx *ctx, int cq_handle);
int roce_put_cq(roce_ctx *ctx, int cq_handle);

roce_pd *roce_get_pd(roce_ctx *ctx, int pd_handle);
int roce_put_pd(roce_ctx *ctx, int pd_handle);

#endif /* LIBROCE_ROCE_PRIVATE_H */
