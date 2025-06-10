/*
 * Exported structures/API of libroce.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_ROCE_H
#define LIBROCE_ROCE_H

#include <sys/uio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct roce_ctx roce_ctx;

#define ROCE_V2_UDP_DPORT 4791

typedef enum roce_version {
    ROCE_V1, /* not supported yet */
    ROCE_V2
} roce_version;

typedef enum roce_device_cap_flags {
    ROCE_DEVICE_RESIZE_MAX_WR,
    ROCE_DEVICE_BAD_PKEY_CNTR,
    ROCE_DEVICE_BAD_QKEY_CNTR,
    ROCE_DEVICE_RAW_MULTI,
    ROCE_DEVICE_AUTO_PATH_MIG,
    ROCE_DEVICE_CHANGE_PHY_PORT,
    ROCE_DEVICE_UD_AV_PORT_ENFORCE,
    ROCE_DEVICE_CURR_QP_STATE_MOD,
    ROCE_DEVICE_SHUTDOWN_PORT,
    ROCE_DEVICE_PORT_ACTIVE_EVENT,
    ROCE_DEVICE_SYS_IMAGE_GUID,
    ROCE_DEVICE_RC_RNR_NAK_GEN,
    ROCE_DEVICE_SRQ_RESIZE,
    ROCE_DEVICE_N_NOTIFY_CQ,
    ROCE_DEVICE_MEM_WINDOW,
    ROCE_DEVICE_UD_IP_CSUM,
    ROCE_DEVICE_XRC,
    ROCE_DEVICE_MEM_MGT_EXTENSIONS,
    ROCE_DEVICE_MEM_WINDOW_TYPE_2A,
    ROCE_DEVICE_MEM_WINDOW_TYPE_2B,
    ROCE_DEVICE_RC_IP_CSUM,
    ROCE_DEVICE_RAW_IP_CSUM,
    ROCE_DEVICE_MANAGED_FLOW_STEERING,
    ROCE_DEVICE_RAW_SCATTER_FCS,
    ROCE_DEVICE_PCI_WRITE_END_PADDING,
    ROCE_DEVICE_FLUSH_GLOBAL,
    ROCE_DEVICE_FLUSH_PERSISTENT,
    ROCE_DEVICE_ATOMIC_WRITE
} roce_device_cap_flags;

typedef struct roce_device_attr {
    uint64_t device_cap_flags; /* mask of roce_device_cap_flags */
    uint32_t max_mr_size;
    uint32_t max_qp;
    uint32_t max_qp_wr;
    uint32_t max_sge;
    uint32_t max_sge_rd;
    uint32_t max_cq;
    uint32_t max_cqe;
    uint32_t max_mr;
    uint32_t max_pd;
    uint32_t max_qp_rd_atom;
    uint32_t max_ee_rd_atom;
    uint32_t max_res_rd_atom;
    uint32_t max_qp_init_rd_atom;
    uint32_t max_ee_init_rd_atom;
    uint32_t max_ee;
    uint32_t max_rdd;
    uint32_t max_mw;
    uint32_t max_raw_ipv6_qp;
    uint32_t max_raw_ethy_qp;
    uint32_t max_mcast_grp;
    uint32_t max_mcast_qp_attach;
    uint32_t max_total_mcast_qp_attach;
    uint32_t max_ah;
    uint32_t max_fmr;
    uint32_t max_map_per_fmr;
    uint32_t max_srq;
    uint32_t max_srq_wr;
    uint32_t max_srq_sge;
    uint32_t max_inline_data;
    uint16_t max_pkeys;
    uint8_t local_ca_ack_delay;
    uint8_t phys_port_cnt;
} roce_device_attr;

