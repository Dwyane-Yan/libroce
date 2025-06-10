/*
 * Author: zhenwei pi <zhenwei.pi@linux.dev>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "spec/ib-spec.h"

static void test_bth_opcode(void)
{
    ib_bth bth = {0};
    uint8_t qp_type = IB_OPCODE_UD;
    uint8_t opcode = IB_OPCODE_ACKNOWLEDGE;
    ib_bth_set_opcode(&bth, qp_type, opcode);
    CU_ASSERT_EQUAL(ib_bth_get_qp_type(&bth), qp_type);
    CU_ASSERT_EQUAL(ib_bth_get_opcode(&bth), opcode);
}

static void test_bth_se(void)
{
    ib_bth bth = {0};
    bool se = true;
    ib_bth_set_se(&bth, se);
    CU_ASSERT_EQUAL(ib_bth_get_se(&bth), se);
    se = false;
    ib_bth_set_se(&bth, se);
    CU_ASSERT_EQUAL(ib_bth_get_se(&bth), se);
}

static void test_bth_mig(void)
{
    ib_bth bth = {0};
    bool mig = true;
    ib_bth_set_mig(&bth, mig);
    CU_ASSERT_EQUAL(ib_bth_get_mig(&bth), mig);
    mig = false;
    ib_bth_set_mig(&bth, mig);
    CU_ASSERT_EQUAL(ib_bth_get_mig(&bth), mig);
}

static void test_bth_padcnt(void)
{
    ib_bth bth = {0};
    uint8_t padcnt = 3;
    ib_bth_set_padcnt(&bth, padcnt);
    CU_ASSERT_EQUAL(ib_bth_get_padcnt(&bth), padcnt);
    padcnt = 0;
    ib_bth_set_padcnt(&bth, padcnt);
    CU_ASSERT_EQUAL(ib_bth_get_padcnt(&bth), padcnt);
}

static void test_bth_tver(void)
{
    ib_bth bth = {0};
    uint8_t tver = 3;
    ib_bth_set_tver(&bth, tver);
    CU_ASSERT_EQUAL(ib_bth_get_tver(&bth), tver);
    tver = 0;
    ib_bth_set_tver(&bth, tver);
    CU_ASSERT_EQUAL(ib_bth_get_tver(&bth), tver);
    tver = 15;
    ib_bth_set_tver(&bth, tver);
    CU_ASSERT_EQUAL(ib_bth_get_tver(&bth), tver);
}

static void test_bth_pkey(void)
{
    ib_bth bth = {0};
    uint16_t pkey = 0x1234;
    ib_bth_set_pkey(&bth, pkey);
    CU_ASSERT_EQUAL(ib_bth_get_pkey(&bth), pkey);
    pkey = 0xffff;
    ib_bth_set_pkey(&bth, pkey);
    CU_ASSERT_EQUAL(ib_bth_get_pkey(&bth), pkey);
    pkey = 0;
    ib_bth_set_pkey(&bth, pkey);
    CU_ASSERT_EQUAL(ib_bth_get_pkey(&bth), pkey);
}

static void test_bth_fecn(void)
{
    ib_bth bth = {0};
    bool fecn = true;
    ib_bth_set_fecn(&bth, fecn);
    CU_ASSERT_EQUAL(ib_bth_get_fecn(&bth), fecn);
    fecn = false;
    ib_bth_set_fecn(&bth, fecn);
    CU_ASSERT_EQUAL(ib_bth_get_fecn(&bth), fecn);
}

static void test_bth_becn(void)
{
    ib_bth bth = {0};
    bool becn = true;
    ib_bth_set_becn(&bth, becn);
    CU_ASSERT_EQUAL(ib_bth_get_becn(&bth), becn);
    becn = false;
    ib_bth_set_becn(&bth, becn);
    CU_ASSERT_EQUAL(ib_bth_get_becn(&bth), becn);
}

static void test_bth_resv1(void)
{
    ib_bth bth = {0};
    bth.u32_1 = htobe32(0xf5c5c5c5);
    ib_bth_clear_resv1(&bth);
    CU_ASSERT_EQUAL(bth.u32_1, htobe32(0xc0c5c5c5));
}

static void test_bth_destqp(void)
{
    ib_bth bth = {0};
    uint32_t destqp = 0x123456;
    ib_bth_set_destqp(&bth, destqp);
    CU_ASSERT_EQUAL(ib_bth_get_destqp(&bth), destqp);
    destqp = 0;
    ib_bth_set_destqp(&bth, destqp);
    CU_ASSERT_EQUAL(ib_bth_get_destqp(&bth), destqp);
}

static void test_bth_ack(void)
{
    ib_bth bth = {0};
    bool ack = true;
    ib_bth_set_ack(&bth, ack);
    CU_ASSERT_EQUAL(ib_bth_get_ack(&bth), ack);
    ack = false;
    ib_bth_set_ack(&bth, ack);
    CU_ASSERT_EQUAL(ib_bth_get_ack(&bth), ack);
}

static void test_bth_resv2(void)
{
    ib_bth bth = {0};
    bth.u32_2 = htobe32(0xf5c5c5c5);
    ib_bth_clear_resv2(&bth);
    CU_ASSERT_EQUAL(bth.u32_2, htobe32(0x80c5c5c5));
    bth.u32_2 = htobe32(0x75c5c5c5);
    ib_bth_clear_resv2(&bth);
    CU_ASSERT_EQUAL(bth.u32_2, htobe32(0x00c5c5c5));
}

static void test_bth_psn(void)
{
    ib_bth bth = {0};
    uint32_t psn = 0x123456;
    ib_bth_set_psn(&bth, psn);
    CU_ASSERT_EQUAL(ib_bth_get_psn(&bth), psn);
    psn = 0;
    ib_bth_set_psn(&bth, psn);
    CU_ASSERT_EQUAL(ib_bth_get_psn(&bth), psn);
}

static void test_bth_size(void)
{
    ib_bth bth = {0};
    CU_ASSERT_EQUAL(sizeof(bth), 12);
}

static void test_rdeth_eecnxt(void)
{
    ib_rdeth rdeth = {0};
    uint32_t eecnxt = 0x123456;
    ib_rdeth_set_eecnxt(&rdeth, eecnxt);
    CU_ASSERT_EQUAL(ib_rdeth_get_eecnxt(&rdeth), eecnxt);
    eecnxt = 0;
    ib_rdeth_set_eecnxt(&rdeth, eecnxt);
    CU_ASSERT_EQUAL(ib_rdeth_get_eecnxt(&rdeth), eecnxt);
}

static void test_rdeth_size(void)
{
    ib_rdeth rdeth = {0};
    CU_ASSERT_EQUAL(sizeof(rdeth), 4);
}

static void test_deth_qkey(void)
{
    ib_deth deth = {0};
    uint32_t qkey = 0x12345678;
    ib_deth_set_qkey(&deth, qkey);
    CU_ASSERT_EQUAL(ib_deth_get_qkey(&deth), qkey);
    qkey = 0;
    ib_deth_set_qkey(&deth, qkey);
    CU_ASSERT_EQUAL(ib_deth_get_qkey(&deth), qkey);
}

static void test_deth_srcqp(void)
{
    ib_deth deth = {0};
    uint32_t srcqp = 0x123456;
    ib_deth_set_srcqp(&deth, srcqp);
    CU_ASSERT_EQUAL(ib_deth_get_srcqp(&deth), srcqp);
    srcqp = 0;
    ib_deth_set_srcqp(&deth, srcqp);
    CU_ASSERT_EQUAL(ib_deth_get_srcqp(&deth), srcqp);
}

static void test_deth_size(void)
{
    ib_deth deth = {0};
    CU_ASSERT_EQUAL(sizeof(deth), 8);
}

static void test_reth_va(void)
{
    ib_reth reth = {0};
    uint64_t va = 0x123456789abcdef0;
    ib_reth_set_va(&reth, va);
    CU_ASSERT_EQUAL(ib_reth_get_va(&reth), va);
    va = 0;
    ib_reth_set_va(&reth, va);
    CU_ASSERT_EQUAL(ib_reth_get_va(&reth), va);
}

static void test_reth_rkey(void)
{
    ib_reth reth = {0};
    uint32_t rkey = 0x12345678;
    ib_reth_set_rkey(&reth, rkey);
    CU_ASSERT_EQUAL(ib_reth_get_rkey(&reth), rkey);
    rkey = 0;
    ib_reth_set_rkey(&reth, rkey);
    CU_ASSERT_EQUAL(ib_reth_get_rkey(&reth), rkey);
}

static void test_reth_dmalen(void)
{
    ib_reth reth = {0};
    uint32_t dmalen = 0x12345678;
    ib_reth_set_dmalen(&reth, dmalen);
    CU_ASSERT_EQUAL(ib_reth_get_dmalen(&reth), dmalen);
    dmalen = 0;
    ib_reth_set_dmalen(&reth, dmalen);
    CU_ASSERT_EQUAL(ib_reth_get_dmalen(&reth), dmalen);
}

static void test_reth_size(void)
{
    ib_reth reth = {0};
    CU_ASSERT_EQUAL(sizeof(reth), 16);
}

static void test_atomiceth_va(void)
{
    ib_atomiceth atomiceth = {0};
    uint64_t va = 0x123456789abcdef0;
    ib_atomiceth_set_va(&atomiceth, va);
    CU_ASSERT_EQUAL(ib_atomiceth_get_va(&atomiceth), va);
    va = 0;
    ib_atomiceth_set_va(&atomiceth, va);
    CU_ASSERT_EQUAL(ib_atomiceth_get_va(&atomiceth), va);
}

static void test_atomiceth_rkey(void)
{
    ib_atomiceth atomiceth = {0};
    uint32_t rkey = 0x12345678;
    ib_atomiceth_set_rkey(&atomiceth, rkey);
    CU_ASSERT_EQUAL(ib_atomiceth_get_rkey(&atomiceth), rkey);
    rkey = 0;
    ib_atomiceth_set_rkey(&atomiceth, rkey);
    CU_ASSERT_EQUAL(ib_atomiceth_get_rkey(&atomiceth), rkey);
}

static void test_atomiceth_swapdt(void)
{
    ib_atomiceth atomiceth = {0};
    uint64_t swapdt = 0x123456789abcdef0;
    ib_atomiceth_set_swapdt(&atomiceth, swapdt);
    CU_ASSERT_EQUAL(ib_atomiceth_get_swapdt(&atomiceth), swapdt);
    swapdt = 0;
    ib_atomiceth_set_swapdt(&atomiceth, swapdt);
    CU_ASSERT_EQUAL(ib_atomiceth_get_swapdt(&atomiceth), swapdt);
}

static void test_atomiceth_cmpdt(void)
{
    ib_atomiceth atomiceth = {0};
    uint64_t cmpdt = 0x123456789abcdef0;
    ib_atomiceth_set_cmpdt(&atomiceth, cmpdt);
    CU_ASSERT_EQUAL(ib_atomiceth_get_cmpdt(&atomiceth), cmpdt);
    cmpdt = 0;
    ib_atomiceth_set_cmpdt(&atomiceth, cmpdt);
    CU_ASSERT_EQUAL(ib_atomiceth_get_cmpdt(&atomiceth), cmpdt);
}

static void test_atomiceth_size(void)
{
    ib_atomiceth atomiceth = {0};
    CU_ASSERT_EQUAL(sizeof(atomiceth), 28);
}

static void test_xrceth_xrcsrq(void)
{
    ib_xrceth xrceth = {0};
    uint32_t xrcsrq = 0x123456;
    ib_xrceth_set_xrcsrq(&xrceth, xrcsrq);
    CU_ASSERT_EQUAL(ib_xrceth_get_xrcsrq(&xrceth), xrcsrq);
    xrcsrq = 0;
    ib_xrceth_set_xrcsrq(&xrceth, xrcsrq);
    CU_ASSERT_EQUAL(ib_xrceth_get_xrcsrq(&xrceth), xrcsrq);
}

static void test_xrceth_size(void)
{
    ib_xrceth xrceth = {0};
    CU_ASSERT_EQUAL(sizeof(xrceth), 4);
}

static void test_aeth_syndrome(void)
{
    ib_aeth aeth = {0};
    ib_aeth_syndrome_type type = IB_AETH_SYNDROME_NAK;
    uint8_t val = 0xc;
    ib_aeth_set_syndrome(&aeth, type, val);
    CU_ASSERT_EQUAL(ib_aeth_get_syndrome_type(&aeth), type);
    CU_ASSERT_EQUAL(ib_aeth_get_syndrome_val(&aeth), val);
}

static void test_aeth_msn(void)
{
    ib_aeth aeth = {0};
    uint32_t msn = 0x123456;
    ib_aeth_set_msn(&aeth, msn);
    CU_ASSERT_EQUAL(ib_aeth_get_msn(&aeth), msn);
    msn = 0;
    ib_aeth_set_msn(&aeth, msn);
    CU_ASSERT_EQUAL(ib_aeth_get_msn(&aeth), msn);
}

static void test_aeth_set_all(void)
{
    ib_aeth aeth = {0};
    ib_aeth_syndrome_type type = IB_AETH_SYNDROME_ACK;
    uint8_t val = 0x1f;
    uint32_t msn = 0x123456;
    ib_aeth_set(&aeth, type, val, msn);
    CU_ASSERT_EQUAL(ib_aeth_get_syndrome_type(&aeth), type);
    CU_ASSERT_EQUAL(ib_aeth_get_syndrome_val(&aeth), val);
    CU_ASSERT_EQUAL(ib_aeth_get_msn(&aeth), msn);
}

static void test_aeth_size(void)
{
    ib_aeth aeth = {0};
    CU_ASSERT_EQUAL(sizeof(aeth), 4);
}

static void test_atomicacketh_origmemdt(void)
{
    ib_atomicacketh aaeth = {0};
    uint64_t origmemdt = 0x123456789abcdef0;
    ib_atomicacketh_set_origmemdt(&aaeth, origmemdt);
    CU_ASSERT_EQUAL(ib_atomicacketh_get_origmemdt(&aaeth), origmemdt);
    origmemdt = 0;
    ib_atomicacketh_set_origmemdt(&aaeth, origmemdt);
    CU_ASSERT_EQUAL(ib_atomicacketh_get_origmemdt(&aaeth), origmemdt);
}

static void test_atomicacketh_size(void)
{
    ib_atomicacketh aaeth = {0};
    CU_ASSERT_EQUAL(sizeof(aaeth), 8);
}

static void test_immdt_value(void)
{
    ib_immdt immdt = {0};
    uint32_t value = 0x12345678;
    ib_immdt_set_immdt(&immdt, value);
    CU_ASSERT_EQUAL(ib_immdt_get_immdt(&immdt), value);
    value = 0;
    ib_immdt_set_immdt(&immdt, value);
    CU_ASSERT_EQUAL(ib_immdt_get_immdt(&immdt), value);
}

static void test_immdt_size(void)
{
    ib_immdt immdt = {0};
    CU_ASSERT_EQUAL(sizeof(immdt), 4);
}

static void test_ieth_rkey(void)
{
    ib_ieth ieth = {0};
    uint32_t rkey = 0x12345678;
    ib_ieth_set_rkey(&ieth, rkey);
    CU_ASSERT_EQUAL(ib_ieth_get_rkey(&ieth), rkey);
    rkey = 0;
    ib_ieth_set_rkey(&ieth, rkey);
    CU_ASSERT_EQUAL(ib_ieth_get_rkey(&ieth), rkey);
}

static void test_ieth_size(void)
{
    ib_ieth ieth = {0};
    CU_ASSERT_EQUAL(sizeof(ieth), 4);
}

int main(void)
{
    if (CU_initialize_registry() != CUE_SUCCESS) {
        return CU_get_error();
    }

    CU_pSuite suite_bth = CU_add_suite("BTH_Test", NULL, NULL);
    if (!suite_bth) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_bth, test_bth_size);
    CU_ADD_TEST(suite_bth, test_bth_opcode);
    CU_ADD_TEST(suite_bth, test_bth_se);
    CU_ADD_TEST(suite_bth, test_bth_mig);
    CU_ADD_TEST(suite_bth, test_bth_padcnt);
    CU_ADD_TEST(suite_bth, test_bth_tver);
    CU_ADD_TEST(suite_bth, test_bth_pkey);
    CU_ADD_TEST(suite_bth, test_bth_fecn);
    CU_ADD_TEST(suite_bth, test_bth_becn);
    CU_ADD_TEST(suite_bth, test_bth_resv1);
    CU_ADD_TEST(suite_bth, test_bth_destqp);
    CU_ADD_TEST(suite_bth, test_bth_ack);
    CU_ADD_TEST(suite_bth, test_bth_resv2);
    CU_ADD_TEST(suite_bth, test_bth_psn);

    CU_pSuite suite_rdeth = CU_add_suite("RDETH_Test", NULL, NULL);
    if (!suite_rdeth) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_rdeth, test_rdeth_size);
    CU_ADD_TEST(suite_rdeth, test_rdeth_eecnxt);

    CU_pSuite suite_deth = CU_add_suite("DETH_Test", NULL, NULL);
    if (!suite_deth) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_deth, test_deth_size);
    CU_ADD_TEST(suite_deth, test_deth_qkey);
    CU_ADD_TEST(suite_deth, test_deth_srcqp);

    CU_pSuite suite_reth = CU_add_suite("RETH_Test", NULL, NULL);
    if (!suite_reth) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_reth, test_reth_size);
    CU_ADD_TEST(suite_reth, test_reth_va);
    CU_ADD_TEST(suite_reth, test_reth_rkey);
    CU_ADD_TEST(suite_reth, test_reth_dmalen);

    CU_pSuite suite_atomiceth = CU_add_suite("ATOMICETH_Test", NULL, NULL);
    if (!suite_atomiceth) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_atomiceth, test_atomiceth_size);
    CU_ADD_TEST(suite_atomiceth, test_atomiceth_va);
    CU_ADD_TEST(suite_atomiceth, test_atomiceth_rkey);
    CU_ADD_TEST(suite_atomiceth, test_atomiceth_swapdt);
    CU_ADD_TEST(suite_atomiceth, test_atomiceth_cmpdt);

    CU_pSuite suite_xrceth = CU_add_suite("XRCETH_Test", NULL, NULL);
    if (!suite_xrceth) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_xrceth, test_xrceth_size);
    CU_ADD_TEST(suite_xrceth, test_xrceth_xrcsrq);

    CU_pSuite suite_aeth = CU_add_suite("AETH_Test", NULL, NULL);
    if (!suite_aeth) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_aeth, test_aeth_size);
    CU_ADD_TEST(suite_aeth, test_aeth_syndrome);
    CU_ADD_TEST(suite_aeth, test_aeth_msn);
    CU_ADD_TEST(suite_aeth, test_aeth_set_all);

    CU_pSuite suite_atomicacketh = CU_add_suite("ATOMICACKETH_Test", NULL, NULL);
    if (!suite_atomicacketh) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_atomicacketh, test_atomicacketh_size);
    CU_ADD_TEST(suite_atomicacketh, test_atomicacketh_origmemdt);

    CU_pSuite suite_immdt = CU_add_suite("IMMDT_Test", NULL, NULL);
    if (!suite_immdt) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_immdt, test_immdt_size);
    CU_ADD_TEST(suite_immdt, test_immdt_value);

    CU_pSuite suite_ieth = CU_add_suite("IETH_Test", NULL, NULL);
    if (!suite_ieth) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_ADD_TEST(suite_ieth, test_ieth_size);
    CU_ADD_TEST(suite_ieth, test_ieth_rkey);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    int failures = CU_get_number_of_failures();
    CU_cleanup_registry();
    return failures;
}
