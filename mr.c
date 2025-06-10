/*
 * MR implementation.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string.h>
#include <stdlib.h>

#include "private/mm.h"
#include "private/roce-private.h"
#include "private/iov-helper.h"
#include "private/util.h"
#include "private/lock.h"
#include "private/log.h"
#include "private/atomic.h"

static inline uint32_t roce_key_to_index(uint32_t key)
{
    return key >> 8;
}

static inline uint32_t roce_new_key(uint32_t index)
{
    return (index << 8) | (random() % (1 << 8));
}

int roce_create_mr(roce_ctx *ctx, int pd_handle, roce_mr_type type, uint64_t iova, uint32_t length,
                   uint32_t access, struct iovec *iovs, uint32_t num_iov, uint32_t page_size,
                   void *mr_ctx)
{
    roce_device *dev = &ctx->dev;
    roce_device_attr *dev_attr = &dev->attr;
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_MR];
    roce_pd *pd;
    roce_mr *mr;
    int key;
    int ret;

    if (page_size) {
        if (page_size < ctx->para.page_size || !roce_power_of_2(page_size)) {
            roce_log_error(ctx, "invalid page size for new MR");
            return -EINVAL;
        }
    } else {
        page_size = ctx->para.page_size;
    }

    pd = roce_get_pd(ctx, pd_handle);
    if (!pd) {
        roce_log_error(ctx, "invalid PD %d on creating MR", pd_handle);
        return -EINVAL;
    }

    mr = roce_calloc(ctx, 1, roce_mr_size(num_iov));
    if (!mr) {
        roce_log_error(ctx, "failed to allocate memory for new MR");
        ret = -ENOMEM;
        goto put_pd;
    }

    switch (type) {
    case ROCE_MR_MEM:
        if (roce_iov_len(iovs, num_iov) > dev_attr->max_mr_size) {
            roce_log_error(ctx, "MR size exceeds for new MR");
            ret = -EINVAL;
            goto free_mem;
        }

        if (!length) {
            roce_log_error(ctx, "0 bytes length for new MR");
            ret = -EINVAL;
            goto free_mem;
        }

        mr->state = ROCE_MR_STATE_VALID;
        mr->num_iov = num_iov;
        memcpy(mr->iovs, iovs, sizeof(struct iovec) * num_iov);
        break;

    case ROCE_MR_FRMR:
        if (num_iov) {
            roce_log_error(ctx, "set PBL on creating FRMR");
            ret = -EINVAL;
            goto free_mem;
        }

        if (!length || length < ctx->para.page_size || (length % page_size)) {
            roce_log_error(ctx, "invalid length for new MR");
            ret = -EINVAL;
            goto free_mem;
        }
        mr->state = ROCE_MR_STATE_FREE;
        break;

    case ROCE_MR_DMA:
        mr->state = ROCE_MR_STATE_VALID;
        break; // TODO

    default:
        roce_log_error(ctx, "unsupported MR type");
        ret = -ENOTSUP;
        goto free_mem;
    }

    key = roce_resource_alloc(resource, &mr->res);
    if (key < 0) {
        ret = key;
        goto free_mem;
    }

    mr->res.handle = key;
    mr->mr_ctx = mr_ctx;
    mr->pd = pd;
    mr->type = type;
    mr->iova = iova;
    mr->length = length;
    mr->lkey = roce_new_key(key);
    mr->rkey = roce_new_key(key);
    mr->access = access;
    mr->page_size = page_size;

    roce_log_info(ctx, "MR(0x%x) type(%s) IOVA(%lx) length(%x) created", key,
                  roce_mr_type_str(type), iova, length);

    return key;

free_mem:
    roce_free(ctx, mr);

put_pd:
    ROCE_ASSERT(!roce_put_pd(ctx, pd_handle));
    return ret;
}

static inline roce_mr *roce_get_mr_noref(roce_ctx *ctx, int mr)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_MR];
    roce_res *res = roce_resource_get(resource, mr);

    return container_of(res, roce_mr, res);
}

roce_mr *roce_get_mr(roce_ctx *ctx, uint32_t mr_handle)
{
    roce_mr *mr = roce_get_mr_noref(ctx, mr_handle);

    if (!mr) {
        return NULL;
    }

    roce_atomic_inc(&mr->res.ref);
    return mr;
}

roce_mr *roce_get_mr_by_key(roce_ctx *ctx, uint32_t lkey)
{
    return roce_get_mr(ctx, roce_key_to_index(lkey));
}

void roce_put_mr(roce_ctx *ctx, uint32_t mr_handle)
{
    roce_mr *mr = roce_get_mr_noref(ctx, mr_handle);

    if (!mr) {
        roce_log_warn(ctx, "Put invalid MR %d", mr_handle);
        return;
    }

    roce_atomic_dec(&mr->res.ref);

    if (mr->state == ROCE_MR_STATE_DESTROYED) {
        roce_destroy_mr(ctx, mr_handle);
    }
}

int roce_get_mrs(roce_ctx *ctx, roce_sge *sges, uint32_t num_sge)
{
    roce_sge *sge;
    int i;

    for (i = 0; i < num_sge; i++) {
        sge = &sges[i];
        if (!roce_get_mr(ctx, roce_key_to_index(sge->lkey))) {
            goto error;
        }
    }

    return 0;

error:
    for (--i; i >= 0; i--) {
        sge = &sges[i];
        roce_put_mr(ctx, roce_key_to_index(sge->lkey));
    }

    return -EACCES;
}

void roce_put_mrs(roce_ctx *ctx, roce_sge *sges, uint32_t num_sge)
{
    for (uint32_t i = 0; i < num_sge; i++) {
        roce_sge *sge = &sges[i];
        roce_put_mr(ctx, roce_key_to_index(sge->lkey));
    }
}

int roce_get_mr_key(roce_ctx *ctx, int mr_handle, uint32_t *lkey, uint32_t *rkey)
{
    roce_mr *mr;

    mr = roce_get_mr_noref(ctx, mr_handle);
    if (!mr) {
        return -EINVAL;
    }

    *lkey = mr->lkey;
    *rkey = mr->rkey;

    return 0;
}

int roce_destroy_mr(roce_ctx *ctx, int mr_handle)
{
    roce_resource *resource = &ctx->dev.resources[ROCE_RESOURCE_MR];
    roce_mr *mr = roce_get_mr_noref(ctx, mr_handle);
    int ret;

    if (!mr) {
        roce_log_error(ctx, "failed to destroy invalid MR %d", mr_handle);
        return -EINVAL;
    }

    if (mr->res.ref) {
        roce_log_info(ctx, "MR %d in use. Ref %d, Destroy later.", mr->res.handle, mr->res.ref);
        mr->state = ROCE_MR_STATE_DESTROYED;
        return 0; /* pending destroyed */
    }

    ret = roce_resource_free(resource, mr_handle);
    if (ret) {
        roce_log_error(ctx, "failed to destroy MR resource");
        return ret;
    }

    ROCE_ASSERT(!roce_put_pd(ctx, mr->pd->res.handle));
    roce_free(ctx, mr);
    return 0;
}