typedef enum roce_port_cap_flags {
    ROCE_PORT_SM,
    ROCE_PORT_NOTICE_SUP,
    ROCE_PORT_TRAP_SUP,
    ROCE_PORT_OPT_IPD_SUP,
    ROCE_PORT_AUTO_MIGR_SUP,
    ROCE_PORT_SL_MAP_SUP,
    ROCE_PORT_MKEY_NVRAM,
    ROCE_PORT_PKEY_NVRAM,
    ROCE_PORT_LED_INFO_SUP,
    ROCE_PORT_SM_DISABLED,
    ROCE_PORT_SYS_IMAGE_GUID_SUP,
    ROCE_PORT_PKEY_SW_EXT_PORT_TRAP_SUP,
    ROCE_PORT_EXTENDED_SPEEDS_SUP,
    ROCE_PORT_CAP_MASK2_SUP,
    ROCE_PORT_CM_SUP,
    ROCE_PORT_SNMP_TUNNEL_SUP,
    ROCE_PORT_REINIT_SUP,
    ROCE_PORT_DEVICE_MGMT_SUP,
    ROCE_PORT_VENDOR_CLASS_SUP,
    ROCE_PORT_DR_NOTICE_SUP,
    ROCE_PORT_CAP_MASK_NOTICE_SUP,
    ROCE_PORT_BOOT_MGMT_SUP,
    ROCE_PORT_LINK_LATENCY_SUP,
    ROCE_PORT_CLIENT_REG_SUP,
    ROCE_PORT_IP_BASED_GIDS
} roce_port_cap_flags;

typedef struct roce_port_attr {
    uint64_t port_cap_flags; /* mask of roce_port_cap_flags */
    /*
     * 256/512 is not supported by libroce.
     * the small MTU on a morden network interface leads poor performance.
     */
    uint16_t max_mtu;    /* 1024/2048/4096. */
    uint16_t active_mtu; /* 1024/2048/4096. */
    uint16_t gid_tbl_len;
    uint16_t pkey_tbl_len;
    uint32_t max_msg_sz;
    uint32_t bad_pkey_cntr;
    uint32_t qkey_viol_cntr;
} roce_port_attr;

typedef enum roce_access_flags {
    ROCE_ACCESS_LOCAL_WRITE = (1 << 0),
    ROCE_ACCESS_REMOTE_WRITE = (1 << 1),
    ROCE_ACCESS_REMOTE_READ = (1 << 2),
    ROCE_ACCESS_REMOTE_ATOMIC = (1 << 3),
    ROCE_ACCESS_MW_BIND = (1 << 4),
    ROCE_ACCESS_ZERO_BASED = (1 << 5),
    ROCE_ACCESS_ON_DEMAND = (1 << 6),
    ROCE_ACCESS_HUGETLB = (1 << 7)
} roce_access_flags;

typedef enum roce_mw_type { ROCE_MW_TYPE_1 = 1, ROCE_MW_TYPE_2 } roce_mw_type;

typedef struct roce_ah_attr {
#define ROCE_GID_LEN 16
    uint8_t gid[ROCE_GID_LEN];
#define ROCE_MAC_LEN 6
    uint8_t mac[ROCE_MAC_LEN];
    uint32_t flow_label;
    uint8_t sgid_index;
    uint8_t hop_limit;
    uint8_t traffic_class;
    uint8_t port_num;
} roce_ah_attr;

typedef enum roce_mr_type { ROCE_MR_MEM, ROCE_MR_FRMR, ROCE_MR_DMA, ROCE_MR_MAX } roce_mr_type;

typedef enum roce_srq_attr_mask {
    ROCE_SRQ_MAX_WR = (1 << 0),
    ROCE_SRQ_LIMIT = (1 << 1)
} roce_srq_attr_mask;

typedef struct roce_srq_attr {
    uint32_t max_wr;
    uint32_t max_sge;
    uint32_t srq_limit;
} roce_srq_attr;

typedef enum roce_srq_type {
    ROCE_SRQT_BASIC,
    ROCE_SRQT_XRC /* not supported yet */
} roce_srq_type;

