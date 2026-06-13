// Host-side unit tests for the graphics toolkit (main/gfx.c). gfx.c is pure
// framebuffer math with no ESP/SPI deps, so it links directly. Pixels are read
// back with gfx_get_pixel() (raw coords, no origin).
//
// Build & run:  make -C test/host run
#include <stdio.h>
#include "gfx.h"

static int g_fail;
#define CHECK(cond, msg)                                                                       \
    do {                                                                                       \
        if (cond) { printf("  ok   - %s\n", (msg)); }                                          \
        else      { printf("  FAIL - %s\n", (msg)); g_fail++; }                                \
    } while (0)

static void test_pixel_and_clear(void)
{
    printf("gfx: clear + pixel\n");
    gfx_clear(0x0);
    CHECK(gfx_get_pixel(10, 10) == 0x0, "clear(0) blanks the buffer");
    gfx_pixel(5, 5, 0xA);
    CHECK(gfx_get_pixel(5, 5) == 0xA, "pixel writes the given shade");
    CHECK(gfx_get_pixel(6, 5) == 0x0, "neighbour untouched");
    CHECK(gfx_get_pixel(-1, 0) == 0x0 && gfx_get_pixel(999, 0) == 0x0, "out-of-range reads 0");
}

static void test_lines(void)
{
    printf("gfx: hline / vline / line\n");
    gfx_clear(0x0);
    gfx_hline(2, 3, 4, 0xF);
    CHECK(gfx_get_pixel(2, 3) == 0xF && gfx_get_pixel(5, 3) == 0xF, "hline spans [x, x+w)");
    CHECK(gfx_get_pixel(1, 3) == 0x0 && gfx_get_pixel(6, 3) == 0x0, "hline does not overrun");

    gfx_clear(0x0);
    gfx_vline(4, 1, 3, 0xF);
    CHECK(gfx_get_pixel(4, 1) == 0xF && gfx_get_pixel(4, 3) == 0xF, "vline spans [y, y+h)");
    CHECK(gfx_get_pixel(4, 4) == 0x0, "vline does not overrun");

    gfx_clear(0x0);
    gfx_line(0, 0, 3, 3, 0xF);
    CHECK(gfx_get_pixel(0, 0) && gfx_get_pixel(1, 1) && gfx_get_pixel(2, 2) && gfx_get_pixel(3, 3),
          "diagonal line hits each step");
}

static void test_frame(void)
{
    printf("gfx: frame is a hollow rectangle\n");
    gfx_clear(0x0);
    gfx_frame(0, 0, 4, 4, 0xF);
    CHECK(gfx_get_pixel(0, 0) && gfx_get_pixel(3, 0) && gfx_get_pixel(0, 3) && gfx_get_pixel(3, 3),
          "all four corners drawn");
    CHECK(gfx_get_pixel(1, 1) == 0x0 && gfx_get_pixel(2, 2) == 0x0, "interior left empty");
}

static void test_text(void)
{
    printf("gfx: text + width\n");
    CHECK(gfx_text_width("AB", 1) == 12, "width = 6px/char at scale 1");
    CHECK(gfx_text_width("A", 2) == 12, "width scales");
    gfx_clear(0x0);
    gfx_text(0, 0, "I", 0xF);                       // 'I' glyph fills column x=2
    CHECK(gfx_get_pixel(2, 0) == 0xF && gfx_get_pixel(2, 6) == 0xF, "'I' draws its vertical bar");
    CHECK(gfx_get_pixel(0, 0) == 0x0, "left column of 'I' is blank");
    gfx_clear(0x0);
    gfx_text(0, 0, " ", 0xF);
    CHECK(gfx_get_pixel(2, 3) == 0x0, "space draws nothing");
}

static void test_origin_shake(void)
{
    printf("gfx: set_origin shifts subsequent drawing\n");
    gfx_clear(0x0);
    gfx_set_origin(10, 10);
    gfx_pixel(0, 0, 0xF);
    gfx_set_origin(0, 0);
    CHECK(gfx_get_pixel(10, 10) == 0xF, "pixel landed at origin offset");
    CHECK(gfx_get_pixel(0, 0) == 0x0, "nothing at the un-offset position");
}

static void test_circle(void)
{
    printf("gfx: filled circle\n");
    gfx_clear(0x0);
    gfx_circle(20, 20, 3, 0xF);
    CHECK(gfx_get_pixel(20, 20) == 0xF, "centre filled");
    CHECK(gfx_get_pixel(40, 40) == 0x0, "far point empty");
}

int main(void)
{
    test_pixel_and_clear();
    test_lines();
    test_frame();
    test_text();
    test_origin_shake();
    test_circle();

    printf("\n%s (%d failure%s)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail,
           g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
