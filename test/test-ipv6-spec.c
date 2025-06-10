/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "spec/ipv6-spec.h"

static void test_ipv6_hdr_version_tc_fl(void)
{
    ipv6_hdr hdr = {0};
    uint8_t version = 6;
    uint8_t tclass = 128;
    uint32_t flow_label = 0x12345;
    ipv6_hdr_set_version_tc_fl(&hdr, version, tclass, flow_label);
    CU_ASSERT_EQUAL(ipv6_hdr_get_version(&hdr), version);
    CU_ASSERT_EQUAL(ipv6_hdr_get_tclass(&hdr), tclass);
    CU_ASSERT_EQUAL(ipv6_hdr_get_flow_lable(&hdr), flow_label);
    version = 6;
    tclass = 0;
    flow_label = 0;
    ipv6_hdr_set_version_tc_fl(&hdr, version, tclass, flow_label);
    CU_ASSERT_EQUAL(ipv6_hdr_get_version(&hdr), version);
    CU_ASSERT_EQUAL(ipv6_hdr_get_tclass(&hdr), tclass);
    CU_ASSERT_EQUAL(ipv6_hdr_get_flow_lable(&hdr), flow_label);
}

static void test_ipv6_hdr_payload_len(void)
{
    ipv6_hdr hdr = {0};
    uint16_t payload_len = 1234;
    ipv6_hdr_set_payload_len(&hdr, payload_len);
    CU_ASSERT_EQUAL(ipv6_hdr_get_payload_len(&hdr), payload_len);
    payload_len = 0;
    ipv6_hdr_set_payload_len(&hdr, payload_len);
    CU_ASSERT_EQUAL(ipv6_hdr_get_payload_len(&hdr), payload_len);
}

static void test_ipv6_hdr_nexthdr(void)
{
    ipv6_hdr hdr = {0};
    uint8_t nexthdr = 0x12;
    ipv6_hdr_set_nexthdr(&hdr, nexthdr);
    CU_ASSERT_EQUAL(ipv6_hdr_get_nexthdr(&hdr), nexthdr);
    nexthdr = 0;
    ipv6_hdr_set_nexthdr(&hdr, nexthdr);
    CU_ASSERT_EQUAL(ipv6_hdr_get_nexthdr(&hdr), nexthdr);
}

static void test_ipv6_hdr_hop_limit(void)
{
    ipv6_hdr hdr = {0};
    uint8_t hop_limit = 0x12;
    ipv6_hdr_set_hop_limit(&hdr, hop_limit);
    CU_ASSERT_EQUAL(ipv6_hdr_get_hop_limit(&hdr), hop_limit);
    hop_limit = 0;
    ipv6_hdr_set_hop_limit(&hdr, hop_limit);
    CU_ASSERT_EQUAL(ipv6_hdr_get_hop_limit(&hdr), hop_limit);
}

static void test_ipv6_hdr_saddr(void)
{
    ipv6_hdr hdr = {0};
    uint8_t saddr[16] = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    ipv6_hdr_set_saddr(&hdr, saddr);
    CU_ASSERT_EQUAL(memcmp(hdr.saddr, saddr, 16), 0);
}

static void test_ipv6_hdr_daddr(void)
{
    ipv6_hdr hdr = {0};
    uint8_t daddr[16] = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
    ipv6_hdr_set_daddr(&hdr, daddr);
    CU_ASSERT_EQUAL(memcmp(hdr.daddr, daddr, 16), 0);
}

static void test_ipv6_hdr_size(void)
{
    ipv6_hdr hdr = {0};
    CU_ASSERT_EQUAL(sizeof(hdr), 40);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS) {
        return CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("IPv6_Spec_Test", NULL, NULL);
    if (!suite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite, test_ipv6_hdr_size);
    CU_ADD_TEST(suite, test_ipv6_hdr_version_tc_fl);
    CU_ADD_TEST(suite, test_ipv6_hdr_payload_len);
    CU_ADD_TEST(suite, test_ipv6_hdr_nexthdr);
    CU_ADD_TEST(suite, test_ipv6_hdr_hop_limit);
    CU_ADD_TEST(suite, test_ipv6_hdr_saddr);
    CU_ADD_TEST(suite, test_ipv6_hdr_daddr);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return failures;
}