typedef enum roce_qp_attr_mask {
    ROCE_QP_STATE = (1 << 0),
    ROCE_QP_CUR_STATE = (1 << 1),
    ROCE_QP_EN_SQD_ASYNC_NOTIFY = (1 << 2),
    ROCE_QP_ACCESS_FLAGS = (1 << 3),
    ROCE_QP_PKEY_INDEX = (1 << 4),
    ROCE_QP_PORT = (1 << 5),
    ROCE_QP_QKEY = (1 << 6),
    ROCE_QP_AV = (1 << 7),
    ROCE_QP_PATH_MTU = (1 << 8),
    ROCE_QP_TIMEOUT = (1 << 9),
    ROCE_QP_RETRY_CNT = (1 << 10),
    ROCE_QP_RNR_RETRY = (1 << 11),
    ROCE_QP_RQ_PSN = (1 << 12),
    ROCE_QP_MAX_QP_RD_ATOMIC = (1 << 13),
    ROCE_QP_ALT_PATH = (1 << 14),
    ROCE_QP_MIN_RNR_TIMER = (1 << 15),
    ROCE_QP_SQ_PSN = (1 << 16),
    ROCE_QP_MAX_DEST_RD_ATOMIC = (1 << 17),
    ROCE_QP_PATH_MIG_STATE = (1 << 18),
    ROCE_QP_CAP = (1 << 19),
    ROCE_QP_DEST_QPN = (1 << 20)
} roce_qp_attr_mask;

typedef enum roce_qp_state {
    ROCE_QPS_RESET,
    ROCE_QPS_INIT,
    ROCE_QPS_RTR,
    ROCE_QPS_RTS,
    ROCE_QPS_SQD,
    ROCE_QPS_SQE,
    ROCE_QPS_ERR,

    ROCE_QPS_MAX
} roce_qp_state;

typedef enum roce_mig_state { ROCE_MIG_MIGRATED, ROCE_MIG_REARM, ROCE_MIG_ARMED } roce_mig_state;

typedef struct roce_qp_cap {
    uint32_t max_send_wr;
    uint32_t max_recv_wr;
    uint32_t max_send_sge;
    uint32_t max_recv_sge;
    uint32_t max_inline_data;
} roce_qp_cap;

typedef struct roce_qp_attr {
    roce_qp_state qp_state;
    roce_qp_state cur_qp_state;
    uint32_t rq_psn;
    uint32_t sq_psn;
    uint16_t path_mtu;
    roce_mig_state path_mig_state;
    uint32_t qkey;
    uint32_t dest_qp_num;
    uint32_t qp_access_flags;
    roce_qp_cap cap;
    roce_ah_attr ah_attr;
    roce_ah_attr alt_ah_attr;
    uint16_t pkey_index;
    uint16_t alt_pkey_index;
    uint8_t en_sqd_async_notify;
    uint8_t sq_draining;
    uint8_t max_rd_atomic;
    uint8_t max_dest_rd_atomic;
    uint8_t min_rnr_timer;
    uint8_t port_num;
    uint8_t timeout;
    uint8_t retry_cnt;
    uint8_t rnr_retry;
    uint8_t alt_port_num;
    uint8_t alt_timeout;
} roce_qp_attr;

typedef enum roce_qp_type {
    ROCE_QPT_SMI = 0,
    ROCE_QPT_GSI = 1,
    ROCE_QPT_RC,
    ROCE_QPT_UC,
    ROCE_QPT_RD,
    ROCE_QPT_UD,

    ROCE_QPT_MAX
} roce_qp_type;

typedef enum roce_qp_flags { ROCE_QP_SIG_ALL = (1 << 0), ROCE_QP_SRQ = (1 << 1) } roce_qp_flags;

typedef enum roce_wc_status {
    ROCE_WC_SUCCESS,
    ROCE_WC_LOC_LEN_ERR,
    ROCE_WC_LOC_QP_OP_ERR,
    ROCE_WC_LOC_EEC_OP_ERR,
    ROCE_WC_LOC_PROT_ERR,
    ROCE_WC_WR_FLUSH_ERR,
    ROCE_WC_MW_BIND_ERR,
    ROCE_WC_BAD_RESP_ERR,
    ROCE_WC_LOC_ACCESS_ERR,
    ROCE_WC_REM_INV_REQ_ERR,
    ROCE_WC_REM_ACCESS_ERR,
    ROCE_WC_REM_OP_ERR,
    ROCE_WC_RETRY_EXC_ERR,
    ROCE_WC_RNR_RETRY_EXC_ERR,
    ROCE_WC_LOC_RDD_VIOL_ERR,
    ROCE_WC_REM_INV_RD_REQ_ERR,
    ROCE_WC_REM_ABORT_ERR,
    ROCE_WC_INV_EECN_ERR,
    ROCE_WC_INV_EEC_STATE_ERR,
    ROCE_WC_FATAL_ERR,
    ROCE_WC_RESP_TIMEOUT_ERR,
    ROCE_WC_GENERAL_ERR
} roce_wc_status;

