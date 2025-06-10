/*
 * Base on IB spec 1.7, define data structures(in big endian) and types.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_IB_SPEC_H
#define LIBROCE_IB_SPEC_H

#include <endian.h>
#include <stdbool.h>
#include <stdint.h>

/* BASE TRANSPORT HEADER (BTH) - 12 BYTES */
#define IB_BTH_SIZE 12

typedef struct ib_bth {
    /* OPERATION CODE (OPCODE) */
    /* Code[7-5] */
#define IB_OPCODE_RC 0x0
#define IB_OPCODE_UC 0x1
#define IB_OPCODE_RD 0x2
#define IB_OPCODE_UD 0x3
#define IB_OPCODE_CNP 0x4
#define IB_OPCODE_XRC 0x5
    /* Code[4-0] */
#define IB_OPCODE_MASK 0x1f
#define IB_OPCODE_SEND_FIRST 0x00
#define IB_OPCODE_SEND_MIDDLE 0x01
#define IB_OPCODE_SEND_LAST 0x02
#define IB_OPCODE_SEND_LAST_WITH_IMMEDIATE 0x03
#define IB_OPCODE_SEND_ONLY 0x04
#define IB_OPCODE_SEND_ONLY_WITH_IMMEDIATE 0x05
#define IB_OPCODE_RDMA_WRITE_FIRST 0x06
#define IB_OPCODE_RDMA_WRITE_MIDDLE 0x07
#define IB_OPCODE_RDMA_WRITE_LAST 0x08
#define IB_OPCODE_RDMA_WRITE_LAST_WITH_IMMEDIATE 0x09
#define IB_OPCODE_RDMA_WRITE_ONLY 0x0a
#define IB_OPCODE_RDMA_WRITE_ONLY_WITH_IMMEDIATE 0x0b
#define IB_OPCODE_RDMA_READ_REQUEST 0x0c
#define IB_OPCODE_RDMA_READ_RESPONSE_FIRST 0x0d
#define IB_OPCODE_RDMA_READ_RESPONSE_MIDDLE 0x0e
#define IB_OPCODE_RDMA_READ_RESPONSE_LAST 0x0f
#define IB_OPCODE_RDMA_READ_RESPONSE_ONLY 0x10
#define IB_OPCODE_ACKNOWLEDGE 0x11
#define IB_OPCODE_ATOMIC_ACKNOWLEDGE 0x12
#define IB_OPCODE_COMPARE_SWAP 0x13
#define IB_OPCODE_FETCH_ADD 0x14
#define IB_OPCODE_RESYNC 0x15
#define IB_OPCODE_SEND_LAST_WITH_INVALIDATE 0x16
#define IB_OPCODE_SEND_ONLY_WITH_INVALIDATE 0x17
#define IB_OPCODE_GET_CLASS_OPERATION 0x1a
#define IB_OPCODE_PUT_CLASS_OPERATION 0x1b
#define IB_OPCODE_FLUSH 0x1c
#define IB_OPCODE_ATOMIC_WRITE 0x1d
    uint8_t opcode;
#define IB_BTH_SE_MASK (0x80)
#define IB_BTH_MIG_MASK (0x40)
#define IB_BTH_PADCNT_MASK (0x30)
#define IB_BTH_TVER_MASK (0x0f)
    uint8_t flags;
    uint16_t pkey;
#define IB_BTH_FECN_MASK (0x80000000)
#define IB_BTH_BECN_MASK (0x40000000)
#define IB_BTH_RESV1_MASK (0x3f000000)
#define IB_BTH_DESTQP_MASK (0x00ffffff)
    uint32_t u32_1;
#define IB_BTH_ACK_MASK (0x80000000)
#define IB_BTH_RESV2_MASK (0x7f000000)
#define IB_BTH_PSN_MASK (0x00ffffff)
    uint32_t u32_2;
} ib_bth;

