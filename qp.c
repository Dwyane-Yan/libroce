/*
 * QP implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string.h>
#include <stdlib.h>

#include "private/roce-private.h"
#include "private/mm.h"
#include "private/lock.h"
#include "private/log.h"
#include "private/atomic.h"

static inline void roce_queue_init(roce_ctx *ctx, roce_queue_t *queue)
{
    roce_spin_init(ctx, &queue->lock);
    list_head_init(&queue->wrs);
}

static inline roce_qp *roce_get_qp_noref(roce_ctx *ctx, int qp_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_QP];
    roce_res *res = roce_resource_get(resource, qp_handle);

    return container_of(res, roce_qp, res);
}

roce_qp_type roce_get_qp_type(roce_ctx *ctx, int qp_handle)
{
    roce_qp *qp = roce_get_qp_noref(ctx, qp_handle);

    return qp->qp_type;
}

int roce_qp_ctx(roce_ctx *ctx, int qp_handle, void **qp_ctx)
{
    roce_qp *qp = roce_get_qp_noref(ctx, qp_handle);

    if (!qp) {
        return -EINVAL;
    }

    *qp_ctx = qp->qp_ctx;
    return 0;
}

roce_qp *roce_get_qp(roce_ctx *ctx, uint32_t qp_handle)
{
    roce_qp *qp = roce_get_qp_noref(ctx, qp_handle);

    if (!qp) {
        return NULL;
    }

    roce_atomic_inc(&qp->res.ref);
    return qp;
}

int roce_put_qp(roce_ctx *ctx, uint32_t qp_handle)
{
    roce_qp *qp = roce_get_qp_noref(ctx, qp_handle);

    if (!qp) {
        return -EINVAL;
    }

    roce_atomic_dec(&qp->res.ref);
    return 0;
}

int roce_alloc_recv_wr(roce_ctx *ctx, uint32_t num_sge, roce_recv_wr **recv_wr)
{
    _roce_recv_wr *wr;
    uint32_t size;

    if (num_sge > ctx->dev.attr.max_sge) {
        return -EINVAL;
    }

    size = sizeof(_roce_recv_wr) + sizeof(roce_sge) * num_sge;
    wr = roce_calloc(ctx, 1, size);
    if (!wr) {
        return -ENOMEM;
    }

    list_node_init(&wr->entry);
    wr->recv_wr.num_sge = num_sge;

    *recv_wr = &wr->recv_wr;
    return 0;
}

void roce_free_recv_wr(roce_ctx *ctx, roce_recv_wr *recv_wr)
{
    _roce_recv_wr *_recv_wr = roce_recv_wr_to_priv(recv_wr);

    roce_put_mrs(ctx, recv_wr->sge, recv_wr->num_sge);
    roce_free(ctx, _recv_wr);
}

int roce_post_recv(roce_ctx *ctx, int qp_handle, roce_recv_wr *recv_wr)
{
    roce_qp *qp = roce_get_qp(ctx, qp_handle);
    roce_queue_t *rq = &qp->rq;
    _roce_recv_wr *_recv_wr = roce_recv_wr_to_priv(recv_wr);

    /* increase references for each MR, put MRs on freeing recv_wr */
    if (roce_get_mrs(ctx, recv_wr->sge, recv_wr->num_sge)) {
        roce_free_recv_wr(ctx, recv_wr);
        return -EACCES;
    }

    roce_spin_lock(ctx, &rq->lock);
    list_add_tail(&rq->wrs, &_recv_wr->entry);
    roce_spin_unlock(ctx, &rq->lock);

    return 0;
}

int roce_alloc_send_wr(roce_ctx *ctx, uint32_t num_sge, roce_send_wr **send_wr)
{
    _roce_send_wr *wr;
    uint32_t size;

    if (num_sge > ctx->dev.attr.max_sge) {
        return -EINVAL;
    }

    size = sizeof(_roce_send_wr) + sizeof(roce_sge) * num_sge;
    wr = roce_calloc(ctx, 1, size);
    if (!wr) {
        return -ENOMEM;
    }

    roce_spin_init(ctx, &wr->lock);
    list_node_init(&wr->entry);
    wr->send_wr.num_sge = num_sge;

    *send_wr = &wr->send_wr;
    return 0;
}

void roce_free_send_wr(roce_ctx *ctx, roce_send_wr *send_wr)
{
    _roce_send_wr *_send_wr = roce_send_wr_to_priv(send_wr);

    if (_send_wr->mapped) {
        roce_put_mrs(ctx, send_wr->sge, send_wr->num_sge);
    }

    roce_free(ctx, _send_wr);
}

