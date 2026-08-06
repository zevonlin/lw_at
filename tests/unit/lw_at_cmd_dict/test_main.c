/**
 * @file test_main.c
 * @brief lw_at_cmd_dict 模块白盒单元测试
 *
 * @details
 * 仅链接 lw_at_cmd_dict.c，覆盖注册校验、跨表重名、重复挂链与查找。
 * 每步打印操作与观测结果，供人工核对。详见同目录 TEST.md。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-07-30
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes    version
 * 2026-07-30 linzhiwei 首次发布 v1.0.0
 */
#include <stdio.h>
#include <string.h>

#include "lw_at_cmd_dict.h"

static uint32_t check_total;
static uint32_t check_fail;

/**
 * @brief 断言并打印
 * @param ok   非 0 通过
 * @param expr 说明
 * @param line 行号
 */
static void test_check(int ok, const char *expr, int line)
{
    check_total++;
    if (ok != 0) {
        (void)printf("[PASS] %s\n", expr);
    } else {
        check_fail++;
        (void)printf("[FAIL] line %d: %s\n", line, expr);
    }
}

#define TEST_CHECK(e) test_check((e) ? 1 : 0, #e, __LINE__)

/**
 * @brief 占位 handler，仅满足类型
 * @param ctx 未使用
 * @return LW_AT_OK
 */
static at_rc_t stub_exe(void *ctx)
{
    (void)ctx;
    return LW_AT_OK;
}

/**
 * @brief D01：reset 后为空
 */
static void test_d01_reset_empty(void)
{
    (void)printf("\n==== D01 reset 后查找为空 ====\n");
    lw_at_cmd_dict_reset();
    (void)printf("<< find(+X)=%p find(\"\")=%p\n",
                 (const void *)lw_at_cmd_dict_find("+X"),
                 (const void *)lw_at_cmd_dict_find(""));
    TEST_CHECK(lw_at_cmd_dict_find("+X") == NULL);
    TEST_CHECK(lw_at_cmd_dict_find("") == NULL);
}

/**
 * @brief D02：register(NULL)
 */
static void test_d02_register_null(void)
{
    lw_at_err_t err;

    (void)printf("\n==== D02 register(NULL) ====\n");
    lw_at_cmd_dict_reset();
    err = lw_at_cmd_dict_register(NULL);
    (void)printf("<< err=%d (expect PARAM=%d)\n", (int)err, (int)LW_AT_ERR_PARAM);
    TEST_CHECK(err == LW_AT_ERR_PARAM);
}

/**
 * @brief D03：cmds/num 非法
 */
