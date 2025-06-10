/*
 * ROCE SGE helper functions.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_ROCE_SGE_HELPER_H
#define LIBROCE_ROCE_SGE_HELPER_H

#include "roce/roce.h"
#include "private/util.h"

/* Count bytes from the index @sge_idx of @num_sge, the first sge has a offset @sge_off */
static inline uint32_t roce_sge_len(roce_sge *sges, uint32_t num_sge, uint32_t sge_idx,
                                    uint32_t sge_off)
{
    roce_sge *sge;
    uint64_t total;

    assert(sge_idx < num_sge);
    assert(sge_off <= sges[sge_idx].length);

    sge = &sges[sge_idx];
    total = sge->length - sge_off;

    for (uint32_t i = sge_idx + 1; i < num_sge; i++) {
        sge = &sges[i];
        total += sge->length;
    }

    assert(total <= UINT32_MAX);
    return total;
}

/* Count how many pages from the @num_sge of @sges */
static inline uint32_t roce_sge_pages(roce_sge *sges, uint32_t num_sge, uint32_t page_size)
{
    uint32_t pages = 0;
    uint64_t addr_aligndown;
    uint64_t addr_alignup;

    assert(roce_power_of_2(page_size));

    for (uint32_t i = 0; i < num_sge; i++) {
        roce_sge *sge = &sges[i];

        addr_aligndown = sge->addr & ~(page_size - 1);
        addr_alignup = (sge->addr + sge->length + page_size - 1) & ~(page_size - 1);
        pages += (addr_alignup - addr_aligndown) / page_size;
    }

    return pages;
}

/*
 * Pick @bytes from @src_sges, save SGE info into @dst_sges.
 * Note that @sge_inx and @off must be initialized as 0 in the first round. @src_sges is not
 * changed.
 */
static inline uint32_t roce_sge_advance(roce_sge *src_sges, uint32_t src_num_sge, uint32_t bytes,
                                        uint32_t *sge_idx, uint32_t *off, roce_sge *dst_sges)
{
    roce_sge *src_sge, *dst_sge;
    uint32_t dst_sge_idx;
    uint32_t remained = 0;

    assert(*sge_idx < src_num_sge);
    assert(*off <= src_sges[*sge_idx].length);
    if (!bytes) {
        return 0;
    }

    for (dst_sge_idx = 0; *sge_idx < src_num_sge; (*sge_idx)++, dst_sge_idx++) {
        src_sge = &src_sges[*sge_idx];
        dst_sge = &dst_sges[dst_sge_idx];

        dst_sge->lkey = src_sge->lkey;
        dst_sge->addr = src_sge->addr + *off;

        remained = src_sge->length - *off;
        if (remained > bytes) {
            dst_sge->length = bytes;
            *off += bytes;
            break;
        } else if (remained == bytes) {
            dst_sge->length = remained;
            *off = 0;
            (*sge_idx)++;
            break;
        } else {
            dst_sge->length = remained;
            bytes -= remained;
            *off = 0;
        }
    }

    return dst_sge_idx + 1;
}

#endif /* LIBROCE_ROCE_SGE_HELPER_H */