static inline uint8_t ib_bth_get_qp_type(void *arg)
{
    ib_bth *bth = arg;

    return bth->opcode >> 5;
}

static inline uint8_t ib_bth_get_opcode(void *arg)
{
    ib_bth *bth = arg;

    return bth->opcode & IB_OPCODE_MASK;
}

static inline void ib_bth_set_opcode(void *arg, uint8_t qp_type, uint8_t opcode)
{
    ib_bth *bth = arg;

    bth->opcode = (qp_type << 5) | opcode;
}

static inline bool ib_bth_get_se(void *arg)
{
    ib_bth *bth = arg;

    return !!(IB_BTH_SE_MASK & bth->flags);
}

static inline void ib_bth_set_se(void *arg, bool se)
{
    ib_bth *bth = arg;

    if (se) {
        bth->flags |= IB_BTH_SE_MASK;
    } else {
        bth->flags &= ~IB_BTH_SE_MASK;
    }
}

static inline bool ib_bth_get_mig(void *arg)
{
    ib_bth *bth = arg;

    return !!(IB_BTH_MIG_MASK & bth->flags);
}

static inline void ib_bth_set_mig(void *arg, bool mig)
{
    ib_bth *bth = arg;

    if (mig) {
        bth->flags |= IB_BTH_MIG_MASK;
    } else {
        bth->flags &= ~IB_BTH_MIG_MASK;
    }
}

static inline uint8_t ib_bth_get_padcnt(void *arg)
{
    ib_bth *bth = arg;

    return (IB_BTH_PADCNT_MASK & bth->flags) >> 4;
}

static inline void ib_bth_set_padcnt(void *arg, uint8_t padcnt)
{
    ib_bth *bth = arg;

    bth->flags = (IB_BTH_PADCNT_MASK & (padcnt << 4)) | (~IB_BTH_PADCNT_MASK & bth->flags);
}

static inline uint8_t ib_bth_get_tver(void *arg)
{
    ib_bth *bth = arg;

    return IB_BTH_TVER_MASK & bth->flags;
}

static inline void ib_bth_set_tver(void *arg, uint8_t tver)
{
    ib_bth *bth = arg;

    bth->flags = (IB_BTH_TVER_MASK & tver) | (~IB_BTH_TVER_MASK & bth->flags);
}

static inline uint16_t ib_bth_get_pkey(void *arg)
{
    ib_bth *bth = arg;

    return be16toh(bth->pkey);
}

static inline void ib_bth_set_pkey(void *arg, uint16_t pkey)
{
    ib_bth *bth = arg;

    bth->pkey = htobe16(pkey);
}

static inline bool ib_bth_get_fecn(void *arg)
{
    ib_bth *bth = arg;

    return !!(be32toh(bth->u32_1) & IB_BTH_FECN_MASK);
}

static inline void ib_bth_set_fecn(void *arg, bool fecn)
{
    ib_bth *bth = arg;

    if (fecn) {
        bth->u32_1 |= htobe32(IB_BTH_FECN_MASK);
    } else {
        bth->u32_1 &= ~htobe32(IB_BTH_FECN_MASK);
    }
}

static inline bool ib_bth_get_becn(void *arg)
{
    ib_bth *bth = arg;

    return !!(be32toh(bth->u32_1) & IB_BTH_BECN_MASK);
}

static inline void ib_bth_set_becn(void *arg, bool becn)
{
    ib_bth *bth = arg;

    if (becn) {
        bth->u32_1 |= htobe32(IB_BTH_BECN_MASK);
    } else {
        bth->u32_1 &= ~htobe32(IB_BTH_BECN_MASK);
    }
}

/* See IB spec: Transmitted as 0, ignored on receive. */
static inline void ib_bth_clear_resv1(void *arg)
{
    ib_bth *bth = arg;

    bth->u32_1 &= htobe32(~IB_BTH_RESV1_MASK);
}