static void test_d03_bad_cmds_num(void)
{
    static const lw_at_cmd_t one[] = {{"+A", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t t1;
    lw_at_cmd_table_t t2;
    lw_at_err_t e1;
    lw_at_err_t e2;

    (void)printf("\n==== D03 cmds=NULL 或 num=0 ====\n");
    lw_at_cmd_dict_reset();
    t1.cmds = NULL;
    t1.num = 1U;
    t1.next = NULL;
    t2.cmds = one;
    t2.num = 0U;
    t2.next = NULL;
    e1 = lw_at_cmd_dict_register(&t1);
    e2 = lw_at_cmd_dict_register(&t2);
    (void)printf("<< null_cmds=%d num0=%d\n", (int)e1, (int)e2);
    TEST_CHECK(e1 == LW_AT_ERR_PARAM);
    TEST_CHECK(e2 == LW_AT_ERR_PARAM);
}

/**
 * @brief D04：name=NULL
 */
static void test_d04_name_null(void)
{
    static const lw_at_cmd_t bad[] = {{NULL, NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t table;
    lw_at_err_t err;

    (void)printf("\n==== D04 条目 name=NULL ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = bad;
    table.num = 1U;
    table.next = NULL;
    err = lw_at_cmd_dict_register(&table);
    (void)printf("<< err=%d\n", (int)err);
    TEST_CHECK(err == LW_AT_ERR_PARAM);
}

/**
 * @brief D05：表内重名
 */
static void test_d05_dup_in_table(void)
{
    static const lw_at_cmd_t dup[] = {
        {"+ECHO", NULL, NULL, NULL, stub_exe},
        {"+ECHO", NULL, NULL, NULL, stub_exe},
    };
    lw_at_cmd_table_t table;
    lw_at_err_t err;

    (void)printf("\n==== D05 表内重名 ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = dup;
    table.num = 2U;
    table.next = NULL;
    err = lw_at_cmd_dict_register(&table);
    (void)printf("<< err=%d\n", (int)err);
    TEST_CHECK(err == LW_AT_ERR_PARAM);
}

/**
 * @brief D06：正常注册查找
 */
static void test_d06_register_find(void)
{
    static const lw_at_cmd_t cmds[] = {
        {"+ECHO", NULL, NULL, NULL, stub_exe},
        {"+UART", NULL, NULL, NULL, stub_exe},
    };
    lw_at_cmd_table_t table;
    const lw_at_cmd_t *hit;
    lw_at_err_t err;

    (void)printf("\n==== D06 正常注册并可查找 ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = cmds;
    table.num = 2U;
    table.next = NULL;
    err = lw_at_cmd_dict_register(&table);
    hit = lw_at_cmd_dict_find("+ECHO");
    (void)printf("<< register=%d find(+ECHO)=%p expect=%p\n", (int)err,
                 (const void *)hit, (const void *)&cmds[0]);
    TEST_CHECK(err == LW_AT_ERR_OK);
    TEST_CHECK(hit == &cmds[0]);
    TEST_CHECK(lw_at_cmd_dict_find("+UART") == &cmds[1]);
}

/**
 * @brief D07：大小写敏感
 */
static void test_d07_case_sensitive(void)
{
    static const lw_at_cmd_t cmds[] = {{"+ECHO", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t table;

    (void)printf("\n==== D07 查找大小写敏感 ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = cmds;
    table.num = 1U;
    table.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&table) == LW_AT_ERR_OK);
    (void)printf("<< find(+echo)=%p\n",
                 (const void *)lw_at_cmd_dict_find("+echo"));
    TEST_CHECK(lw_at_cmd_dict_find("+echo") == NULL);
    TEST_CHECK(lw_at_cmd_dict_find("+ECHO") == &cmds[0]);
}

/**
 * @brief D08：未知名
 */
static void test_d08_unknown(void)
{
    static const lw_at_cmd_t cmds[] = {{"+ECHO", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t table;

    (void)printf("\n==== D08 未注册名 ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = cmds;
    table.num = 1U;
    table.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&table) == LW_AT_ERR_OK);
    TEST_CHECK(lw_at_cmd_dict_find("+NOPE") == NULL);
    (void)printf("<< find(+NOPE)=NULL\n");
}

/**
 * @brief D09：find(NULL)
 */
static void test_d09_find_null(void)
{
    (void)printf("\n==== D09 find(NULL) ====\n");
    lw_at_cmd_dict_reset();
    TEST_CHECK(lw_at_cmd_dict_find(NULL) == NULL);
    (void)printf("<< find(NULL)=NULL\n");
}

/**
 * @brief D10：裸 AT 空名
 */
static void test_d10_bare_at(void)
{
    static const lw_at_cmd_t cmds[] = {{"", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t table;

    (void)printf("\n==== D10 裸 AT 空名 \"\" ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = cmds;
    table.num = 1U;
    table.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&table) == LW_AT_ERR_OK);
    TEST_CHECK(lw_at_cmd_dict_find("") == &cmds[0]);
    (void)printf("<< find(\"\") hit\n");
}

/**
 * @brief D11：两张表
 */
static void test_d11_two_tables(void)
{
    static const lw_at_cmd_t a[] = {{"+A", NULL, NULL, NULL, stub_exe}};
    static const lw_at_cmd_t b[] = {{"+B", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t ta;
    lw_at_cmd_table_t tb;

    (void)printf("\n==== D11 两张表先后注册 ====\n");
    lw_at_cmd_dict_reset();
    ta.cmds = a;
    ta.num = 1U;
    ta.next = NULL;
    tb.cmds = b;
    tb.num = 1U;
    tb.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&ta) == LW_AT_ERR_OK);
    TEST_CHECK(lw_at_cmd_dict_register(&tb) == LW_AT_ERR_OK);
    TEST_CHECK(lw_at_cmd_dict_find("+A") == &a[0]);
    TEST_CHECK(lw_at_cmd_dict_find("+B") == &b[0]);
    (void)printf("<< find(+A)/find(+B) both hit\n");
}

/**
 * @brief D12：跨表重名
 */
static void test_d12_cross_dup(void)
{
    static const lw_at_cmd_t a[] = {{"+ECHO", NULL, NULL, NULL, stub_exe}};
    static const lw_at_cmd_t b[] = {{"+ECHO", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t ta;
    lw_at_cmd_table_t tb;
    lw_at_err_t err;

    (void)printf("\n==== D12 跨表重名 ====\n");
    lw_at_cmd_dict_reset();
    ta.cmds = a;
    ta.num = 1U;
    ta.next = NULL;
    tb.cmds = b;
    tb.num = 1U;
    tb.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&ta) == LW_AT_ERR_OK);
    err = lw_at_cmd_dict_register(&tb);
    (void)printf("<< second=%d first_still=%p\n", (int)err,
                 (const void *)lw_at_cmd_dict_find("+ECHO"));
    TEST_CHECK(err == LW_AT_ERR_PARAM);
    TEST_CHECK(lw_at_cmd_dict_find("+ECHO") == &a[0]);
}

/**
 * @brief D13：重复挂链
 */
static void test_d13_re_register(void)
{
    static const lw_at_cmd_t cmds[] = {{"+X", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t table;
    lw_at_err_t err;

    (void)printf("\n==== D13 同一节点重复注册 ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = cmds;
    table.num = 1U;
    table.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&table) == LW_AT_ERR_OK);
    err = lw_at_cmd_dict_register(&table);
    (void)printf("<< second=%d (expect STATE=%d)\n", (int)err,
                 (int)LW_AT_ERR_STATE);
    TEST_CHECK(err == LW_AT_ERR_STATE);
}

/**
 * @brief D14：头插共存
 */
static void test_d14_head_insert(void)
{
    static const lw_at_cmd_t first[] = {{"+F", NULL, NULL, NULL, stub_exe}};
    static const lw_at_cmd_t second[] = {{"+S", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t t1;
    lw_at_cmd_table_t t2;

    (void)printf("\n==== D14 头插后两表共存 ====\n");
    lw_at_cmd_dict_reset();
    t1.cmds = first;
    t1.num = 1U;
    t1.next = NULL;
    t2.cmds = second;
    t2.num = 1U;
    t2.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&t1) == LW_AT_ERR_OK);
    TEST_CHECK(lw_at_cmd_dict_register(&t2) == LW_AT_ERR_OK);
    /* 头插后 t2 在前，精确查找仍应都命中 */
    TEST_CHECK(lw_at_cmd_dict_find("+F") == &first[0]);
    TEST_CHECK(lw_at_cmd_dict_find("+S") == &second[0]);
    (void)printf("<< both findable after head-insert\n");
}

/**
 * @brief D15：reset 清空
 */
static void test_d15_reset_clears(void)
{
    static const lw_at_cmd_t cmds[] = {{"+Z", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t table;

    (void)printf("\n==== D15 reset 清空链表 ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = cmds;
    table.num = 1U;
    table.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&table) == LW_AT_ERR_OK);
    lw_at_cmd_dict_reset();
    TEST_CHECK(lw_at_cmd_dict_find("+Z") == NULL);
    (void)printf("<< after reset find(+Z)=NULL\n");
}

/**
 * @brief D16：reset 后再注册
 */
static void test_d16_reregister_after_reset(void)
{
    static const lw_at_cmd_t cmds[] = {{"+R", NULL, NULL, NULL, stub_exe}};
    lw_at_cmd_table_t table;

    (void)printf("\n==== D16 reset 后再注册 ====\n");
    lw_at_cmd_dict_reset();
    table.cmds = cmds;
    table.num = 1U;
    table.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&table) == LW_AT_ERR_OK);
    lw_at_cmd_dict_reset();
    table.next = NULL;
    TEST_CHECK(lw_at_cmd_dict_register(&table) == LW_AT_ERR_OK);
    TEST_CHECK(lw_at_cmd_dict_find("+R") == &cmds[0]);
    (void)printf("<< re-register after reset OK\n");
}

int main(void)
{
    (void)printf("lw_at_cmd_dict unit test\n");
    (void)printf("see TEST.md for items / reasons / expected results\n");

    test_d01_reset_empty();
    test_d02_register_null();
    test_d03_bad_cmds_num();
    test_d04_name_null();
    test_d05_dup_in_table();
    test_d06_register_find();
    test_d07_case_sensitive();
    test_d08_unknown();
    test_d09_find_null();
    test_d10_bare_at();
    test_d11_two_tables();
    test_d12_cross_dup();
    test_d13_re_register();
    test_d14_head_insert();
    test_d15_reset_clears();
    test_d16_reregister_after_reset();

    (void)printf("\n==== 汇总 ====\n");
    (void)printf("checks: %u, failed: %u\n", (unsigned)check_total,
                 (unsigned)check_fail);
    return (check_fail == 0U) ? 0 : 1;
}
