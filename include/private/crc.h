/*
 * CRC of libroce.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_CRC_H
#define LIBROCE_CRC_H

#include <sys/uio.h>
#include <stdbool.h>

#include "spec/udp-spec.h"
#include "spec/ib-spec.h"

uint32_t roce_crc32(uint32_t crc, struct iovec *iovs, uint32_t num_iov);

uint32_t roce_crc32_pkt(bool is_ipv4, void *ip, uint8_t *ihlbuf, uint8_t ihllen, udp_hdr *udph,
                        ib_bth *bth, struct iovec *iovs, uint32_t num_iov);

#endif /* LIBROCE_CRC_H */
