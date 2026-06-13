// Host-side unit tests for the WS wire protocol (main/proto.c): the inbound JSON
// field extractors and the outbound message formatters. proto.c is dependency-
// free, so this links it directly — no ESP32, no mocks.
//
// Build & run:  make -C test/host run
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "proto.h"

static int g_fail;
#define CHECK(cond, msg)                                                                       \
    do {                                                                                       \
        if (cond) {                                                                            \
            printf("  ok   - %s\n", (msg));                                                    \
        } else {                                                                               \
            printf("  FAIL - %s\n", (msg));                                                    \
            g_fail++;                                                                          \
        }                                                                                      \
    } while (0)

// --- inbound: proto_find_str / proto_find_num ---

static void test_find_str(void)
{
    printf("proto: find a string field\n");
    char out[16] = {0};
    CHECK(proto_find_str("{\"t\":\"hello\"}", "t", out, sizeof out), "key present");
    CHECK(strcmp(out, "hello") == 0, "value is \"hello\"");
    CHECK(!proto_find_str("{\"t\":\"hello\"}", "missing", out, sizeof out),
          "absent key -> false");
    CHECK(!proto_find_str("{\"dir\":-1}", "dir", out, sizeof out),
          "numeric value -> not a string");
}

static void test_find_str_truncation(void)
{
    printf("proto: string extraction is bounded by cap\n");
    char out[4] = {0};
    CHECK(proto_find_str("{\"ev\":\"abcdef\"}", "ev", out, sizeof out), "key present");
    CHECK(strcmp(out, "abc") == 0, "value truncated to cap-1 and NUL-terminated");
}

static void test_find_num(void)
{
    printf("proto: find a numeric field\n");
    double v = 0;
    CHECK(proto_find_num("{\"t\":\"nav\",\"dir\":-1}", "dir", &v), "key present");
    CHECK(v == -1.0, "value is -1");
    CHECK(proto_find_num("{\"t\":\"tilt\",\"g\":12.5}", "g", &v), "float key present");
    CHECK(fabs(v - 12.5) < 1e-9, "value is 12.5");
    CHECK(!proto_find_num("{\"t\":\"nav\"}", "dir", &v), "absent key -> false");
    CHECK(!proto_find_num("{\"t\":\"nav\"}", "t", &v), "string value -> not a number");
}

// --- outbound: proto_fmt_* (locks the exact wire format) ---

static void test_formatters(void)
{
    printf("proto: outbound messages match the documented wire format\n");
    char b[96];

    proto_fmt_welcome(b, sizeof b, 1);
    CHECK(strcmp(b, "{\"v\":1,\"t\":\"welcome\",\"player\":1}") == 0, "welcome");

    proto_fmt_system_info(b, sizeof b, "1.0-abc");
    CHECK(strcmp(b, "{\"v\":1,\"t\":\"system_info\",\"version\":\"1.0-abc\"}") == 0, "system_info");

    proto_fmt_active(b, sizeof b, "pong", 1);
    CHECK(strcmp(b, "{\"v\":1,\"t\":\"active\",\"game\":\"pong\",\"players\":1}") == 0, "active");

    proto_fmt_waiting(b, sizeof b, 2, 1);
    CHECK(strcmp(b, "{\"v\":1,\"t\":\"waiting\",\"need\":2,\"have\":1}") == 0, "waiting");

    proto_fmt_over(b, sizeof b, -1, 5);
    CHECK(strcmp(b, "{\"v\":1,\"t\":\"over\",\"winner\":-1,\"score\":5}") == 0, "over");

    proto_fmt_pong(b, sizeof b, 1700000000123.0);
    CHECK(strcmp(b, "{\"v\":1,\"t\":\"pong\",\"ts\":1700000000123}") == 0, "pong echo (no precision loss)");
}

static void test_schema_version(void)
{
    printf("proto: every server message carries the schema version\n");
    char   b[96];
    double v = 0;
    proto_fmt_active(b, sizeof b, "tron", 2);
    CHECK(proto_find_num(b, "v", &v) && v == (double)PROTO_SCHEMA_VERSION,
          "v field equals PROTO_SCHEMA_VERSION");
}

// --- round-trip: format then parse the fields back out ---

static void test_round_trip(void)
{
    printf("proto: a formatted message parses back to its fields\n");
    char   b[96];
    char   t[16] = {0};
    double w = 0, s = 0;
    proto_fmt_over(b, sizeof b, 1, 3);
    CHECK(proto_find_str(b, "t", t, sizeof t) && strcmp(t, "over") == 0, "t == over");
    CHECK(proto_find_num(b, "winner", &w) && w == 1.0, "winner == 1");
    CHECK(proto_find_num(b, "score", &s) && s == 3.0, "score == 3");
}

int main(void)
{
    test_find_str();
    test_find_str_truncation();
    test_find_num();
    test_formatters();
    test_schema_version();
    test_round_trip();

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail,
           g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
