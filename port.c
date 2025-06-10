/*
 * RoCE port management.
 *
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string.h>

#include "private/roce-private.h"
#include "private/mm.h"
#include "private/log.h"
#include "private/util.h"

int roce_init_port(roce_ctx *ctx, uint8_t port, roce_port_attr *_attr)
{
    roce_port_attr *attr;

    /* port index begins from 1 */
    if (!port || (port > ROCE_MAX_PORTS)) {
        roce_log_error(ctx, "invalid port, available port[1, %d]", ROCE_MAX_PORTS);
        return -EINVAL;
    }

    if (!roce_valid_mtu(_attr->max_mtu) || !roce_valid_mtu(_attr->active_mtu)) {
        roce_log_error(ctx, "invalid MTU");
        return -EINVAL;
    }

    if (_attr->active_mtu > _attr->max_mtu) {
        roce_log_error(ctx, "invalid active MTU");
        return -EINVAL;
    }

    if (_attr->gid_tbl_len > ROCE_GID_TBL_LEN) {
        roce_log_error(ctx, "invalid gid_tbl_len, exceeds %d", ROCE_GID_TBL_LEN);
        return -EINVAL;
    }

    if (!_attr->gid_tbl_len) {
        _attr->gid_tbl_len = ROCE_GID_TBL_LEN;
    }

    if (_attr->pkey_tbl_len > ROCE_MAX_PKEY) {
        roce_log_error(ctx, "invalid pkey_tbl_len, exceeds %d", ROCE_MAX_PKEY);
        return -EINVAL;
    }

    if (!_attr->pkey_tbl_len) {
        _attr->pkey_tbl_len = ROCE_MAX_PKEY;
    }

    if (!_attr->max_msg_sz) {
        _attr->max_msg_sz = 32 * 1024 * 1024;
    }

    attr = &ctx->ports[port - 1].attr;
    memcpy(attr, _attr, sizeof(roce_port_attr));

    return 0;
}

int roce_set_port_mac(roce_ctx *ctx, uint8_t port, uint8_t *mac)
{
    /* port index begins from 1 */
    if (!port || (port > ROCE_MAX_PORTS)) {
        roce_log_error(ctx, "invalid port, available port[1, %d]", ROCE_MAX_PORTS);
        return -EINVAL;
    }

    memcpy(ctx->ports[port - 1].mac, mac, ROCE_MAC_LEN);

    return 0;
}

int roce_set_port_vnet(roce_ctx *ctx, uint8_t port, uint8_t vnet_hdr)
{
    /* port index begins from 1 */
    if (!port || (port > ROCE_MAX_PORTS)) {
        roce_log_error(ctx, "invalid port, available port[1, %d]", ROCE_MAX_PORTS);
        return -EINVAL;
    }

    if (vnet_hdr > ROCE_MAX_VNET_HDR) {
        roce_log_error(ctx, "invalid vnet hdr");
        return -EINVAL;
    }

    ctx->ports[port - 1].vnet_hdr = vnet_hdr;
    return 0;
}

int roce_query_port(roce_ctx *ctx, uint8_t port, roce_port_attr *_attr)
{
    /* port index begins from 1 */
    if (!port || (port > ROCE_MAX_PORTS)) {
        roce_log_error(ctx, "invalid port, available port[1, %d]", ROCE_MAX_PORTS);
        return -EINVAL;
    }

    memcpy(_attr, &ctx->ports[port - 1].attr, sizeof(*_attr));

    return 0;
}

static bool roce_empty_gid(uint8_t *gid)
{
    static uint8_t zero_gid[ROCE_GID_LEN];

    return !memcmp(zero_gid, gid, ROCE_GID_LEN);
}

int roce_add_gid(roce_ctx *ctx, uint8_t _port, uint8_t index, uint8_t *gid)
{
    roce_port *port;

    /* port index begins from 1 */
    if (!_port || (_port > ROCE_MAX_PORTS)) {
        roce_log_error(ctx, "invalid port, available port[1, %d]", ROCE_MAX_PORTS);
        return -EINVAL;
    }

    port = &ctx->ports[_port - 1];
    if (!roce_empty_gid(port->gids[index])) {
        roce_log_error(ctx, "GID[%d] is in use", index);
        return -EBUSY;
    }

    memcpy(port->gids[index], gid, ROCE_GID_LEN);

    return 0;
}

int roce_del_gid(roce_ctx *ctx, uint8_t _port, uint8_t index)
{
    roce_port *port;

    /* port index begins from 1 */
    if (!_port || (_port > ROCE_MAX_PORTS)) {
        roce_log_error(ctx, "invalid port, available port[1, %d]", ROCE_MAX_PORTS);
        return -EINVAL;
    }

    port = &ctx->ports[_port - 1];
    if (roce_empty_gid(port->gids[index])) {
        roce_log_error(ctx, "GID[%d] not exist", index);
        return -EINVAL;
    }

    memset(port->gids[index], 0x00, ROCE_GID_LEN);

    return 0;
}

int roce_get_gid(roce_ctx *ctx, uint8_t _port, uint8_t index, uint8_t *gid)
{
    roce_port *port;

    /* port index begins from 1 */
    if (!_port || (_port > ROCE_MAX_PORTS)) {
        roce_log_error(ctx, "invalid port, available port[1, %d]", ROCE_MAX_PORTS);
        return -EINVAL;
    }

    port = &ctx->ports[_port - 1];
    if (roce_empty_gid(port->gids[index])) {
        roce_log_warn(ctx, "GID[%d] is in not use", index);
        return -EBUSY;
    }

    memcpy(gid, port->gids[index], ROCE_GID_LEN);

    return 0;
}