typedef enum roce_wc_opcode {
    ROCE_WC_SEND,
    ROCE_WC_RDMA_WRITE,
    ROCE_WC_RDMA_READ,
    ROCE_WC_COMP_SWAP,
    ROCE_WC_FETCH_ADD,
    ROCE_WC_ATOMIC_WRITE,
    ROCE_WC_FLUSH,
    ROCE_WC_LOCAL_INV,
    ROCE_WC_REG_MR,
    ROCE_WC_BIND_MW,
    ROCE_WC_RECV,
    ROCE_WC_RECV_RDMA_WITH_IMM,

    ROCE_WC_OPCODE_MAX
} roce_wc_opcode;

static inline const char *roce_wc_opcode_str(roce_wc_opcode opcode)
{
    static const char *wc_opcode_str[] = {
        [ROCE_WC_SEND] = "send",           [ROCE_WC_RDMA_WRITE] = "rdma-write",
        [ROCE_WC_RDMA_READ] = "rdma-read", [ROCE_WC_COMP_SWAP] = "comp-swap",
        [ROCE_WC_FETCH_ADD] = "fetch-add", [ROCE_WC_ATOMIC_WRITE] = "atomic-write",
        [ROCE_WC_FLUSH] = "flush",         [ROCE_WC_LOCAL_INV] = "local-inv",
        [ROCE_WC_REG_MR] = "reg-mr",       [ROCE_WC_BIND_MW] = "bind-mw",
        [ROCE_WC_RECV] = "recv",           [ROCE_WC_RECV_RDMA_WITH_IMM] = "recv-rdma-with-imm"};

    if (opcode >= sizeof(wc_opcode_str) / sizeof(wc_opcode_str[0])) {
        return "UNKNOWN";
    }

    return wc_opcode_str[opcode];
}

typedef enum roce_wc_flags {
    ROCE_WC_GRH = (1 << 0),
    ROCE_WC_WITH_IMM = (1 << 1),
    ROCE_WC_WITH_INVALIDATE = (1 << 2),
    ROCE_WC_IP_CSUM_OK = (1 << 3),
    ROCE_WC_WITH_SMAC = (1 << 4),
    ROCE_WC_WITH_VLAN = (1 << 5),
    ROCE_WC_WITH_NETWORK_HDR_TYPE = (1 << 6)
} roce_wc_flags;

typedef struct roce_wc {
    uint64_t wr_id;
    roce_wc_status status;
    roce_wc_opcode opcode;
    uint32_t byte_len;
    uint32_t local_qpn;
    uint32_t remote_qpn;
    union {
        uint32_t imm_data; /* be32 */
        uint32_t invalidated_rkey;
    };
    uint32_t vendor_err;
    uint16_t wc_flags; /* mask of roce_wc_flags */
    uint16_t pkey_index;
} roce_wc;

typedef enum roce_wr_opcode {
    ROCE_WR_SEND,
    ROCE_WR_SEND_WITH_IMM,
    ROCE_WR_SEND_WITH_INV,
    ROCE_WR_RDMA_WRITE,
    ROCE_WR_RDMA_WRITE_WITH_IMM,
    ROCE_WR_RDMA_READ,
    ROCE_WR_ATOMIC_CMP_AND_SWP,
    ROCE_WR_ATOMIC_FETCH_AND_ADD,
    ROCE_WR_ATOMIC_WRITE,
    ROCE_WR_FLUSH,

    ROCE_WR_LOCAL_INV,
    ROCE_WR_REG_MR,
    ROCE_WR_BIND_MW,

    ROCE_WR_OPCODE_MAX,
} roce_wr_opcode;

typedef enum roce_send_flags {
    ROCE_SEND_FENCE = (1 << 0),
    ROCE_SEND_SIGNALED = (1 << 1),
    ROCE_SEND_SOLICITED = (1 << 2),
    ROCE_SEND_INLINE = (1 << 3),
    ROCE_SEND_IP_CSUM = (1 << 4)
} roce_send_flags;