static inline uint32_t ib_bth_get_destqp(void *arg)
{
    ib_bth *bth = arg;

    return IB_BTH_DESTQP_MASK & be32toh(bth->u32_1);
}

static inline void ib_bth_set_destqp(void *arg, uint32_t destqp)
{
    ib_bth *bth = arg;
    uint32_t u32_1 = be32toh(bth->u32_1);

    bth->u32_1 = htobe32((IB_BTH_DESTQP_MASK & destqp) | (~IB_BTH_DESTQP_MASK & u32_1));
}

static inline bool ib_bth_get_ack(void *arg)
{
    ib_bth *bth = arg;

    return !!(be32toh(bth->u32_2) & IB_BTH_ACK_MASK);
}

static inline void ib_bth_set_ack(void *arg, bool ack)
{
    ib_bth *bth = arg;

    if (ack) {
        bth->u32_2 |= htobe32(IB_BTH_ACK_MASK);
    } else {
        bth->u32_2 &= ~htobe32(IB_BTH_ACK_MASK);
    }
}

/* See IB spec: Transmitted as 0, ignored on receive. */
static inline void ib_bth_clear_resv2(void *arg)
{
    ib_bth *bth = arg;

    bth->u32_2 &= htobe32(~IB_BTH_RESV2_MASK);
}

static inline uint32_t ib_bth_get_psn(void *arg)
{
    ib_bth *bth = arg;

    return IB_BTH_PSN_MASK & be32toh(bth->u32_2);
}

static inline void ib_bth_set_psn(void *arg, uint32_t psn)
{
    ib_bth *bth = arg;
    uint32_t u32_2 = be32toh(bth->u32_2);

    bth->u32_2 = htobe32((IB_BTH_PSN_MASK & psn) | (~IB_BTH_PSN_MASK & u32_2));
}

/* RELIABLE DATAGRAM EXTENDED TRANSPORT HEADER (RDETH) - 4 BYTES */
#define IB_RDETH_SIZE 4

typedef struct ib_rdeth {
#define IB_RDETH_EECNXT_MASK (0x00ffffff)
    uint32_t u32_0;
} ib_rdeth;

static inline uint32_t ib_rdeth_get_eecnxt(void *arg)
{
    struct ib_rdeth *rdeth = arg;

    return IB_RDETH_EECNXT_MASK & be32toh(rdeth->u32_0);
}

static inline void ib_rdeth_set_eecnxt(void *arg, uint32_t eecnxt)
{
    struct ib_rdeth *rdeth = arg;

    /* all the significant 8 bits are reserved, no need to clear */
    rdeth->u32_0 = htobe32(IB_RDETH_EECNXT_MASK & eecnxt);
}

/* DATAGRAM EXTENDED TRANSPORT HEADER (DETH) - 8 BYTES */
#define IB_DETH_SIZE 8

typedef struct ib_deth {
    uint32_t qkey;
#define IB_DETH_SRCQP_MASK (0x00ffffff)
    uint32_t u32_1;
} ib_deth;

static inline uint32_t ib_deth_get_qkey(void *arg)
{
    ib_deth *deth = arg;

    return be32toh(deth->qkey);
}

static inline void ib_deth_set_qkey(void *arg, uint32_t qkey)
{
    ib_deth *deth = arg;

    deth->qkey = htobe32(qkey);
}

static inline uint32_t ib_deth_get_srcqp(void *arg)
{
    ib_deth *deth = arg;

    return be32toh(deth->u32_1) & IB_DETH_SRCQP_MASK;
}

static inline void ib_deth_set_srcqp(void *arg, uint32_t srcqp)
{
    ib_deth *deth = arg;

    /* all the significant 8 bits are reserved, no need to clear */
    deth->u32_1 = be32toh(srcqp & IB_DETH_SRCQP_MASK);
}

/* RDMA EXTENDED TRANSPORT HEADER (RETH) - 16 BYTES */
#define IB_RETH_SIZE 16