int roce_post_send(roce_ctx *ctx, int qp_handle, roce_send_wr *send_wr)
{
    roce_qp *qp = roce_get_qp(ctx, qp_handle);
    _roce_send_wr *_send_wr = roce_send_wr_to_priv(send_wr);

    do {
        if (!roce_send_wr_is_inline(_send_wr)) {
            /* increase references for each MR, put MRs on freeing send_wr */
            _send_wr->error = roce_get_mrs(ctx, send_wr->sge, send_wr->num_sge);
            if (_send_wr->error) {
                roce_log_warn(ctx, "QP(0x%x) failed to ref MR on Post Send", qp_handle);
                break;
            }

            _send_wr->mapped = true;
        }
    } while (false);

    switch (qp->qp_type) {
    case ROCE_QPT_RC:
        roce_request_rc(ctx, qp, _send_wr);
        break;

    case ROCE_QPT_GSI:
    case ROCE_QPT_UD:
        roce_request_ud(ctx, qp, _send_wr);
        break;

    default:
        ROCE_ASSERT("Unsupported QP type" == NULL);
    }

    return 0;
}

static int roce_verify_qp_cap(roce_ctx *ctx, roce_qp_cap *cap)
{
    roce_device *dev = &ctx->dev;
    roce_device_attr *dev_attr = &dev->attr;

    if (!cap->max_send_wr || (cap->max_send_wr > dev_attr->max_qp_wr)) {
        roce_log_error(ctx, "invalid max_send_wr");
        return -EINVAL;
    }

    if (!cap->max_recv_wr || (cap->max_recv_wr > dev_attr->max_qp_wr)) {
        roce_log_error(ctx, "invalid max_recv_wr");
        return -EINVAL;
    }

    if (!cap->max_send_sge || (cap->max_send_sge > dev_attr->max_sge)) {
        roce_log_error(ctx, "invalid max_send_sge");
        return -EINVAL;
    }

    if (!cap->max_recv_sge || (cap->max_recv_sge > dev_attr->max_sge)) {
        roce_log_error(ctx, "invalid max_recv_sge");
        return -EINVAL;
    }

    if (cap->max_inline_data > dev_attr->max_inline_data) {
        roce_log_error(ctx, "invalid max_inline_data");
        return -EINVAL;
    }

    return 0;
}

int roce_alloc_special_qps(roce_ctx *ctx)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_QP];
    int qpn;

    ctx->dev.smi = roce_calloc(ctx, 1, sizeof(roce_qp));
    if (!ctx->dev.smi) {
        roce_log_error(ctx, "failed to allocate memory for SMI");
        return -ENOMEM;
    }

    qpn = roce_resource_alloc_at(resource, &ctx->dev.smi->res, ROCE_QPT_SMI);
    if (qpn < 0) {
        roce_log_error(ctx, "failed to allocate GSI");
        goto free_smi;
    }

    ctx->dev.gsi = roce_calloc(ctx, 1, sizeof(roce_qp));
    if (!ctx->dev.gsi) {
        roce_log_error(ctx, "failed to allocate memory for GSI");
        goto remove_smi;
    }

    ctx->dev.gsi->res.handle = ROCE_QPT_GSI;
    qpn = roce_resource_alloc_at(resource, &ctx->dev.gsi->res, ROCE_QPT_GSI);
    if (qpn < 0) {
        roce_log_error(ctx, "failed to allocate GSI");
        goto free_gsi;
    }

    /* mark the two QPs as ROCE_QPT_MAX, it indicates *unused*  */
    ctx->dev.smi->qp_type = ROCE_QPT_MAX;
    ctx->dev.gsi->qp_type = ROCE_QPT_MAX;

    return 0;

free_gsi:
    roce_free(ctx, ctx->dev.gsi);

remove_smi:
    roce_resource_free(resource, ROCE_QPT_SMI);

free_smi:
    roce_free(ctx, ctx->dev.smi);

    return qpn;
}

void roce_destroy_special_qps(roce_ctx *ctx)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_QP];

    if (ctx->dev.smi) {
        roce_resource_free(resource, ROCE_QPT_SMI);
        roce_free(ctx, ctx->dev.smi);
    }

    if (ctx->dev.gsi) {
        if (ctx->dev.gsi->qp_type != ROCE_QPT_MAX) {
            roce_destroy_qp(ctx, ROCE_QPT_GSI);
        } else {
            roce_resource_free(resource, ROCE_QPT_GSI);
            roce_free(ctx, ctx->dev.gsi);
        }
    }
}