int roce_mr_map_sge(roce_ctx *ctx, roce_qp *qp, roce_sge *sges, uint32_t num_sge,
                    struct iovec *iovs, bool local, uint32_t access, bool *dmamap)
{
    roce_mr *mr;
    struct roce_sge *sge;
    uint32_t num_iov = 0;
    uint32_t idx;
    roce_mr_type mr_type = ROCE_MR_MAX;
    const char *keystr = local ? "LKEY" : "RKEY";

    for (idx = 0; idx < num_sge; idx++) {
        sge = &sges[idx];
        /* the MRs must be referenced */
        mr = roce_get_mr_noref(ctx, roce_key_to_index(sge->lkey));
        if (!mr) {
            roce_log_warn(ctx, "QP(0x%x) handle invalid %s(0x%x) ", qp->res.handle, keystr,
                          sge->lkey);
            return -EACCES;
        }

        uint32_t key = local ? mr->lkey : mr->rkey;
        if (key != sge->lkey) {
            roce_log_warn(ctx, "QP(0x%x) %s(0x%x) mismatched 0x%x", qp->res.handle, keystr,
                          sge->lkey, key);
            return -EACCES;
        }

        if (mr->pd != qp->pd) {
            roce_log_warn(ctx, "QP(0x%x) %s(0x%x) mismatched PD", qp->res.handle, keystr,
                          sge->lkey);
            return -EACCES;
        }

        if (mr->state != ROCE_MR_STATE_VALID) {
            roce_log_warn(ctx, "QP(0x%x) %s(0x%x) not valid", qp->res.handle, keystr, sge->lkey);
            return -EACCES;
        }

        if ((access & mr->access) != access) {
            roce_log_warn(ctx, "QP(0x%x) %s(0x%x) access 0x%x excees 0x%x", qp->res.handle, keystr,
                          sge->lkey, access, mr->access);
            return -EACCES;
        }

        if (mr_type == ROCE_MR_MAX) {
            mr_type = mr->type;
        } else if (mr_type != mr->type) {
            /* we don't want to support SGEs with mixed MR types. Ex, MR MEM of SGE[0], MR FRMR of
             * SGE[1], MR DMA of SGE[2] */
            roce_log_warn(ctx, "QP(0x%x) use mixed MR type in a single WR", qp->res.handle);
            return -EACCES;
        }

        if (mr->type == ROCE_MR_DMA) {
            /* DMA MR is used by MAD only in Linux */
            if (qp->qp_type != ROCE_QPT_GSI) {
                roce_log_warn(ctx, "QP(0x%x) type %d does not support DMA MR", qp->res.handle,
                              qp->qp_type);
                return -EACCES;
            }

            struct iovec *iov = &iovs[num_iov];
            iov->iov_base = ctx->para.dma_map(ctx->para.ctx_opaque, sge->addr, sge->length);
            iov->iov_len = sge->length;
            num_iov++;
            *dmamap = true;
        } else {
            if ((sge->addr < mr->iova) || (sge->addr + sge->length > mr->iova + mr->length)) {
                roce_log_warn(ctx, "QP(0x%x) %s(0x%x) 0x%lx#0x%x out of MR range 0x%lx#0x%x",
                              qp->res.handle, keystr, sge->lkey, sge->addr, sge->length, mr->iova,
                              mr->length);
                return -EACCES;
            }

            uint32_t page_size = mr->page_size;
            uint32_t len = sge->length;
            uint32_t off_pages = sge->addr / page_size - mr->iova / page_size;
            uint32_t off_bytes = sge->addr % page_size;
            struct iovec *iov = &mr->iovs[off_pages];
            num_iov += roce_iov_copy(iov, mr->num_iov - off_pages, off_bytes, len, &iovs[num_iov]);
            *dmamap = false;
        }
    }

    return num_iov;
}