typedef struct ib_reth {
    uint64_t va;
    uint32_t rkey;
    uint32_t dmalen;
} ib_reth;

static inline uint64_t ib_reth_get_va(void *arg)
{
    ib_reth *reth = arg;

    return be64toh(reth->va);
}

static inline void ib_reth_set_va(void *arg, uint64_t va)
{
    ib_reth *reth = arg;

    reth->va = htobe64(va);
}

static inline uint32_t ib_reth_get_rkey(void *arg)
{
    ib_reth *reth = arg;

    return be32toh(reth->rkey);
}

static inline void ib_reth_set_rkey(void *arg, uint32_t rkey)
{
    ib_reth *reth = arg;

    reth->rkey = htobe32(rkey);
}

static inline uint32_t ib_reth_get_dmalen(void *arg)
{
    ib_reth *reth = arg;

    return be32toh(reth->dmalen);
}

static inline void ib_reth_set_dmalen(void *arg, uint32_t dmalen)
{
    ib_reth *reth = arg;

    reth->dmalen = htobe32(dmalen);
}

/* ATOMIC EXTENDED TRANSPORT HEADER (ATOMICETH) - 28 BYTES */
#define IB_ATOMICETH_SIZE 28

typedef struct __attribute__((__packed__)) ib_atomiceth {
    uint64_t va;
    uint32_t rkey;
    uint64_t swapdt;
    uint64_t cmpdt;
} ib_atomiceth;

static inline uint64_t ib_atomiceth_get_va(void *arg)
{
    ib_atomiceth *atomiceth = arg;

    return be64toh(atomiceth->va);
}

static inline void ib_atomiceth_set_va(void *arg, uint64_t va)
{
    ib_atomiceth *atomiceth = arg;

    atomiceth->va = htobe64(va);
}

static inline uint32_t ib_atomiceth_get_rkey(void *arg)
{
    ib_atomiceth *atomiceth = arg;

    return be32toh(atomiceth->rkey);
}

static inline void ib_atomiceth_set_rkey(void *arg, uint32_t rkey)
{
    ib_reth *reth = arg;

    reth->rkey = htobe32(rkey);
}

static inline uint64_t ib_atomiceth_get_swapdt(void *arg)
{
    ib_atomiceth *atomiceth = arg;

    return be64toh(atomiceth->swapdt);
}

static inline void ib_atomiceth_set_swapdt(void *arg, uint64_t swapdt)
{
    ib_atomiceth *atomiceth = arg;

    atomiceth->swapdt = htobe64(swapdt);
}

static inline uint64_t ib_atomiceth_get_cmpdt(void *arg)
{
    ib_atomiceth *atomiceth = arg;

    return be64toh(atomiceth->cmpdt);
}

static inline void ib_atomiceth_set_cmpdt(void *arg, uint64_t cmpdt)
{
    ib_atomiceth *atomiceth = arg;

    atomiceth->cmpdt = htobe64(cmpdt);
}

/* XRC EXTENDED TRANSPORT HEADER (XRCETH) */
typedef struct ib_xrceth {
#define IB_XRCETH_XRCSRQ_MASK (0x00ffffff)
    uint32_t u32_0;
} ib_xrceth;

static inline uint32_t ib_xrceth_get_xrcsrq(void *arg)
{
    ib_xrceth *xrceth = arg;

    return IB_XRCETH_XRCSRQ_MASK & be32toh(xrceth->u32_0);
}

static inline void ib_xrceth_set_xrcsrq(void *arg, uint32_t xrcsrq)
{
    ib_xrceth *xrceth = arg;

    /* all the significant 8 bits are reserved, no need to clear */
    xrceth->u32_0 = htobe32(IB_XRCETH_XRCSRQ_MASK & xrcsrq);
}

