/*
 * PCAP helpers.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef LIBROCE_PCAP_H
#define LIBROCE_PCAP_H

#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "private/iov-helper.h"

typedef struct roce_pcap_frame_hdr {
    int32_t tv_sec;
    int32_t tv_usec;
    uint32_t caplen;
    uint32_t len;
} roce_pcap_frame_hdr;

typedef struct roce_pcap_file_hdr {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t linktype;
} roce_pcap_file_hdr;

static inline void roce_pcap_init_header(roce_pcap_file_hdr *hdr)
{
    hdr->magic = 0xa1b2c3d4;
    hdr->version_major = 2;
    hdr->version_minor = 4;
    hdr->thiszone = 0;
    hdr->sigfigs = 0;
    hdr->snaplen = 65536 * 4;
    hdr->linktype = 1;
}

static inline int roce_pcap_create(const char *path)
{
    struct roce_pcap_file_hdr hdr;
    struct stat statbuf;
    int fd;

    roce_pcap_init_header(&hdr);

    if (!stat(path, &statbuf)) {
        return -EEXIST;
    }

    fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd == -1) {
        return -errno;
    }

    write(fd, &hdr, sizeof(hdr));
    return fd;
}

static inline int roce_pcap_write_frame(int fd, struct iovec *iovs, uint32_t num_iov)
{
    struct roce_pcap_frame_hdr frame;
    struct timeval now;

    gettimeofday(&now, NULL);
    frame.tv_sec = now.tv_sec;
    frame.tv_usec = now.tv_usec;
    frame.caplen = frame.len = roce_iov_len(iovs, num_iov);
    if (write(fd, &frame, sizeof(frame)) == -1) {
        return -errno;
    }

    if (writev(fd, iovs, num_iov) == -1) {
        return -errno;
    }

    return 0;
}

#endif /* LIBROCE_PCAP_H */
