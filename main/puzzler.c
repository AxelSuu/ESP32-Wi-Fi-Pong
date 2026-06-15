#include "puzzler.h"
#include "display.h"
#include "fx.h"
#include "hw_config.h"
#include "esp_random.h"
#include <stdio.h>

#define GRID_W 8
#define GRID_H 16
#define CELL   5
#define PFX    4         // playfield pixel origin
#define PFY    8

// Seven tetrominoes, 4 rotations each, as 4x4 bitmasks (bit = row*4 + col).
static const uint16_t SHAPES[7][4] = {
    { 0x00F0, 0x4444, 0x0F00, 0x2222 },  // I
    { 0x0066, 0x0066, 0x0066, 0x0066 },  // O
    { 0x0072, 0x0262, 0x0270, 0x0232 },  // T
    { 0x0036, 0x0462, 0x0360, 0x0231 },  // S
    { 0x0063, 0x0264, 0x0630, 0x0132 },  // Z
    { 0x0071, 0x0226, 0x0470, 0x0322 },  // J
    { 0x0074, 0x0622, 0x0170, 0x0223 },  // L
};
static const uint8_t SHADE[7] = { 0xF, 0xC, 0xA, 0x8, 0x6, 0xE, 0x9 };

static uint8_t s_board[GRID_H][GRID_W];   // 0 empty, else piece-type + 1
static int     s_type, s_rot, s_px, s_py; // current piece
static int     s_next;
static int     s_bag[7], s_bag_i;
static int     s_score, s_lines, s_level;
static int     s_grav_acc, s_grav_ms;
static bool    s_over;

static int cell_set(int type, int rot, int r, int c)
{
    return (SHAPES[type][rot] >> (r * 4 + c)) & 1;
}

static bool collides(int type, int rot, int px, int py)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            if (!cell_set(type, rot, r, c)) continue;
            int gx = px + c, gy = py + r;
            if (gx < 0 || gx >= GRID_W || gy >= GRID_H) return true;
            if (gy >= 0 && s_board[gy][gx]) return true;
        }
    return false;
}

static void refill_bag(void)
{
    for (int i = 0; i < 7; i++) s_bag[i] = i;
    for (int i = 6; i > 0; i--) {                       // Fisher-Yates
        int j = (int)(esp_random() % (uint32_t)(i + 1));
        int t = s_bag[i]; s_bag[i] = s_bag[j]; s_bag[j] = t;
    }
    s_bag_i = 0;
}

static int next_from_bag(void)
{
    if (s_bag_i >= 7) refill_bag();
    return s_bag[s_bag_i++];
}

static void spawn(void)
{
    s_type = s_next;
    s_next = next_from_bag();
    s_rot  = 0;
    s_px   = (GRID_W - 4) / 2;
    s_py   = 0;
    if (collides(s_type, s_rot, s_px, s_py)) s_over = true;   // top-out
}

static void grav_from_level(void)
{
    s_grav_ms = 600 - s_level * 45;
    if (s_grav_ms < 90) s_grav_ms = 90;
}

static void puzzler_reset(void)
{
    for (int r = 0; r < GRID_H; r++)
        for (int c = 0; c < GRID_W; c++) s_board[r][c] = 0;
    s_score = s_lines = s_level = 0;
    s_grav_acc = 0;
    s_over = false;
    refill_bag();
    s_next = next_from_bag();
    grav_from_level();
    spawn();
}

static void clear_lines(void)
{
    int cleared = 0;
    for (int r = GRID_H - 1; r >= 0; r--) {
        bool full = true;
        for (int c = 0; c < GRID_W; c++) if (!s_board[r][c]) { full = false; break; }
        if (!full) continue;
        cleared++;
        for (int rr = r; rr > 0; rr--)
            for (int c = 0; c < GRID_W; c++) s_board[rr][c] = s_board[rr - 1][c];
        for (int c = 0; c < GRID_W; c++) s_board[0][c] = 0;
        r++;   // re-check the row that shifted down into this slot
    }
    if (cleared > 0) {
        static const int TBL[5] = { 0, 40, 100, 300, 1200 };
        s_score += TBL[cleared] * (s_level + 1);
        s_lines += cleared;
        s_level = s_lines / 10;
        grav_from_level();
        fx_flash();
    }
}

static void lock_piece(void)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (cell_set(s_type, s_rot, r, c)) {
                int gx = s_px + c, gy = s_py + r;
                if (gy >= 0 && gy < GRID_H && gx >= 0 && gx < GRID_W)
                    s_board[gy][gx] = (uint8_t)(s_type + 1);
            }
    clear_lines();
    spawn();
}