/* ACK EXTENDED TRANSPORT HEADER (AETH) - 4 BYTES */
#define IB_AETH_SIZE 4

typedef struct ib_aeth {
#define IB_AETH_SYNDROME_MASK (0xff000000)
#define IB_AETH_MSN_MASK (0x00ffffff)
    uint32_t u32_0;
} ib_aeth;

static inline uint8_t ib_aeth_get_syndrome(void *arg)
{
    ib_aeth *aeth = arg;

    return (be32toh(aeth->u32_0) & IB_AETH_SYNDROME_MASK) >> 24;
}

typedef enum ib_aeth_syndrome_type {
    IB_AETH_SYNDROME_ACK = 0,
    IB_AETH_SYNDROME_RNR_NAK = 1,
    IB_AETH_SYNDROME_RESERVED = 2,
    IB_AETH_SYNDROME_NAK = 3,
    IB_AETH_SYNDROME_MAX
} ib_aeth_syndrome_type;

static inline const char *ib_syndrome_type_string(ib_aeth_syndrome_type syndrome)
{
    static const char *syndrome_strings[] = {
        "ACK",
        "RNR-NAK",
        "RESERVED",
        "NAK",
    };

    if (syndrome < IB_AETH_SYNDROME_MAX) {
        return syndrome_strings[syndrome];
    }

    return "Unknown syndrome";
}

typedef enum ib_nak_code {
    IB_NAK_PSN_SEQUENCE_ERROR = 0,
    IB_NAK_INVALID_REQUEST = 1,
    IB_NAK_REMOTE_ACCESS_ERROR = 2,
    IB_NAK_REMOTE_OPERATION_ERROR = 3,
    IB_NAK_INVALID_RD_REQUEST = 4,
    IB_NAK_OPERATION_IN_PROGRESS = 5,
    IB_NAK_MAX /* keep last */
} ib_nak_code;

static inline const char *ib_nak_string(ib_nak_code code)
{
    static const char *nak_strings[] = {"PSN Sequence Error",  "Invalid Request",
                                        "Remote Access Error", "Remote Operational Error",
                                        "Invalid RD Request",  "Operation In Progress"};

    if (code < IB_NAK_MAX) {
        return nak_strings[code];
    }

    return "Unknown NAK code";
}

static inline ib_aeth_syndrome_type ib_aeth_get_syndrome_type(void *arg)
{
    uint8_t val = ib_aeth_get_syndrome(arg);

    /* bits 6:5 */
    return (val >> 5) & 0x3;
}

static inline uint8_t ib_aeth_get_syndrome_val(void *arg)
{
    uint8_t val = ib_aeth_get_syndrome(arg);

    /* bits 4:0 */
    return val & 0x1f;
}

static inline void ib_aeth_set_syndrome(void *arg, ib_aeth_syndrome_type type, uint8_t val)
{
    ib_aeth *aeth = arg;
    uint32_t u32_0 = be32toh(aeth->u32_0);
    uint32_t syndrome = (((type & 3) << 5) | (val & 0x1f)) << 24;

    aeth->u32_0 = htobe32((IB_AETH_SYNDROME_MASK & syndrome) | (~IB_AETH_SYNDROME_MASK & u32_0));
}

static inline uint32_t ib_aeth_get_msn(void *arg)
{
    ib_aeth *aeth = arg;

    return be32toh(aeth->u32_0) & IB_AETH_MSN_MASK;
}

static inline void ib_aeth_set_msn(void *arg, uint32_t msn)
{
    ib_aeth *aeth = arg;
    uint32_t u32_0 = be32toh(aeth->u32_0);

    aeth->u32_0 = htobe32((IB_AETH_MSN_MASK & msn) | (~IB_AETH_MSN_MASK & u32_0));
}

