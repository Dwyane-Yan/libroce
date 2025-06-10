/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "spec/ip-spec.h"

static void test_ip_hdr_version_ihl(void)
{
    ip_hdr hdr = {0};
    uint8_t version = 4;
    uint8_t ihl = 5;
    ip_hdr_set_version_ihl(&hdr, version, ihl);
    CU_ASSERT_EQUAL(ip_hdr_get_version(&hdr), version);
    CU_ASSERT_EQUAL(ip_hdr_get_ihl(&hdr), ihl);

    version = 0;
    ihl = 0;
    ip_hdr_set_version_ihl(&hdr, version, ihl);
    CU_ASSERT_EQUAL(ip_hdr_get_version(&hdr), version);
    CU_ASSERT_EQUAL(ip_hdr_get_ihl(&hdr), ihl);
}

static void test_ip_hdr_tos(void)
{
    ip_hdr hdr = {0};
    ip_precedence_type precedence = IP_PRECEDENCE_PRIORITY;
    bool d = true;
    bool t = true;
    bool r = true;
    ip_hdr_set_tos(&hdr, precedence, d, t, r);
    CU_ASSERT_EQUAL(ip_hdr_get_precedence(&hdr), precedence);
    CU_ASSERT_EQUAL(ip_hdr_get_tos_d(&hdr), d);
    CU_ASSERT_EQUAL(ip_hdr_get_tos_t(&hdr), t);
    CU_ASSERT_EQUAL(ip_hdr_get_tos_r(&hdr), r);

    precedence = IP_PRECEDENCE_NETWORK_CONTROL;
    d = false;
    t = false;
    r = false;
    ip_hdr_set_tos(&hdr, precedence, d, t, r);
    CU_ASSERT_EQUAL(ip_hdr_get_precedence(&hdr), precedence);
    CU_ASSERT_EQUAL(ip_hdr_get_tos_d(&hdr), d);
    CU_ASSERT_EQUAL(ip_hdr_get_tos_t(&hdr), t);
    CU_ASSERT_EQUAL(ip_hdr_get_tos_r(&hdr), r);
}

static void test_ip_hdr_length(void)
{
    ip_hdr hdr = {0};
    uint16_t length = 1234;
    ip_hdr_set_length(&hdr, length);
    CU_ASSERT_EQUAL(ip_hdr_get_length(&hdr), length);
}

static void test_ip_hdr_id(void)
{
    ip_hdr hdr = {0};
    uint16_t id = 0x1234;
    ip_hdr_set_id(&hdr, id);
    CU_ASSERT_EQUAL(ip_hdr_get_id(&hdr), id);
}

static void test_ip_hdr_frag_off(void)
{
    ip_hdr hdr = {0};
    bool df = true;
    bool mf = true;
    uint16_t offset = 0x1234;
    id_hdr_set_frag_off(&hdr, df, mf, offset);
    CU_ASSERT_EQUAL(ip_hdr_get_frag_df(&hdr), df);
    CU_ASSERT_EQUAL(ip_hdr_get_frag_mf(&hdr), mf);
    CU_ASSERT_EQUAL(ip_hdr_get_offset(&hdr), offset);
}

static void test_ip_hdr_ttl(void)
{
    ip_hdr hdr = {0};
    uint8_t ttl = 0x12;
    ip_hdr_set_ttl(&hdr, ttl);
    CU_ASSERT_EQUAL(ip_hdr_get_ttl(&hdr), ttl);
    ttl = 0;
    ip_hdr_set_ttl(&hdr, ttl);
    CU_ASSERT_EQUAL(ip_hdr_get_ttl(&hdr), ttl);
}

static void test_ip_hdr_protocol(void)
{
    ip_hdr hdr = {0};
    uint8_t protocol = 0x12;
    ip_hdr_set_protocol(&hdr, protocol);
    CU_ASSERT_EQUAL(ip_hdr_get_protocol(&hdr), protocol);
    protocol = 0;
    ip_hdr_set_protocol(&hdr, protocol);
    CU_ASSERT_EQUAL(ip_hdr_get_protocol(&hdr), protocol);
}

static void test_ip_hdr_checksum(void)
{
    ip_hdr hdr = {0};
    uint16_t checksum = 0xabcd;
    ip_hdr_set_checksum(&hdr, checksum);
    CU_ASSERT_EQUAL(ip_hdr_get_checksum(&hdr), checksum);
    checksum = 0;
    ip_hdr_set_checksum(&hdr, checksum);
    CU_ASSERT_EQUAL(ip_hdr_get_checksum(&hdr), checksum);
}

static void test_ip_hdr_saddr(void)
{
    ip_hdr hdr = {0};
    uint32_t saddr = 0x12345678;
    ip_hdr_set_saddr(&hdr, saddr);
    CU_ASSERT_EQUAL(ip_hdr_get_saddr(&hdr), saddr);
    saddr = 0;
    ip_hdr_set_saddr(&hdr, saddr);
    CU_ASSERT_EQUAL(ip_hdr_get_saddr(&hdr), saddr);
}

static void test_ip_hdr_daddr(void)
{
    ip_hdr hdr = {0};
    uint32_t daddr = 0x12345678;
    ip_hdr_set_daddr(&hdr, daddr);
    CU_ASSERT_EQUAL(ip_hdr_get_daddr(&hdr), daddr);
    daddr = 0;
    ip_hdr_set_daddr(&hdr, daddr);
    CU_ASSERT_EQUAL(ip_hdr_get_daddr(&hdr), daddr);
}

static void test_ip_hdr_size(void)
{
    ip_hdr hdr = {0};
    CU_ASSERT_EQUAL(sizeof(hdr), 20);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS) {
        return CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("IP_Spec_Test", NULL, NULL);
    if (!suite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite, test_ip_hdr_size);
    CU_ADD_TEST(suite, test_ip_hdr_version_ihl);
    CU_ADD_TEST(suite, test_ip_hdr_tos);
    CU_ADD_TEST(suite, test_ip_hdr_length);
    CU_ADD_TEST(suite, test_ip_hdr_id);
    CU_ADD_TEST(suite, test_ip_hdr_frag_off);
    CU_ADD_TEST(suite, test_ip_hdr_ttl);
    CU_ADD_TEST(suite, test_ip_hdr_protocol);
    CU_ADD_TEST(suite, test_ip_hdr_checksum);
    CU_ADD_TEST(suite, test_ip_hdr_saddr);
    CU_ADD_TEST(suite, test_ip_hdr_daddr);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return failures;
}
