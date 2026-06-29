/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "spec/udp-spec.h"
#include "roce/roce.h"

static void test_udp_hdr_sport(void)
{
    udp_hdr hdr = {0};
    uint16_t sport = ROCE_V2_UDP_DPORT;
    udp_hdr_set_sport(&hdr, sport);
    CU_ASSERT_EQUAL(udp_hdr_get_sport(&hdr), sport);
    sport = 0;
    udp_hdr_set_sport(&hdr, sport);
    CU_ASSERT_EQUAL(udp_hdr_get_sport(&hdr), sport);
}

static void test_udp_hdr_dport(void)
{
    udp_hdr hdr = {0};
    uint16_t dport = ROCE_V2_UDP_DPORT;
    udp_hdr_set_dport(&hdr, dport);
    CU_ASSERT_EQUAL(udp_hdr_get_dport(&hdr), dport);
    dport = 0;
    udp_hdr_set_dport(&hdr, dport);
    CU_ASSERT_EQUAL(udp_hdr_get_dport(&hdr), dport);
}

static void test_udp_hdr_length(void)
{
    udp_hdr hdr = {0};
    uint16_t length = 1500;
    udp_hdr_set_length(&hdr, length);
    CU_ASSERT_EQUAL(udp_hdr_get_length(&hdr), length);
    length = 0;
    udp_hdr_set_length(&hdr, length);
    CU_ASSERT_EQUAL(udp_hdr_get_length(&hdr), length);
}

static void test_udp_hdr_checksum(void)
{
    udp_hdr hdr = {0};
    uint16_t checksum = 0xabcd;
    udp_hdr_set_checksum(&hdr, checksum);
    CU_ASSERT_EQUAL(udp_hdr_get_checksum(&hdr), checksum);
    checksum = 0;
    udp_hdr_set_checksum(&hdr, checksum);
    CU_ASSERT_EQUAL(udp_hdr_get_checksum(&hdr), checksum);
}

static void test_udp_hdr_size(void)
{
    udp_hdr hdr = {0};
    CU_ASSERT_EQUAL(sizeof(hdr), 8);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS) {
        return CU_get_error();
    }

    CU_pSuite suite = CU_add_suite("UDP_Spec_Test", NULL, NULL);
    if (!suite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite, test_udp_hdr_size);
    CU_ADD_TEST(suite, test_udp_hdr_sport);
    CU_ADD_TEST(suite, test_udp_hdr_dport);
    CU_ADD_TEST(suite, test_udp_hdr_length);
    CU_ADD_TEST(suite, test_udp_hdr_checksum);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return failures;
}