typedef struct roce_sge {
    uint64_t addr;
    uint32_t length;
    uint32_t lkey;
} roce_sge;

typedef struct roce_recv_wr {
    uint64_t wr_id;
    uint32_t num_sge;
    roce_sge sge[0];
} roce_recv_wr;

typedef struct roce_send_wr {
    uint64_t wr_id;
    roce_wr_opcode opcode;
    uint32_t send_flags; /* mask of roce_send_flags */
    union {
        uint32_t imm_data;
        uint32_t invalidated_rkey;
    };
    union {
        struct {
            uint64_t remote_addr;
            uint32_t rkey;
        } rdma;
        struct {
            uint64_t remote_addr;
            uint64_t compare_add;
            uint64_t swap;
            uint32_t rkey;
        } atomic;
        struct {
            uint32_t remote_qpn;
            uint32_t remote_qkey;
            uint32_t ah;
        } ud;
    } wr;
    uint32_t num_sge;
    roce_sge sge[0];
} roce_send_wr;

typedef enum roce_log_level {
    roce_log_error = 1,
    roce_log_warn,
    roce_log_notice,
    roce_log_info,
    roce_log_debug
} roce_log_level;

typedef struct roce_ctx_para {
    roce_version version;

    /* user opaque pointer */
    void *ctx_opaque;

    /* guest page size. no limitation on host page size */
    uint32_t page_size;

    /* optional: log level and callback */
    roce_log_level log_level;
    void (*log)(void *ctx_opaque, char *msg);

    /* required: network transmission callback */
    void (*net_xmit)(void *ctx_opaque, uint16_t queue, struct iovec *iovs, uint32_t num_iov);

    /* required: CQ completion callback, return 0 if CQE is handle successfully */
    int (*cq_comp)(void *ctx_opaque, roce_wc *wc, int cq, void *cq_ctx);

    /* required: map/unmap DMA for MR */
    void *(*dma_map)(void *ctx_opaque, uint64_t hwaddr, uint32_t len);
    void (*dma_unmap)(void *ctx_opaque, void *addr, uint32_t len);

    /*
     * optional: set memory management functions instead of default libc
     */
    struct {
        void *(*malloc)(size_t size);
        void (*free)(void *ptr);
        void *(*calloc)(size_t nmemb, size_t size);
        void *(*realloc)(void *ptr, size_t size);
    };

    /*
     * optional: set spin lock functions instead of default libc
     *   - QEMU may use qatomic functions.
     *   - DPDK may use RTE functions.
     *   - pthread_spin_lock/unlock would be fine for general programs.
     */
    struct {
        unsigned int lock_size; /* bytes of spin-lock in memory */
        int (*init)(void *lock, int shared);
        int (*lock)(void *lock);
        int (*trylock)(void *lock);
        int (*unlock)(void *lock);
    } spin;
} roce_ctx_para;

/* create a context of libroce. return NULL on failure. */
roce_ctx *roce_new_ctx(roce_ctx_para *para);

/* free a context of libroce. all the resources are freed automatically. */
void roce_free_ctx(roce_ctx *ctx);

/* initialize *device* associated with context */
int roce_init_device(roce_ctx *ctx, roce_device_attr *para);

/* initialize *port* associated with context */
int roce_init_port(roce_ctx *ctx, uint8_t port, roce_port_attr *attr);

/* set MAC to a port */
int roce_set_port_mac(roce_ctx *ctx, uint8_t port, uint8_t *mac);

/* set vnet header size to a port */
int roce_set_port_vnet(roce_ctx *ctx, uint8_t port, uint8_t vnet_hdr);

/* query attributes of a port */
int roce_query_port(roce_ctx *ctx, uint8_t port, roce_port_attr *attr);

/* handle RoCE packets.
 *   return consumed bytes on success;
 *   return -errno on failure */
int roce_net_recv(roce_ctx *ctx, uint8_t queue, const struct iovec *iovs, uint32_t num_iov);