static inline void ib_aeth_set(void *arg, ib_aeth_syndrome_type type, uint8_t val, uint32_t msn)
{
    ib_aeth *aeth = arg;

    aeth->u32_0 = (((type & 3) << 5) | (val & 0x1f)) << 24;
    aeth->u32_0 |= (IB_AETH_MSN_MASK & msn);
    aeth->u32_0 = htobe32(aeth->u32_0);
}

/* ATOMIC ACK EXTENDED TRANSPORT HEADER (ATOMICACKETH) - 8 BYTES */
#define IB_ATOMICACKETH_SIZE 8

typedef struct ib_atomicacketh {
    uint64_t origmemdt;
} ib_atomicacketh;

static inline uint64_t ib_atomicacketh_get_origmemdt(void *arg)
{
    ib_atomicacketh *aaeth = arg;

    return be64toh(aaeth->origmemdt);
}

static inline void ib_atomicacketh_set_origmemdt(void *arg, uint64_t origmemdt)
{
    ib_atomicacketh *aaeth = arg;

    aaeth->origmemdt = htobe64(origmemdt);
}

/* IMMEDIATE DATA EXTENDED TRANSPORT HEADER (IMMDT) - 4 BYTES */
#define IB_IMMDT_SIZE 4

typedef struct ib_immdt {
    uint32_t immdt;
} ib_immdt;

static inline uint32_t ib_immdt_get_immdt(void *arg)
{
    struct ib_immdt *immdt = arg;

    return immdt->immdt;
}

static inline void ib_immdt_set_immdt(void *arg, uint32_t immdt)
{
    ib_immdt *_immdt = arg;

    _immdt->immdt = immdt;
}

/* INVALIDATE EXTENDED TRANSPORT HEADER (IETH) - 4 BYTES */
#define IB_IETH_SIZE 4

typedef struct ib_ieth {
    uint32_t rkey;
} ib_ieth;

static inline uint32_t ib_ieth_get_rkey(void *arg)
{
    struct ib_ieth *ieth = arg;

    return be32toh(ieth->rkey);
}

static inline void ib_ieth_set_rkey(void *arg, uint32_t rkey)
{
    ib_ieth *ieth = arg;

    ieth->rkey = htobe32(rkey);
}

/* FLUSH EXTENDED TRANSPORT HEADER (FETH) - 4 BYTES */
#define IB_FETH_SIZE 4

typedef struct ib_feth {
    uint32_t u32_0;
} ib_feth;

/* helpers */

static inline const char *ib_opcode_str(uint8_t opcode)
{
    static const char *strs[] = {"SEND-FIRST",
                                 "SEND-MIDDLE",
                                 "SEND-LAST",
                                 "SEND-LAST-WITH-IMMEDIATE",
                                 "SEND-ONLY",
                                 "SEND-ONLY-WITH-IMMEDIATE",
                                 "RDMA-WRITE-FIRST",
                                 "RDMA-WRITE-MIDDLE",
                                 "RDMA-WRITE-LAST",
                                 "RDMA-WRITE-LAST-WITH-IMMEDIATE",
                                 "RDMA-WRITE-ONLY",
                                 "RDMA-WRITE-ONLY-WITH-IMMEDIATE",
                                 "RDMA-READ-REQUEST",
                                 "RDMA-READ-RESPONSE-FIRST",
                                 "RDMA-READ-RESPONSE-MIDDLE",
                                 "RDMA-READ-RESPONSE-LAST",
                                 "RDMA-READ-RESPONSE-ONLY",
                                 "ACKNOWLEDGE",
                                 "ATOMIC-ACKNOWLEDGE",
                                 "COMPARE-SWAP",
                                 "FETCH-ADD",
                                 "RESYNC",
                                 "SEND-LAST-WITH-INVALIDATE",
                                 "SEND-ONLY-WITH-INVALIDATE"};

    if (opcode > sizeof(strs) / sizeof(strs[0])) {
        return "UNKNOWN";
    }

    return strs[opcode];
}

#endif /* LIBROCE_IB_SPEC_H */