static void step_down(void)            // gravity / soft-drop step; locks if blocked
{
    if (!collides(s_type, s_rot, s_px, s_py + 1)) s_py++;
    else lock_piece();
}

static void rotate(void)
{
    int nr = (s_rot + 1) & 3;
    static const int kick[5] = { 0, -1, 1, -2, 2 };    // basic wall kicks
    for (int k = 0; k < 5; k++)
        if (!collides(s_type, nr, s_px + kick[k], s_py)) { s_px += kick[k]; s_rot = nr; return; }
}

static void hard_drop(void)
{
    while (!collides(s_type, s_rot, s_px, s_py + 1)) s_py++;
    lock_piece();
}

static void puzzler_on_input(const input_event_t *ev)
{
    if (ev->player != 0 || s_over) return;
    switch (ev->kind) {
    case INPUT_LEFT:    if (!collides(s_type, s_rot, s_px - 1, s_py)) s_px--; break;
    case INPUT_RIGHT:   if (!collides(s_type, s_rot, s_px + 1, s_py)) s_px++; break;
    case INPUT_DOWN:    if (!collides(s_type, s_rot, s_px, s_py + 1)) { s_py++; s_score++; } break;
    case INPUT_UP:      rotate();    break;   // ROTATE button
    case INPUT_PRIMARY: hard_drop(); break;   // DROP button
    default: break;
    }
}

static void puzzler_tick(uint32_t dt_ms)
{
    if (s_over) return;
    s_grav_acc += (int)dt_ms;
    if (s_grav_acc >= s_grav_ms) { s_grav_acc = 0; step_down(); }
}

static void cell_px(int gx, int gy, int shade)
{
    gfx_rect(PFX + gx * CELL, PFY + gy * CELL, CELL - 1, CELL - 1, shade);
}

static void puzzler_render(void)
{
    gfx_frame(PFX - 1, PFY - 1, GRID_W * CELL + 1, GRID_H * CELL + 1, 0x6);

    for (int r = 0; r < GRID_H; r++)
        for (int c = 0; c < GRID_W; c++)
            if (s_board[r][c]) cell_px(c, r, SHADE[s_board[r][c] - 1]);

    if (!s_over) {
        int gy = s_py;                                   // ghost landing row
        while (!collides(s_type, s_rot, s_px, gy + 1)) gy++;
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                if (cell_set(s_type, s_rot, r, c)) {
                    int y = gy + r;
                    if (y >= 0) gfx_frame(PFX + (s_px + c) * CELL, PFY + y * CELL, CELL - 1, CELL - 1, 0x4);
                }
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                if (cell_set(s_type, s_rot, r, c)) {
                    int y = s_py + r;
                    if (y >= 0) cell_px(s_px + c, y, SHADE[s_type]);
                }
    }

    int hx = PFX + GRID_W * CELL + 6;
    gfx_text(hx, 6, "NEXT", 0xC);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (cell_set(s_next, 0, r, c))
                gfx_rect(hx + c * 4, 16 + r * 4, 3, 3, SHADE[s_next]);
    char buf[16];
    snprintf(buf, sizeof buf, "SC%d", s_score);
    gfx_text(hx, 40, buf, 0xF);
    snprintf(buf, sizeof buf, "LN%d", s_lines);
    gfx_text(hx, 52, buf, 0xA);
    snprintf(buf, sizeof buf, "LV%d", s_level);
    gfx_text(hx, 64, buf, 0xA);
}

static bool puzzler_is_over(void) { return s_over; }
static int  puzzler_winner(void)  { return -1; }
static int  puzzler_score(void)   { return s_score; }

const game_module_t PUZZLER = {
    .id          = "puzzler",
    .title       = "BLOCKS",
    .min_players = 1,
    .scored      = true,
    .controls    = "[{\"w\":\"dpad\",\"dirs\":[\"left\",\"down\",\"right\"]},"
                   "{\"w\":\"btn\",\"label\":\"ROTATE\",\"ev\":\"up\"},"
                   "{\"w\":\"btn\",\"label\":\"DROP\",\"ev\":\"primary\"}]",
    .reset       = puzzler_reset,
    .on_input    = puzzler_on_input,
    .tick        = puzzler_tick,
    .render      = puzzler_render,
    .is_over     = puzzler_is_over,
    .winner      = puzzler_winner,
    .score       = puzzler_score,
};