int roce_create_qp(roce_ctx *ctx, int pd_handle, roce_qp_type qp_type, int srq, int send_cq,
                   int recv_cq, roce_qp_cap *cap, uint32_t flags, void *qp_ctx)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_QP];
    roce_port_attr *port_attr = &ctx->ports[0].attr; /* default port 1 */
    roce_qp_attr *qp_attr;
    roce_pd *pd = NULL;
    roce_cq *scq = NULL, *rcq = NULL;
    roce_qp *qp = NULL;
    int qpn;

    switch (qp_type) {
    case ROCE_QPT_GSI:
    case ROCE_QPT_RC:
    case ROCE_QPT_UD:
        break;

    case ROCE_QPT_SMI:
    case ROCE_QPT_UC:
    case ROCE_QPT_RD:
    default:
        return -ENOTSUP;
    }

    if (roce_verify_qp_cap(ctx, cap)) {
        return -EINVAL;
    }

    pd = roce_get_pd(ctx, pd_handle);
    if (!pd) {
        roce_log_error(ctx, "invalid PD %d on creating QP", pd_handle);
        return -EINVAL;
    }

    scq = roce_get_cq(ctx, send_cq);
    if (!scq) {
        roce_log_error(ctx, "invalid SCQ %d on creating QP", send_cq);
        goto put_resource;
    }

    rcq = roce_get_cq(ctx, recv_cq);
    if (!scq) {
        roce_log_error(ctx, "invalid RCQ %d on creating QP", send_cq);
        goto put_resource;
    }

    if (qp_type == ROCE_QPT_GSI) {
        ROCE_ASSERT(ctx->dev.gsi);
        if (ctx->dev.gsi->qp_type != ROCE_QPT_MAX) {
            roce_log_error(ctx, "GSI in use");
            goto put_resource;
        }

        qp = ctx->dev.gsi;
        qp->qp_type = ROCE_QPT_GSI;
        qpn = qp->res.handle;
        ROCE_ASSERT(qpn == ROCE_QPT_GSI);
    } else {
        qp = roce_calloc(ctx, 1, sizeof(roce_qp));
        if (!qp) {
            roce_log_error(ctx, "failed to allocate memory for QP");
            goto put_resource;
        }

        qpn = roce_resource_alloc(resource, &qp->res);
        if (qpn < 0) {
            roce_log_error(ctx, "failed to allocate QP resource");
            goto free_qp;
        }
    }

    qp->res.handle = qpn;
    qp->qp_type = qp_type;
    qp->sq_sig_all = flags & ROCE_QP_SIG_ALL;
    qp->use_srq = flags & ROCE_QP_SRQ;
    qp->pd = pd;
    qp->scq = scq;
    qp->rcq = rcq;
    qp->qp_ctx = qp_ctx;
    qp_attr = &qp->attr;
    memcpy(&qp_attr->cap, cap, sizeof(qp_attr->cap));
    /* assign default QP attributes */
    qp_attr->qp_state = ROCE_QPS_RESET;
    qp_attr->path_mtu = port_attr->active_mtu;
    qp_attr->port_num = 1;
    qp_attr->rq_psn = random() & IB_BTH_PSN_MASK;
    qp_attr->sq_psn = random() & IB_BTH_PSN_MASK;

    roce_queue_init(ctx, &qp->sq);
    roce_queue_init(ctx, &qp->rq);
    roce_spin_init(ctx, &qp->req.lock);
    list_head_init(&qp->req.pending_pkts);
    list_head_init(&qp->req.outgoing_pkts);

    return qpn;

free_qp:
    roce_free(ctx, qp);

put_resource:
    if (pd) {
        ROCE_ASSERT(!roce_put_pd(ctx, pd_handle));
    }

    if (scq) {
        ROCE_ASSERT(!roce_put_cq(ctx, send_cq));
    }

    if (rcq) {
        ROCE_ASSERT(!roce_put_cq(ctx, recv_cq));
    }

    return -EINVAL;
}

int roce_destroy_qp(roce_ctx *ctx, int qp_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_QP];
    roce_qp *qp = roce_get_qp(ctx, qp_handle);

    ROCE_ASSERT(!roce_put_pd(ctx, qp->pd->res.handle));
    ROCE_ASSERT(!roce_put_cq(ctx, qp->scq->res.handle));
    ROCE_ASSERT(!roce_put_cq(ctx, qp->rcq->res.handle));

    roce_resource_free(resource, qp->res.handle);

    _roce_send_wr *send_wr, *tmp_send_wr;
    list_for_each_safe (&qp->sq.wrs, send_wr, tmp_send_wr, entry) {
        list_del(&send_wr->entry);
        roce_free_send_wr(ctx, &send_wr->send_wr);
    }

    _roce_recv_wr *recv_wr, *tmp_recv_wr;
    list_for_each_safe (&qp->rq.wrs, recv_wr, tmp_recv_wr, entry) {
        list_del(&recv_wr->entry);
        roce_free_recv_wr(ctx, &recv_wr->recv_wr);
    }

    if (qp->qp_type == ROCE_QPT_GSI) {
        memset(ctx->dev.gsi, 0x00, sizeof(*ctx->dev.gsi));
        ctx->dev.gsi->qp_type = ROCE_QPT_MAX;
    } else {
        roce_free(ctx, qp);
    }

    roce_log_info(ctx, "QP(0x%x) destroyed", qp_handle);
    return 0;
}

