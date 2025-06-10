/*
 * Simple checksum helper function.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_CHECKSUM_H
#define LIBROCE_CHECKSUM_H

#include <stdint.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include "spec/udp-spec.h"

/* @addr: IP/UDP header address
 * @len: length in bytes, must be even numbers
 */
static inline uint16_t roce_checksum(uint16_t *addr, uint32_t len)
{
    uint32_t sum = 0;

    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }

    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

#endif /* LIBROCE_CHECKSUM_H */