/* add GID to a port */
int roce_add_gid(roce_ctx *ctx, uint8_t port, uint8_t index, uint8_t *gid);

/* delete GID from a port */
int roce_del_gid(roce_ctx *ctx, uint8_t port, uint8_t index);

/* query GID from a port */
int roce_get_gid(roce_ctx *ctx, uint8_t port, uint8_t index, uint8_t *gid);

/* allocate a protection domain.
 *   return PD handle (>= 0) on success.
 *   return -errno on failure */
int roce_alloc_pd(roce_ctx *ctx, void *pd_ctx);

/* free a protection domain */
int roce_dealloc_pd(roce_ctx *ctx, int pd);

/* create a completion queue
 *   return CQ handle (>= 0) on success.
 *   return -errno on failure */
int roce_create_cq(roce_ctx *ctx, int cqe, int comp_vector, void *cq_ctx);

/* modifies the capacity of the CQ */
int roce_resize_cq(roce_ctx *ctx, int cq, int cqe);

/* destroy a completion queue */
int roce_destroy_cq(roce_ctx *ctx, int cq);

/* get opaque user data of a completion queue */
int roce_cq_ctx(roce_ctx *ctx, int cq, void **cq_ctx);

/* create an address handle
 *   return AH handle (>= 0) on success.
 *   return -errno on failure */
int roce_create_ah(roce_ctx *ctx, int pd, roce_ah_attr *attr, void *ah_ctx);

/* destroy an address handle */
int roce_destroy_ah(roce_ctx *ctx, int ah);

/* create memory region. if @page_size is 0, use the device page_size.
 *   return MR handle (>= 0) on success.
 *   return -errno on failure */
int roce_create_mr(roce_ctx *ctx, int pd, roce_mr_type type, uint64_t iova, uint32_t length,
                   uint32_t access, struct iovec *iovs, uint32_t num_iov, uint32_t page_size,
                   void *mr_ctx);

/* get @lkey and @rkey of a MR */
int roce_get_mr_key(roce_ctx *ctx, int mr, uint32_t *lkey, uint32_t *rkey);

/* destroy a MR */
int roce_destroy_mr(roce_ctx *ctx, int mr);

/* allocate a receive word request. Auto freed roce_post_recv. */
int roce_alloc_recv_wr(roce_ctx *ctx, uint32_t num_sge, roce_recv_wr **recv_wr);

/* Post a work requests to a receive queue */
int roce_post_recv(roce_ctx *ctx, int qp, roce_recv_wr *recv_wr);

/* free a receive word request */
void roce_free_recv_wr(roce_ctx *ctx, roce_recv_wr *recv_wr);

/* allocate a send word request. Auto freed once roce_post_send */
int roce_alloc_send_wr(roce_ctx *ctx, uint32_t num_sge, roce_send_wr **send_wr);

/* free a send word request */
void roce_free_send_wr(roce_ctx *ctx, roce_send_wr *send_wr);

/* Post a work requests to a send queue */
int roce_post_send(roce_ctx *ctx, int qp, roce_send_wr *send_wr);

/* create a queue pair. @flags is mask of roce_qp_flags.
 *   @recv_cq is receive CQ handle, or SRQ handle on (flags & ROCE_QP_SRQ).
 *   return QP handle (>= 0) on success.
 *   return -errno on failure */
int roce_create_qp(roce_ctx *ctx, int pd, roce_qp_type qp_type, int srq, int send_cq, int recv_cq,
                   roce_qp_cap *cap, uint32_t flags, void *qp_ctx);

/* Get QP type */
roce_qp_type roce_get_qp_type(roce_ctx *ctx, int qp);

/* destroy a queue pair */
int roce_destroy_qp(roce_ctx *ctx, int qp);

/* query attributes from a QP */
int roce_query_qp(roce_ctx *ctx, int qp, roce_qp_attr *attr, uint32_t attr_mask);

/* modify attributes to a QP */
int roce_modify_qp(roce_ctx *ctx, int qp, roce_qp_attr *attr, uint32_t attr_mask);

/* get opaque user data of a QP */
int roce_qp_ctx(roce_ctx *ctx, int qp, void **qp_ctx);

#endif /* LIBROCE_ROCE_H */