static int roce_modify_qp_ah(roce_ctx *ctx, roce_qp *qp, roce_ah_attr *ah_attr)
{
    roce_qp_attr *_attr = &qp->attr;
    uint8_t *sgid;

    if (ah_attr->sgid_index >= ROCE_GID_TBL_LEN) {
        return -EINVAL;
    }

    sgid = ctx->ports[0].gids[ah_attr->sgid_index];
    if (roce_gid_is_ipv4(sgid) != roce_gid_is_ipv4(ah_attr->gid)) {
        return -EINVAL;
    }

    memcpy(&_attr->ah_attr, ah_attr, sizeof(_attr->ah_attr));
    return 0;
}

int roce_set_qp_state(roce_ctx *ctx, roce_qp *qp, roce_qp_state state)
{
    return 0;
}

static int32_t unsupported_qp_attr = ROCE_QP_ALT_PATH | ROCE_QP_PATH_MIG_STATE;

int roce_modify_qp(roce_ctx *ctx, int qp_handle, roce_qp_attr *attr, uint32_t attr_mask)
{
    roce_qp *qp = roce_get_qp(ctx, qp_handle);
    roce_qp_attr *_attr = &qp->attr;
    int ret;

    if (attr_mask & unsupported_qp_attr) {
        roce_log_error(ctx, "unsupported attr mask on modifying QP");
        return -ENOTSUP;
    }

    if (attr_mask & ROCE_QP_STATE) {
        // XXX
        _attr->qp_state = attr->qp_state;
        roce_log_info(ctx, "QP(0x%x) modify state to %s", qp_handle,
                      roce_qp_state_str(_attr->qp_state));
    }

    if (attr_mask & ROCE_QP_ACCESS_FLAGS) {
        _attr->qp_access_flags = attr->qp_access_flags;
    }

    if (attr_mask & ROCE_QP_PKEY_INDEX) {
        if (attr->pkey_index >= ROCE_MAX_PKEY) {
            return -EINVAL;
        }
        _attr->pkey_index = attr->pkey_index;
    }

    if (attr_mask & ROCE_QP_PORT) {
        if (attr->port_num > ROCE_MAX_PORTS) {
            return -EINVAL;
        }
        _attr->port_num = attr->port_num;
    }

    if (attr_mask & ROCE_QP_AV) {
        ret = roce_modify_qp_ah(ctx, qp, &attr->ah_attr);
        if (ret) {
            return ret;
        }
    }

    if (attr_mask & ROCE_QP_QKEY) {
        _attr->qkey = attr->qkey;
    }

    if (attr_mask & ROCE_QP_PATH_MTU) {
        if (!roce_valid_mtu(attr->path_mtu)) {
            return -EINVAL;
        }
        if (attr->path_mtu > ctx->ports[attr->port_num - 1].attr.max_mtu) {
            return -EINVAL;
        }
        _attr->path_mtu = attr->path_mtu;
    }

    if (attr_mask & ROCE_QP_SQ_PSN) {
        if (!list_empty(&qp->req.pending_pkts) || !list_empty(&qp->req.outgoing_pkts)) {
            roce_log_error(ctx, "SQ PSN busy");
            return -EBUSY;
        }
        _attr->sq_psn = attr->sq_psn;
        qp->req.psn = attr->sq_psn;
    }

    if (attr_mask & ROCE_QP_RQ_PSN) {
        _attr->rq_psn = attr->rq_psn;
    }

    if (attr_mask & ROCE_QP_MAX_QP_RD_ATOMIC) {
        _attr->max_rd_atomic = attr->max_rd_atomic;
    }

    if (attr_mask & ROCE_QP_MAX_DEST_RD_ATOMIC) {
        _attr->max_dest_rd_atomic = attr->max_dest_rd_atomic;
    }

    if (attr_mask & ROCE_QP_CAP) {
        ret = roce_verify_qp_cap(ctx, &attr->cap);
        if (ret) {
            return ret;
        }

        memcpy(&_attr->cap, &attr->cap, sizeof(attr->cap));
    }

    if (attr_mask & ROCE_QP_DEST_QPN) {
        _attr->dest_qp_num = attr->dest_qp_num;
    }

    return 0;
}

int roce_query_qp(roce_ctx *ctx, int qp_handle, roce_qp_attr *attr, uint32_t attr_mask)
{
    roce_qp *qp = roce_get_qp(ctx, qp_handle);
    roce_qp_attr *_attr = &qp->attr;

    if (attr_mask & unsupported_qp_attr) {
        roce_log_error(ctx, "unsupported attr mask on querying QP");
        return -ENOTSUP;
    }

    memcpy(attr, _attr, sizeof(*attr));

    return 0;
}
